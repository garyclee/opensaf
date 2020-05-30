/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2019 - All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.
 *
 * Author(s): Ericsson AB
 *
 */

#ifndef LOG_LOGD_LGS_CACHE_H_
#define LOG_LOGD_LGS_CACHE_H_

#include <atomic>
#include <cstring>
#include <string>
#include <sstream>
#include <deque>
#include <memory>

#include "log/logd/lgs.h"
#include "log/logd/lgs_mbcsv_v8.h"
#include "base/macros.h"

// This atomic variable stores the readiness status of file hdle thread.
// It is set to false when the request just arrives at the file handling thread
// and is set to true when the thread is done with the file i/o request.
extern std::atomic<bool> is_filehdl_thread_ready;

//>
// In order to improve resilience of OpenSAF LOG service when underlying
// file system is unresponsive, a queue is introduced to hold the async
// write request up to an configurable time that is around 15 - 30 seconds.
//
// Before passing the async write request to the file handling thread,
// the request have to go through this Cache class (singleton) via
// Cache::Write() method; if any pending requests in queue, the pending
// request have to go first if the file handling thread is ready; if
// the either having pending requests or file handling thread is not
// ready, the coming write request is pushed into the back of the queue.
//
// The Write() method also takes care of 1) doing checkpoint necessary data
// to the standby, 2) streaming the record to additional destinations,
// 3) giving acknowledgment to client and 4) updating the queue.
//
// Besides, the queue will be periodically monitored from the main poll
// via the method Cache::PeriodicCheck(). This periodic check includes
// 1) Check if any pending request is overdue, if so giving confirmation
// to client with SA_AIS_ERR_TRY_AGAIN code, sync with standby, and
// removing the item from the queue, 2) Check if any targeting stream is closed,
// then do the same as above the case - request is overdue, 3) Check if the
// file handling thread is ready, then forwarding the front request to
// that thread, syncing with standby, and ack to client. The check serves only
// one element each time to avoid blocking the main thread.
//
// This feature is only enabled if the queue capacity is set to an non-zero
// value via the attribute `logMaxPendingWriteRequests`; Default is disabled
// to keep service backward compatible.
//
// The resilient time is confiruable via the attribute `logResilienceTimeout`.
//
// This class is used by both active and standby log server.
//<
class Cache {
 public:
  // This is unique entry point for outside world to access Cache methods.
  static Cache* instance() {
    // Thread safe since C++1y
    static Cache cache;
    return &cache;
  }

  ~Cache() {
    pending_write_async_.clear();
  }

  // A part of data that is stored in the queue. The below info is almost
  // provided by the log client that have passed into write async request.
  // We need these these extra information for 1) Streaming
  // 2) Ack to client, 3) Update the log_stream_t if needed.
  // We put this part into a separate structure to simplify the way of
  // intializing the queued data.
  struct WriteAsyncInfo {
    SaInvocationT invocation;
    uint32_t ack_flags;
    uint32_t client_id;
    uint32_t stream_id;
    char* svc_name;
    SaTimeT log_stamp;
    SaLogSeverityT severity;
    MDS_DEST dest;
    char* from_node;

    // Constructors. One for forming cache data from async write event,
    // the other one is for forming cache data on standby instance from
    // push async event.
    WriteAsyncInfo(WriteAsyncParam* param, MDS_DEST dest,
                   const char* node_name);
    explicit WriteAsyncInfo(const CkptPushAsync* data);

    ~WriteAsyncInfo() {
      // these attributes are either nullptr or point to valid memories.
      // nullptr if the data is targettng to alarm/notif streams.
      free(from_node);
      free(svc_name);
    }

    // Show the info of myself in case the request is dropped.
    std::string info() const;
    // Dump values of above data - using for debugging almost.
    void Dump() const;
    // Clone a copy of my data into `CkptPushAsync` for synching
    // with standby.
    void CloneData(CkptPushAsync* output) const;

    // Check if the client whose owns this WriteAsyncInfo data
    // is alive. True if alive, false otherwise.
    bool is_client_alive() const {
      return lgs_client_get_by_id(client_id) != nullptr;
    }

    // Check if the targeting stream of this data is openning or not.
    // True if open, false otherwise.
    bool is_stream_open() const {
      return log_stream_get_by_id(stream_id) != nullptr;
    }

    // Get the stream instance which this data is targetting.
    log_stream_t* stream() const {
      return log_stream_get_by_id(stream_id);
    }
  };

  // This class reprensents the actual data that the queue stores in.
  // In addition of above info, the data also holds the time showing
  // when the data is put into queue, the unique sequence id of the data
  // and the full log record containing right format that complies with
  // tokens given to the targeting stream.
  class Data {
   public:
    // Constructors. One for forming cache data from async write event,
    // the other one is for forming cache data on standby instance from
    // push async event.
    Data(std::shared_ptr<WriteAsyncInfo> info, char* log_record, int size);
    explicit Data(const CkptPushAsync* data);

    ~Data() {
      free(log_record_);
    }

