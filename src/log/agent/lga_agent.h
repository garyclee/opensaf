/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2017 - All Rights Reserved.
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

#ifndef SRC_LOG_AGENT_LGA_AGENT_H_
#define SRC_LOG_AGENT_LGA_AGENT_H_

#include <atomic>
#include <string>
#include <vector>
#include "mds/mds_papi.h"
#include <saAis.h>
#include "base/macros.h"
#include <saLog.h>
#include "log/common/lgsv_msg.h"
#include "log/common/lgsv_defs.h"
#include "log/agent/lga_common.h"

// Forward declarations
class LogStreamInfo;
class LogClient;

//>
// @LogAgent class - singleton
//
// Below is simple composition, showing relations b/w objects.
// [LogAgent (1)] ------> [LogClient (0..n)] -------> [LogStreamInfo (0..n)]
// Note: (n)/(n..m) - show the number of objects
//
// @LogAgent singleton object is created when starting LOG application.
// When @saLogInitialize() API is called successfully, one @LogClient object
// is created and added to a list @client_list_, and removed when that client
// is finalized. All operation on the lists is protected by mutex.
//
// When getting @NCSMDS_DOWN event from MDS thread, @NoLogServer() method
// will be called by MDS thread to inform the situation to @LogAgent.
// In turn, @LogAgent will broadcast the info to all its @LogClient.
// Each @LogClient will take responsibility to announce that info
// to all its @LogStreamInfo. If getting the @NCSMDS_DOWN event
// all on-going recovery clients will be canceled immediately.
//
// To deal with the case @LogClient or @LogStreamInfo object is deleted
// while that objects are being referred by somewhere else, these objects
// have their one own attribute, called @ref_counter_. The attribute has one
// of these 03 status: being use (>0), being delete(-1) or none of above - idle.
// Once the LOG client wants to delete the object (e.g: Deleting log stream)
// but it is in state "being use", @LogAgent will not allow to delete
// that object, but saying try next time until its state is in "idle" (0).
// Or in other case, if the object is marked "being delete"
// then, any operation on that log stream object will be rejected.
// When in status "being use", reading on that object is not blocked.
//
// The aim of introducing @ref_counter_ is mainly to avoid race conditions
// when LOG application has more than one thread operating on LOG APIs.
//
// @LogAgent also provides methods to outer world to retrieve @LogClient
// or @LogStreamInfo object based on their handles.
//
// Besides, @LogClient have its internal recursive mutex @mutex_,
// used to keep its resource safe from multiple accessing sources
// such as MDS thread or Recovery thread.
//
// When Recovery thread is ongoing, operation on clients which are successfully
// is not blocked. If client is failed to recovery in Recovery thread
// or in client thread, that client will be deleted from the database by
// LOG client thread.
//<
class LogAgent {
 public:
  static LogAgent& instance() { return me_; }

  //<
  // C++ APIs wrapper for corresponding C LOG Agent APIs
  //>
  SaAisErrorT saLogInitialize(SaLogHandleT*, const SaLogCallbacksT*,
                              SaVersionT*);

  SaAisErrorT saLogSelectionObjectGet(SaLogHandleT, SaSelectionObjectT*);

  SaAisErrorT saLogStreamOpen_2(SaLogHandleT, const SaNameT*,
                                const SaLogFileCreateAttributesT_2*,
                                SaLogStreamOpenFlagsT, SaTimeT,
                                SaLogStreamHandleT*);

  SaAisErrorT saLogStreamOpenAsync_2(SaLogHandleT, const SaNameT*,
                                     const SaLogFileCreateAttributesT_2*,
                                     SaLogStreamOpenFlagsT, SaInvocationT);

  SaAisErrorT saLogWriteLog(SaLogStreamHandleT, SaTimeT, const SaLogRecordT*);

  SaAisErrorT saLogWriteLogAsync(SaLogStreamHandleT, SaInvocationT,
                                 SaLogAckFlagsT, const SaLogRecordT*);

  SaAisErrorT saLogLimitGet(SaLogHandleT, SaLogLimitIdT, SaLimitValueT*);

  SaAisErrorT saLogStreamClose(SaLogStreamHandleT);
  SaAisErrorT saLogDispatch(SaLogHandleT, SaDispatchFlagsT);
  SaAisErrorT saLogFinalize(SaLogHandleT);

  // Search for client in @client_list_ based on client @id
  LogClient* SearchClientById(uint32_t);

  // Remove all clients from the list
  void RemoveAllLogClients();

  // Add one client @client to the list of clients @client_list_
  // This method should be called in @saLogInitialize() context
  void AddLogClient(LogClient* client);

  // Do recover all log clients in list @client_list_.
  // This method should only be called within recovery thread
  // when SCs is retarted from headless.
  bool RecoverAllLogClients();

  // Called to inform that no active SC at the moment.
  // Should be called winthin MDS thread after getting
  // @NCSMDS_DOWN/@NCSMDS_NOACTIVE
  // Normally, it is because of headless or failover/switch over
  void NoActiveLogServer();

