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

#include "mds/mds_tipc_fctrl_portid.h"
#include "base/ncssysf_def.h"

#include "mds/mds_dt.h"
#include "mds/mds_log.h"

namespace mds {

Timer::Timer(Event::Type type) {
  tmr_id_ = nullptr;
  type_ = type;
  is_active_ = false;
}

Timer::~Timer() {
}

void Timer::Start(int64_t period, void (*tmr_exp_func)(void*)) {
  // timer will not start if it's already started
  // period is in centiseconds
  if (is_active_ == false) {
    if (tmr_id_ == nullptr) {
      tmr_id_ = ncs_tmr_alloc(nullptr, 0);
    }
    tmr_id_ = ncs_tmr_start(tmr_id_, period, tmr_exp_func, this,
        nullptr, 0);
    is_active_ = true;
  }
}

void Timer::Stop() {
  if (is_active_ == true) {
    ncs_tmr_stop(tmr_id_);
    is_active_ = false;
  }
}

void MessageQueue::Queue(DataMessage* msg) {
  queue_.push_back(msg);
}

DataMessage* MessageQueue::Find(uint32_t mseq, uint16_t mfrag) {
  for (auto it = queue_.begin(); it != queue_.end(); ++it) {
    DataMessage *m = *it;
    if (m->header_.mseq_ == mseq && m->header_.mfrag_ == mfrag) {
      return m;
    }
  }
  return nullptr;
}

uint64_t MessageQueue::Erase(uint16_t fseq_from, uint16_t fseq_to) {
  uint64_t msg_len = 0;
  for (auto it = queue_.begin(); it != queue_.end();) {
    DataMessage *m = *it;
    if (fseq_from <= m->header_.fseq_ &&
        m->header_.fseq_ <= fseq_to) {
      msg_len += m->header_.msg_len_;
      it = queue_.erase(it);
      delete m;
    } else {
      it++;
    }
  }
  return msg_len;
}

void MessageQueue::Clear() {
  while (queue_.empty() == false) {
    DataMessage* msg = queue_.front();
    queue_.pop_front();
    delete msg;
  }
}

TipcPortId::TipcPortId(struct tipc_portid id, int sock, uint16_t chksize,
    uint64_t sock_buf_size):
  id_(id), bsrsock_(sock), chunk_size_(chksize), rcv_buf_size_(sock_buf_size) {
  state_ = State::kStartup;
}

TipcPortId::~TipcPortId() {
  // Fake a TmrChunkAck event to ack all received messages
  ReceiveTmrChunkAck();
  // clear all msg in sndqueue_
  sndqueue_.Clear();
}

uint64_t TipcPortId::GetUniqueId(struct tipc_portid id) {
  // this uid is equivalent to the mds adest
  uint64_t uid = ((uint64_t)id.node << 32) | (uint64_t)id.ref;
  return uid;
}

uint32_t TipcPortId::Send(uint8_t* data, uint16_t length) {
  struct sockaddr_tipc server_addr;
  ssize_t send_len = 0;
  uint32_t rc = NCSCC_RC_SUCCESS;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.family = AF_TIPC;
  server_addr.addrtype = TIPC_ADDR_ID;
  server_addr.addr.id = id_;
  send_len = sendto(bsrsock_, data, length, 0,
        (struct sockaddr *)&server_addr, sizeof(server_addr));

  if (send_len == length) {
    rc = NCSCC_RC_SUCCESS;
  } else {
    m_MDS_LOG_ERR("sendto() err :%s", strerror(errno));
    rc = NCSCC_RC_FAILURE;
  }
  return rc;
}

uint32_t TipcPortId::Queue(const uint8_t* data, uint16_t length) {
  uint32_t rc = NCSCC_RC_SUCCESS;

  DataMessage *msg = new DataMessage;
  msg->header_.Decode(const_cast<uint8_t*>(data));
  msg->Decode(const_cast<uint8_t*>(data));
  msg->msg_data_ = new uint8_t[length];
  memcpy(msg->msg_data_, data, length);
  sndqueue_.Queue(msg);
  ++sndwnd_.send_;
  sndwnd_.nacked_space_ += length;
  m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
      "SndData[mseq:%u, mfrag:%u, fseq:%u, len:%u], "
      "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
      id_.node, id_.ref,
      msg->header_.mseq_, msg->header_.mfrag_, msg->header_.fseq_, length,
      sndwnd_.acked_, sndwnd_.send_, sndwnd_.nacked_space_);