    // Show detailed information about this data. Benefit for logging when
    // the record is dropped.
    std::string info() const { return param_->info(); }
    // Check if the client owning this data is still alive.
    bool is_client_alive() const { return param_->is_client_alive(); }
    // Check if the targeting stream is opening.
    bool is_stream_open() const { return param_->is_stream_open(); }
    // Get the full log record.
    char* record() const { return log_record_; }
    // Check if the data is valid or not. The data is not valid if either
    // the targeting stream is closed or the the time of its staying in the
    // queue is reaching the maximum.
    bool is_valid(std::string* reason) const;
    // Dump the values of data's attributes.
    void Dump() const;
    // Clone values of my attributes to `CkptPushAsync`; and CkptPushAsync
    // value is used for synching with standby.
    void CloneData(CkptPushAsync* data) const;
    // Synch necessary data to standby in case of pushing a write async
    // to the queue. This is only valid to active log service.
    int SyncPushWithStandby() const;
    // Synch necessary data to standby in case of pop a write async
    // from the queue. This is only valid to active log service.
    int SyncPopWithStandby() const;
    // Sync necessary data to standby in case of successfully writing
    // a async write request. ONly valid to active log service.
    int SyncWriteWithStandby() const;
    // Sync necessary data to standby in case of successfully writing
    // a async write request after the file handling thread transits
    // from unreadiness to readiness. In other word, this is a combination
    // b/w SyncPopWithStandby and SyncWriteWithStandby, but we put the case
    // into a separated request to optimize the traffic load.
    int SyncPopAndWriteWithStandby() const;
    // Forward the data to the file handling thread.
    int Write() const;
    // True if it is necessary to send ack to the client,
    // False otherwise.
    bool IsActToClientRequired() const;
    // Send acknowledge with given code to client if the client is still alive
    // and the client is desired to receive the confirmation.
    void AckToClient(SaAisErrorT code) const;
    // Performing streaming this data if needed.
    void Streaming() const;
    // Check if the data has been stayed in the queue so long - reaching
    // the maximum setting time.
    bool is_overdue() const;
    // Store the local time when log server starts to process the write request
    uint64_t queue_at_;
    // The unique id for this data
    uint64_t seq_id_;
    // Write async info which is comming from log client via write async request
    std::shared_ptr<WriteAsyncInfo> param_;
    // The full log record which already complied with stream format
    char* log_record_;
    // The record size
    int size_;
  };

  // Verify if the given capacity `max` is valid. The value is considered
  // valid if the value is either not larger than 1000 or less than current size
  // of the queue. Default capacity is zero (0).
  bool VerifyMaxQueueSize(uint32_t max) const;

  // Verify if the given resilient time is valid or not. The valid value
  // is in range [15 - 30] seconds. Default value is 15s.
  bool VerifyResilienceTime(uint32_t time) const;

  // Return the queue size
  size_t Size() const { return pending_write_async_.size(); }
  // Return the queue's capacity.
  size_t Capacity() const;
  // Get the reference to the front element
  std::shared_ptr<Data> Front() const { return pending_write_async_.front(); }
  // Generate the approriate poll timeout depending on the last poll run,
  // queue size and HA state of log server instance.
  int GeneratePollTimeout(timespec last) const;
  // Forward the data to the file handling thread or put back into
  // the queue depending on the readiness of the thread/and queue status.
  void Write(std::shared_ptr<Data> data);
  // Periodic check the data in queue whether if any of them is invalid
  // and also check if the file handling thread state turns to ready.
  // If the i/o thread is ready, will flush the front element, and will
  // drop the element if the data is no longer valid.
  void PeriodicCheck();
  // Pop the front element from queue. wstatus shows if the going-to-pop
  // request has been successfully written to log file (wstatus = true)
  // or it has been dropped due to the data is invalid (wstatus = false).
  // Before removing the front element, the Pop method also sends ack
  // to client, streaming to destination if necessary.
  void Pop(bool wstatus = false);
  // Push the data back into the queue.
  void Push(std::shared_ptr<Data> data);
  // Encode the queue at cold sync on active side.
  int EncodeColdSync(NCS_UBAID* uba) const;
  // Decode the queue on stanby side.
  int DecodeColdSync(NCS_UBAID* uba, lgsv_ckpt_header_t* header,
                     void* vdata, void** vckpt_rec,
                     size_t ckpt_rec_size) const;

 private:
  // Private constructor to not allow to instantiate this object directly,
  // but accessing this class method via a single instance via a static public
  // method `Cache::instance()`.
  Cache() : pending_write_async_{} {}

  // true if the queue is empty.
  bool Empty() const { return pending_write_async_.empty(); }
  // true if the queue is full - reaching the given capacity.
  bool Full() const { return Size() == Capacity(); }
  // true if the file handling thread is ready.
  bool is_iothread_ready() const { return is_filehdl_thread_ready; }
  // Flush the front element of the queue.
  void FlushFrontElement();
  // Remove the front if its data is no longer valid.
  void PopOverdueData();
  // Return the setting resilience timeout
  uint32_t Timeout() const;
  // Jobs need to be done after writing record to file successfully.
  // 1) streaming to destination 2) sync with standby 3) ack to client
  void PostWrite(std::shared_ptr<Data> data);

 private:
  // Use std::deque<> rather std::queue because we need to access
  // all elements at once during cold sync. Adding to this queue
  // when getting timeout from I/O thread, and removing from this
  // queue either when the data has successfully written to log file
  // or the data is invalid (stream owner is closed or it has stayed
  // in the queue too long). This queue is always kept in sync with standby.
  std::deque<std::shared_ptr<Data> > pending_write_async_;

  DELETE_COPY_AND_MOVE_OPERATORS(Cache);
};

#endif  // LOG_LOGD_LGS_CACHE_H_