  // Set @log_server_state flag to true. Should only be called within MDS thread
  // after getting @NCSMDS_DOWN event. This method will in turn informs to all
  // its clients and all log streams belong to each log client.
  void NoLogServer();

  // Has active SC which having @lgs_dest address. Should only be called within
  // MDS thread when getting @NCSMDS_NEW_ACTIVE/@NCSMDS_UP events.
  void HasActiveLogServer(MDS_DEST lgs_dest);

  // Help methods to get references to atomic attributes
  // This lazy/insecure form to let outer world updating
  // the attributes directly if they want to do so.
  std::atomic<MDS_HDL>& atomic_get_mds_hdl();
  std::atomic<MDS_DEST>& atomic_get_lgs_mds_dest();
  std::atomic<bool>& atomic_get_lgs_sync_wait();
  std::atomic<SaClmClusterChangesT>& atomic_get_clm_node_state();

  // Get pointer to @lgs_sync_sel attribute
  NCS_SEL_OBJ* get_lgs_sync_sel() { return &lgs_sync_sel_; }

  // True if log agent is still waiting for active LOG service up
  bool waiting_log_server_up() const;

  // Enter critical section - make sure ref counter is fetched.
  // Introduce these public interface for MDS thread use.
  void EnterCriticalSection();
  void LeaveCriticalSection();

 private:
  // Not allow to create @LogAgent object, except the singleton object @me_.
  LogAgent();
  ~LogAgent() {}

  // True if there is no Active SC, otherwise false
  bool no_active_log_server() const;
  bool no_active_log_server_but_not_headless() const;

  // Search for client in the @client_list_ based on client @handle
  LogClient* SearchClientByHandle(uint32_t handle);

  // Search for @LogStreamInfo object having the @handle using the
  // handle manager (ncshm_take_hdl)
  LogStreamInfo* SearchLogStreamInfoByHandle(SaLogStreamHandleT);

  // Remove one client @client from the list @client_list_
  // This method should be called in @saLogFinalize() context
  void RemoveLogClient(LogClient** client);

  // True if there is no LOG server at all (headless)
  bool is_no_log_server() const;

  // Form finalize Msg and send to MDS
  SaAisErrorT SendFinalizeMsg(uint32_t client_id);

  // Validate open parameter passed by LOG client
  SaAisErrorT ValidateOpenParams(const char*,
                                 const SaLogFileCreateAttributesT_2*,
                                 SaLogStreamOpenFlagsT, SaLogStreamHandleT*,
                                 SaLogHeaderTypeT*);

  // Form @lgsv_stream_open_req_t parameter based on inputs
  void PopulateOpenParams(const char*, uint32_t, SaLogFileCreateAttributesT_2*,
                          SaLogStreamOpenFlagsT, lgsv_stream_open_req_t*);

  // Validate and form Log record
  SaAisErrorT HandleLogRecord(const SaLogRecordT* logRecord,
                              char* logSvcUsrName,
                              lgsv_write_log_async_req_t* write_param,
                              SaTimeT* const logTimeStamp);

  // True if the stream has well-known distinguish name
  static bool is_well_known_stream(const std::string&);
  // Return true if @size is over limitation.
  static bool is_logrecord_size_valid(SaUint32T size);
  // Return true if @ver is valid version
  static bool is_log_version_valid(const SaVersionT* ver);
  // Return true if @flag is valid
  static bool is_dispatch_flag_valid(SaDispatchFlagsT flag);
  // Retrieve current time
  static SaTimeT SetLogTime();

  // Constant values used by @LogAgent only
  enum {
    kSafMinAcceptTime = 10,
    kNanoSecondToLeapTm = 10 * 1000 * 1000,
    // Temporary maximum log file name length. Used to avoid LOG client
    // sends too long (big data) file name to LOG service.
    // The real limit check will be done on LOG service side.
    kMaxLogFileNameLenth = 2048
  };

  // Group all atomic data into one for easily management.
  struct AtomicData {
    // Hold the current state of LOG server.
    std::atomic<LogServerState> log_server_state;

    // True if LOG agent still in waiting for LOG server coming up
    std::atomic<bool> waiting_log_server_up;

    // Hold the MDS handle for @this log agent
    std::atomic<MDS_HDL> mds_hdl;

    // Hold the MDS destination address of LOG server.
    std::atomic<MDS_DEST> lgs_mds_dest;

    // Reflects CLM status of this node (for future use)
    std::atomic<SaClmClusterChangesT> clm_node_state;

    // Constructor with default values
    AtomicData()
        : log_server_state{LogServerState::kHasActiveLogServer},
          waiting_log_server_up{false},
          mds_hdl{0},
          lgs_mds_dest{0},
          clm_node_state{SA_CLM_NODE_JOINED} {}
  };

  // Hold all atomic attributes
  AtomicData atomic_data_;