  return rc;
}

bool TipcPortId::ReceiveCapable(uint16_t sending_len) {
  return true;
}

void TipcPortId::SendChunkAck(uint16_t fseq, uint16_t svc_id,
    uint16_t chksize) {
  uint8_t data[ChunkAck::kChunkAckMsgLength];

  HeaderMessage header(ChunkAck::kChunkAckMsgLength, 0, 0, fseq);
  header.Encode(reinterpret_cast<uint8_t*>(&data));

  ChunkAck sack(svc_id, fseq, chksize);
  sack.Encode(reinterpret_cast<uint8_t*>(&data));
  Send(reinterpret_cast<uint8_t*>(&data), ChunkAck::kChunkAckMsgLength);
  m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
      "SndChkAck[fseq:%u, chunk:%u]",
      id_.node, id_.ref,
      fseq, chksize);
}

uint32_t TipcPortId::ReceiveData(uint32_t mseq, uint16_t mfrag,
    uint16_t fseq, uint16_t svc_id) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  if (state_ == State::kDisabled) {
    m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvData, TxProb[retries:%u, state:%u], "
        "Error[receive fseq:%u in invalid state]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_,
        fseq);
    return rc;
  }
  // update state
  if (state_ == State::kTxProb || state_ == State::kStartup) {
    state_ = State::kEnabled;
    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvData, TxProb[retries:%u, state:%u]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_);
  }
  // update receiver sequence window
  if (rcvwnd_.acked_ < fseq && rcvwnd_.rcv_ + 1 == fseq) {
    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
        "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "]",
        id_.node, id_.ref,
        mseq, mfrag, fseq,
        rcvwnd_.acked_, rcvwnd_.rcv_, rcvwnd_.nacked_space_);

    ++rcvwnd_.rcv_;
    if (rcvwnd_.rcv_ - rcvwnd_.acked_ >= chunk_size_) {
      // send ack for @chunk_size_ msgs starting from fseq
      SendChunkAck(fseq, svc_id, chunk_size_);
      rcvwnd_.acked_ = rcvwnd_.rcv_;
      rc = NCSCC_RC_CONTINUE;
    } else if (fseq == 1 && rcvwnd_.acked_ == 0) {
      // send ack right away for the very first data message
      // to stop txprob timer at sender
      SendChunkAck(fseq, svc_id, 1);
      rcvwnd_.acked_ = rcvwnd_.rcv_;
      rc = NCSCC_RC_CONTINUE;
    }
  } else {
    // todo: update rcvwnd_.nacked_space_.
    // This nacked_space_ will tell the number of bytes that has not been acked
    // to the sender. If this nacked_space_ is growing large, and approaching
    // the socket buffer size, the transmission of sender may be queued.
    // this nacked_space_ can be used to detect if the kChunkAckTimeout or
    // kChunkAckSize are set excessively large.
    // It is not used for now, so ignore it.

    // check for transmission error
    if (rcvwnd_.rcv_ + 1 < fseq) {
      if (rcvwnd_.rcv_ == 0 && rcvwnd_.acked_ == 0) {
        // peer does not realize that this portid reset
        m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
            "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
            "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
            "Warning[portid reset]",
            id_.node, id_.ref,
            mseq, mfrag, fseq,
            rcvwnd_.acked_, rcvwnd_.rcv_, rcvwnd_.nacked_space_);

        rcvwnd_.rcv_ = fseq;
      } else {
        rc = NCSCC_RC_FAILURE;
        // msg loss
        m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
            "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
            "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
            "Error[msg loss]",
            id_.node, id_.ref,
            mseq, mfrag, fseq,
            rcvwnd_.acked_, rcvwnd_.rcv_, rcvwnd_.nacked_space_);
      }
    }
    if (fseq <= rcvwnd_.acked_) {
      rc = NCSCC_RC_FAILURE;
      // unexpected retransmission
      m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
          "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
          "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
          "Error[unexpected retransmission]",
          id_.node, id_.ref,
          mseq, mfrag, fseq,
          rcvwnd_.acked_, rcvwnd_.rcv_, rcvwnd_.nacked_space_);
    }
  }
  return rc;
}

