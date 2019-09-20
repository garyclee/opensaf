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
  // do not call Stop
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
  for (const auto& it : queue_) {
    DataMessage *m = it;
    if (m->header_.mseq_ == mseq && m->header_.mfrag_ == mfrag) {
      return m;
    }
  }
  return nullptr;
}

uint64_t MessageQueue::Erase(Seq16 fseq_from, Seq16 fseq_to) {
  uint64_t msg_len = 0;
  for (auto it = queue_.begin(); it != queue_.end();) {
    DataMessage *m = *it;
    if (fseq_from <= Seq16(m->header_.fseq_) &&
        Seq16(m->header_.fseq_) <= fseq_to) {
      msg_len += m->header_.msg_len_;
      it = queue_.erase(it);
      delete m;
    } else {
      it++;
    }
  }
  return msg_len;
}

DataMessage* MessageQueue::FirstUnsent() {
  for (const auto& it : queue_) {
    DataMessage *m = it;
    if (m->is_sent_ == false) {
      return m;
    }
  }
  return nullptr;
}

void MessageQueue::MarkUnsentFrom(Seq16 fseq) {
  for (const auto& it : queue_) {
    DataMessage *m = it;
    if (Seq16(m->header_.fseq_) >= fseq) m->is_sent_ = false;
  }
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
  // flush all unsent msg in sndqueue_
  FlushData();
}

uint64_t TipcPortId::GetUniqueId(struct tipc_portid id) {
  // this uid is equivalent to the mds adest
  uint64_t uid = ((uint64_t)id.node << 32) | (uint64_t)id.ref;
  return uid;
}