  // Used to synchronize adding/removing @client to/from @client_list_
  // MUST use this mutex whenever accessing to client from @client_list_
  // This mutex is RECURSIVE.
  pthread_mutex_t mutex_;

  // Protect the short critical section: Search<xx>ByHandle() to
  // FetchAndUpdateObjectState(). Avoid race condition b/w
  // deletion on these object righ after `Search<xx>ByHandle()` is called.
  // Aim to prevent one thread enter `Search<xx>ByHandle()`, not yet
  // fetch and update the @ref_counter_ while the other thread do delete
  // the object. Such case will cause trouble to first thread when fetching
  // and updating @ref_counter_.
  // NOTE: Use native mutex instead of base::Mutex because base::Mutex
  // has coredump in MDS thread when unlock the mutex. Not yet analyze.
  // base::Mutex get_delete_obj_sync_mutex_;
  pthread_mutex_t get_delete_obj_sync_mutex_;

  // Hold list of current log clients
  std::vector<LogClient*> client_list_;

  // LGS LGA sync params
  NCS_SEL_OBJ lgs_sync_sel_;

  // Singleton represents LOG Agent in LOG application process
  static LogAgent me_;

  DELETE_COPY_AND_MOVE_OPERATORS(LogAgent);
};

//------------------------------------------------------------------------------
// LogAgent inline methods
//------------------------------------------------------------------------------
inline void LogAgent::HasActiveLogServer(MDS_DEST lgs_dest) {
  atomic_data_.lgs_mds_dest = lgs_dest;
  atomic_data_.log_server_state = LogServerState::kHasActiveLogServer;
}

inline void LogAgent::NoActiveLogServer() {
  atomic_data_.log_server_state = LogServerState::kNoActiveLogServer;
  // Reset the LOG server destination address
  atomic_data_.lgs_mds_dest = 0;
}

inline bool LogAgent::is_dispatch_flag_valid(SaDispatchFlagsT flag) {
  return ((SA_DISPATCH_ONE == flag) || (SA_DISPATCH_ALL == flag) ||
          (SA_DISPATCH_BLOCKING == flag));
}

inline bool LogAgent::is_well_known_stream(const std::string& dn) {
  if (dn == std::string{SA_LOG_STREAM_ALARM}) return true;
  if (dn == std::string{SA_LOG_STREAM_NOTIFICATION}) return true;
  if (dn == std::string{SA_LOG_STREAM_SYSTEM}) return true;
  return false;
}

inline bool LogAgent::is_logrecord_size_valid(SaUint32T size) {
  return (size > SA_LOG_MAX_RECORD_SIZE);
}

inline bool LogAgent::is_log_version_valid(const SaVersionT* ver) {
  assert(ver != nullptr);
  return ((ver->releaseCode == LOG_RELEASE_CODE) &&
          (ver->majorVersion <= LOG_MAJOR_VERSION) && (ver->majorVersion > 0) &&
          (ver->minorVersion <= LOG_MINOR_VERSION));
}

inline SaTimeT LogAgent::SetLogTime() {
  struct timeval currentTime;
  SaTimeT logTime;
  // Fetch current system time for time stamp value
  gettimeofday(&currentTime, 0);
  logTime = ((static_cast<SaTimeT>(currentTime.tv_sec) * 1000000000ULL) +
             (static_cast<SaTimeT>(currentTime.tv_usec) * 1000ULL));
  return logTime;
}

// This is a poor/insecure/lazy form. Go with this as an trade-off
// with convenience for caller.
inline std::atomic<MDS_HDL>& LogAgent::atomic_get_mds_hdl() {
  return atomic_data_.mds_hdl;
}

inline std::atomic<MDS_DEST>& LogAgent::atomic_get_lgs_mds_dest() {
  return atomic_data_.lgs_mds_dest;
}

inline std::atomic<bool>& LogAgent::atomic_get_lgs_sync_wait() {
  return atomic_data_.waiting_log_server_up;
}

inline std::atomic<SaClmClusterChangesT>&
LogAgent::atomic_get_clm_node_state() {
  return atomic_data_.clm_node_state;
}

inline bool LogAgent::no_active_log_server() const {
  return (atomic_data_.log_server_state != LogServerState::kHasActiveLogServer);
}

inline bool LogAgent::no_active_log_server_but_not_headless() const {
  return (atomic_data_.log_server_state == LogServerState::kNoActiveLogServer);
}

inline bool LogAgent::is_no_log_server() const {
  return (atomic_data_.log_server_state == LogServerState::kNoLogServer);
}

inline bool LogAgent::waiting_log_server_up() const {
  return atomic_data_.waiting_log_server_up.load();
}

inline void LogAgent::EnterCriticalSection() {
  osaf_mutex_lock_ordie(&get_delete_obj_sync_mutex_);
}

inline void LogAgent::LeaveCriticalSection() {
  osaf_mutex_unlock_ordie(&get_delete_obj_sync_mutex_);
}

#endif  // SRC_LOG_AGENT_LGA_AGENT_H_