void TipcPortId::ReceiveChunkAck(uint16_t fseq, uint16_t chksize) {
  if (state_ == State::kDisabled) {
    m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvData, TxProb[retries:%u, state:%u], "
        "Error[receive fseq:%u in invalid state]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_,
        fseq);
    return;
  }
  // update state
  if (state_ == State::kTxProb) {
    state_ = State::kEnabled;
    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvChkAck, "
        "TxProb[retries:%u, state:%u]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_);
  }
  // update sender sequence window
  if (sndwnd_.acked_ < fseq && fseq < sndwnd_.send_) {
    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvChkAck[fseq:%u, chunk:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
        "queue[size:%" PRIu64 "]",
        id_.node, id_.ref,
        fseq, chksize,
        sndwnd_.acked_, sndwnd_.send_, sndwnd_.nacked_space_,
        sndqueue_.Size());

    // fast forward the sndwnd_.acked_ sequence to fseq
    sndwnd_.acked_ = fseq;

    // remove a number @chksize messages out of sndqueue_ and decrease
    // the nacked_space_ of sender
    sndwnd_.nacked_space_ -= sndqueue_.Erase(fseq - chksize + 1, fseq);
  } else {
    m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvChkAck[fseq:%u, chunk:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
        "queue[size:%" PRIu64 "], "
        "Error[msg disordered]",
        id_.node, id_.ref,
        fseq, chksize,
        sndwnd_.acked_, sndwnd_.send_, sndwnd_.nacked_space_,
        sndqueue_.Size());
  }
}

void TipcPortId::ReceiveNack(uint32_t mseq, uint16_t mfrag,
    uint16_t fseq) {
  if (state_ == State::kDisabled) {
    m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvNack, TxProb[retries:%u, state:%u], "
        "Error[receive fseq:%u in invalid state]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_,
        fseq);
    return;
  }
  DataMessage* msg = sndqueue_.Find(mseq, mfrag);
  if (msg != nullptr) {
    // Resend the msg found
    Send(msg->msg_data_, msg->header_.msg_len_);
    m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
        "RsndData[mseq:%u, mfrag:%u, fseq:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
        id_.node, id_.ref,
        mseq, mfrag, fseq,
        sndwnd_.acked_, sndwnd_.send_, sndwnd_.nacked_space_);
  } else {
    m_MDS_LOG_ERR("FCTRL: [me] --> [node:%x, ref:%u], "
        "RsndData[mseq:%u, mfrag:%u, fseq:%u], "
        "Error[msg not found]",
        id_.node, id_.ref,
        mseq, mfrag, fseq);
  }
}

bool TipcPortId::ReceiveTmrTxProb(uint8_t max_txprob) {
  bool restart_txprob = false;
  if (state_ == State::kDisabled ||
      sndwnd_.acked_ > 1 ||
      rcvwnd_.rcv_ > 1) return restart_txprob;
  if (state_ == State::kTxProb) {
    txprob_cnt_++;
    if (txprob_cnt_ >= max_txprob) {
      state_ = State::kDisabled;
      restart_txprob = false;
    } else {
      restart_txprob = true;
    }

    // at kDisabled state, clear all message in sndqueue_,
    // receiver is at old mds version
    if (state_ == State::kDisabled) {
      sndqueue_.Clear();
    }

    m_MDS_LOG_DBG("FCTRL: [node:%x, ref:%u], "
        "TxProbExp, TxProb[retries:%u, state:%u]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_);
  }
  return restart_txprob;
}

void TipcPortId::ReceiveTmrChunkAck() {
  if (state_ == State::kDisabled) return;
  uint16_t chksize = rcvwnd_.rcv_ - rcvwnd_.acked_;
  if (chksize > 0) {
    m_MDS_LOG_DBG("FCTRL: [node:%x, ref:%u], "
        "ChkAckExp",
        id_.node, id_.ref);
    // send ack for @chksize msgs starting from rcvwnd_.rcv_
    SendChunkAck(rcvwnd_.rcv_, 0, chksize);
    rcvwnd_.acked_ = rcvwnd_.rcv_;
  }
}

}  // end namespace mds