void TipcPortId::FlushData() {
  DataMessage* msg = nullptr;
  do {
    // find the lowest sequence unsent yet
    msg = sndqueue_.FirstUnsent();
    if (msg != nullptr) {
      Send(msg->msg_data_, msg->header_.msg_len_);
      msg->is_sent_ = true;
      m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
          "FlushData[mseq:%u, mfrag:%u, fseq:%u], "
          "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
          id_.node, id_.ref,
          msg->header_.mseq_, msg->header_.mfrag_, msg->header_.fseq_,
          sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
    }
  } while (msg != nullptr);
  sndqueue_.Clear();
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

uint32_t TipcPortId::Queue(const uint8_t* data, uint16_t length,
    bool is_sent) {
  uint32_t rc = NCSCC_RC_SUCCESS;

  DataMessage *msg = new DataMessage;
  msg->header_.Decode(const_cast<uint8_t*>(data));
  msg->Decode(const_cast<uint8_t*>(data));
  msg->msg_data_ = new uint8_t[length];
  msg->is_sent_ = is_sent;
  memcpy(msg->msg_data_, data, length);
  sndqueue_.Queue(msg);
  if (is_sent) {
    ++sndwnd_.send_;
    sndwnd_.nacked_space_ += length;
    m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
        "SndData[mseq:%u, mfrag:%u, fseq:%u, len:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
        id_.node, id_.ref,
        msg->header_.mseq_, msg->header_.mfrag_, msg->header_.fseq_, length,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
  } else {
    ++sndwnd_.send_;
    m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
        "QueData[mseq:%u, mfrag:%u, fseq:%u, len:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
        id_.node, id_.ref,
        msg->header_.mseq_, msg->header_.mfrag_, msg->header_.fseq_, length,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
  }
  return rc;
}

bool TipcPortId::ReceiveCapable(uint16_t sending_len) {
  if (state_ == State::kRcvBuffOverflow) return false;
  if (sndwnd_.nacked_space_ + sending_len < rcv_buf_size_) {
    return true;
  } else {
    if (state_ == State::kTxProb) {
      // Too many msgs are not acked by receiver while in txprob state
      // disable flow control
      state_ = State::kDisabled;
      m_MDS_LOG_ERR("FCTRL: [node:%x, ref:%u] --> Disabled, %" PRIu64
          ", %u, %" PRIu64, id_.node, id_.ref, sndwnd_.nacked_space_,
          sending_len, rcv_buf_size_);
      return true;
    } else if (state_ == State::kEnabled) {
      state_ = State::kRcvBuffOverflow;
      m_MDS_LOG_NOTIFY("FCTRL: [node:%x, ref:%u] --> Overflow, %" PRIu64
          ", %u, %" PRIu64, id_.node, id_.ref, sndwnd_.nacked_space_,
          sending_len, rcv_buf_size_);
    }
    return false;
  }
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
    m_MDS_LOG_NOTIFY("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvData, TxProb[retries:%u, state:%u]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_);
  }
  // update receiver sequence window
  if (rcvwnd_.acked_ < Seq16(fseq) && rcvwnd_.rcv_ + Seq16(1) == Seq16(fseq)) {
    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
        "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "]",
        id_.node, id_.ref,
        mseq, mfrag, fseq,
        rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_);

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
    if (rcvwnd_.rcv_ + Seq16(1) < Seq16(fseq)) {
      if (rcvwnd_.rcv_ == 0 && rcvwnd_.acked_ == 0) {
        // peer does not realize that this portid reset
        m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
            "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
            "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
            "Warning[portid reset]",
            id_.node, id_.ref,
            mseq, mfrag, fseq,
            rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_);

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
            rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_);
      }
    }
    if (Seq16(fseq) <= rcvwnd_.acked_) {
      rc = NCSCC_RC_FAILURE;
      // unexpected retransmission
      m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
          "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
          "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
          "Error[unexpected retransmission]",
          id_.node, id_.ref,
          mseq, mfrag, fseq,
          rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_);
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
    m_MDS_LOG_NOTIFY("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvChkAck, "
        "TxProb[retries:%u, state:%u]",
        id_.node, id_.ref,
        txprob_cnt_, (uint8_t)state_);
  }
  // update sender sequence window
  if (sndwnd_.acked_ < Seq16(fseq) && Seq16(fseq) < sndwnd_.send_) {
    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvChkAck[fseq:%u, chunk:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
        "queue[size:%" PRIu64 "]",
        id_.node, id_.ref,
        fseq, chksize,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_,
        sndqueue_.Size());

    // fast forward the sndwnd_.acked_ sequence to fseq
    sndwnd_.acked_ = fseq;

    // remove a number @chksize messages out of sndqueue_ and decrease
    // the nacked_space_ of sender
    uint64_t acked_bytes = sndqueue_.Erase(Seq16(fseq) - (chksize-1),
        Seq16(fseq));
    sndwnd_.nacked_space_ -= acked_bytes;

    // try to send a few pending msg
    DataMessage* msg = nullptr;
    uint64_t resend_bytes = 0;
    while (resend_bytes < acked_bytes) {
      // find the lowest sequence unsent yet
      msg = sndqueue_.FirstUnsent();
      if (msg == nullptr) {
        break;
      } else {
        if (resend_bytes < acked_bytes) {
          if (Send(msg->msg_data_, msg->header_.msg_len_) == NCSCC_RC_SUCCESS) {
            sndwnd_.nacked_space_ += msg->header_.msg_len_;
            msg->is_sent_ = true;
            resend_bytes += msg->header_.msg_len_;
            m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
                "SndQData[fseq:%u, len:%u], "
                "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
                id_.node, id_.ref,
                msg->header_.fseq_, msg->header_.msg_len_,
                sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
          }
        } else {
          break;
        }
      }
    }
    // no more unsent message, back to kEnabled
    if (msg == nullptr && state_ == State::kRcvBuffOverflow) {
      state_ = State::kEnabled;
      m_MDS_LOG_NOTIFY("FCTRL: [node:%x, ref:%u] Overflow --> Enabled ",
          id_.node, id_.ref);
    }
  } else {
    m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvChkAck[fseq:%u, chunk:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
        "queue[size:%" PRIu64 "], "
        "Error[msg disordered]",
        id_.node, id_.ref,
        fseq, chksize,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_,
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
  if (state_ == State::kRcvBuffOverflow) {
    m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvNack,  ignore[fseq:%u, state:%u]",
        id_.node, id_.ref,
        fseq, (uint8_t)state_);
    sndqueue_.MarkUnsentFrom(Seq16(fseq));
    return;
  }
  if (state_ != State::kRcvBuffOverflow) {
    state_ = State::kRcvBuffOverflow;
    m_MDS_LOG_NOTIFY("FCTRL: [node:%x, ref:%u] --> Overflow ",
        id_.node, id_.ref);
    sndqueue_.MarkUnsentFrom(Seq16(fseq));
  }
  DataMessage* msg = sndqueue_.Find(mseq, mfrag);
  if (msg != nullptr) {
    // Resend the msg found
    if (Send(msg->msg_data_, msg->header_.msg_len_) == NCSCC_RC_SUCCESS) {
      msg->is_sent_ = true;
    }
    m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
        "RsndData[mseq:%u, mfrag:%u, fseq:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
        id_.node, id_.ref,
        mseq, mfrag, fseq,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
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
      sndwnd_.acked_ > Seq16(1) ||
      rcvwnd_.rcv_ > Seq16(1)) return restart_txprob;
  if (state_ == State::kTxProb || state_ == State::kRcvBuffOverflow) {
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
      FlushData();
    }

    m_MDS_LOG_NOTIFY("FCTRL: [node:%x, ref:%u], "
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
    SendChunkAck(rcvwnd_.rcv_.v(), 0, chksize);
    rcvwnd_.acked_ = rcvwnd_.rcv_;
  }
}

}  // end namespace mds
