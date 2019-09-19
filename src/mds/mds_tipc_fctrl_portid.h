/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2019 The OpenSAF Foundation
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

#ifndef MDS_MDS_TIPC_FCTRL_PORTID_H_
#define MDS_MDS_TIPC_FCTRL_PORTID_H_

#include <linux/tipc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <deque>
#include "mds/mds_tipc_fctrl_msg.h"

namespace mds {

class MessageQueue {
 public:
  void Queue(DataMessage* msg);
  DataMessage* Find(uint32_t mseq, uint16_t mfrag);
  uint64_t Erase(uint16_t fseq_from, uint16_t fseq_to);
  uint64_t Size() const { return queue_.size(); }
  void Clear();
 private:
  std::deque<DataMessage*> queue_;
};

class Timer {
 public:
  tmr_t tmr_id_{nullptr};
  bool is_active_{false};
  Event::Type type_;
  void Start(int64_t period, void (*tmr_exp_func)(void*));
  void Stop();
  explicit Timer(Event::Type type);
  ~Timer();
};

class TipcPortId {
 public:
  enum class State {
    kDisabled,  // no flow control support for this published portid
    kStartup,   // a newly published portid starts at this state
    kTxProb,    // txprob timer runs to confirm the flow control support
    kEnabled,   // flow control support is confirmed
    kRcvBuffOverflow   // the receiver's buffer overflow
  };
  TipcPortId(struct tipc_portid id, int sock, uint16_t chunk_size,
      uint64_t sock_buf_size);
  ~TipcPortId();
  static uint64_t GetUniqueId(struct tipc_portid id);
  int GetSock() const { return bsrsock_; }
  uint16_t GetCurrentSeq() { return sndwnd_.send_; }
  bool ReceiveCapable(uint16_t sending_len);
  void ReceiveChunkAck(uint16_t fseq, uint16_t chunk_size);
  void SendChunkAck(uint16_t fseq, uint16_t svc_id, uint16_t chunk_size);
  uint32_t ReceiveData(uint32_t mseq, uint16_t mfrag,
      uint16_t fseq, uint16_t svc_id);
  void ReceiveNack(uint32_t mseq, uint16_t mfrag, uint16_t fseq);
  bool ReceiveTmrTxProb(uint8_t max_txprob);
  void ReceiveTmrChunkAck();
  uint32_t Send(uint8_t* data, uint16_t length);
  uint32_t Queue(const uint8_t* data, uint16_t length);

  uint16_t svc_cnt_{1};  // number of service subscribed on this portid

  State state_;
  uint8_t txprob_cnt_{0};

 private:
  struct tipc_portid id_;
  int bsrsock_;  // tipc socket to send/receive data per tipc_portid
  uint16_t chunk_size_{5};
  uint64_t rcv_buf_size_{0};  // estimated buffer size at receiver

  struct sndwnd {
    // sender sequence window
    uint16_t acked_{0};  // last sequence has been acked by receiver
    uint16_t send_{1};   // next sequence to be sent
    uint64_t nacked_space_{0};  // total bytes are sent but not acked
  };
  struct sndwnd sndwnd_;

  struct rcvwnd {
    // receiver sequence window
    uint16_t acked_{0};  // last sequence has been acked to sender
    uint16_t rcv_{0};    // last sequence has been received
    uint64_t nacked_space_{0};  // total bytes has not been acked
  };
  struct rcvwnd rcvwnd_;

  MessageQueue sndqueue_;
};

}  // end namespace mds
#endif  // MDS_MDS_TIPC_FCTRL_PORTID_H_
