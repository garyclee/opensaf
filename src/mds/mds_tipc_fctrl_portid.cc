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

extern "C" {
#include "mds/mds_dt_tipc.h"
}

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

DataMessage* MessageQueue::Find(Seq16 fseq) {
  for (const auto& it : queue_) {
    DataMessage *m = it;
    if (Seq16(m->header_.fseq_) == fseq) {
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
  SendIntro();
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

bool TipcPortId::SendUnsentMsg(bool by_chunkAck) {
  if ((by_chunkAck == false) && (tmr_trigger_send_ == false))
    return true;
  // try to send a few pending msg
  DataMessage* msg = nullptr;
  uint16_t send_msg_cnt = 0;
  while (send_msg_cnt < chunk_size_) {
    // find the lowest sequence unsent yet
    msg = sndqueue_.FirstUnsent();
    if (msg == nullptr) {
      // return false only if no more unsent msg
      return false;
    } else {
      if (Send(msg->msg_data_, msg->header_.msg_len_) == NCSCC_RC_SUCCESS) {
        tmr_trigger_send_ = false;  // timer will not trigger send anymore
        send_msg_cnt++;
        msg->is_sent_ = true;
        m_MDS_LOG_NOTIFY("FCTRL: [me] --> [node:%x, ref:%u], "
            "SndQData[fseq:%u, len:%u], "
            "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
            id_.node, id_.ref,
            msg->header_.fseq_, msg->header_.msg_len_,
            sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
      } else {
        if (by_chunkAck && (send_msg_cnt == 0))
          tmr_trigger_send_ = true;  // need timer to trigger send later
        break;
      }
    }
  }
  return true;
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
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.family = AF_TIPC;
  server_addr.addrtype = TIPC_ADDR_ID;
  server_addr.addr.id = id_;
  ssize_t send_len = mds_retry_sendto(bsrsock_, data, length, MSG_DONTWAIT,
        (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (send_len == length)
    return NCSCC_RC_SUCCESS;
  m_MDS_LOG_ERR("FCTRL: TipcPortId::Send() failed, Err[%s]", strerror(errno));
  return NCSCC_RC_FAILURE;
}

uint32_t TipcPortId::Queue(const uint8_t* data, uint16_t length,
    bool is_sent) {
  uint32_t rc = NCSCC_RC_SUCCESS;

  DataMessage *msg = new DataMessage;
  msg->header_.Decode(const_cast<uint8_t*>(data));
  msg->Decode(const_cast<uint8_t*>(data));
  msg->is_sent_ = is_sent;
  msg->msg_data_ = const_cast<uint8_t*>(data);
  sndqueue_.Queue(msg);
  ++sndwnd_.send_;
  sndwnd_.nacked_space_ += length;
  if (is_sent) {
    m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
        "SndData[mseq:%u, mfrag:%u, fseq:%u, len:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
        id_.node, id_.ref,
        msg->header_.mseq_, msg->header_.mfrag_, msg->header_.fseq_, length,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
  } else {
    m_MDS_LOG_NOTIFY("FCTRL: [me] --> [node:%x, ref:%u], "
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
      m_MDS_LOG_ERR("FCTRL: me --> [node:%x, ref:%u], "
          "Warning[Too many nacked in kTxProb]",
          id_.node, id_.ref);
      ChangeState(State::kDisabled);
      return true;
    } else if (state_ == State::kEnabled) {
      ChangeState(State::kRcvBuffOverflow);
    }
    return false;
  }
}

void TipcPortId::SendChunkAck(uint16_t fseq, uint16_t svc_id,
    uint16_t chksize) {
  uint8_t data[ChunkAck::kChunkAckMsgLength] = {0};

  HeaderMessage header(ChunkAck::kChunkAckMsgLength, 0, 0, fseq);
  header.Encode(data);

  ChunkAck sack(svc_id, fseq, chksize);
  sack.Encode(data);
  Send(data, ChunkAck::kChunkAckMsgLength);
  m_MDS_LOG_DBG("FCTRL: [me] --> [node:%x, ref:%u], "
      "SndChkAck[fseq:%u, chunk:%u]",
      id_.node, id_.ref,
      fseq, chksize);
}

void TipcPortId::SendNack(uint16_t fseq, uint16_t svc_id) {
  uint8_t data[Nack::kNackMsgLength] = {0};

  HeaderMessage header(Nack::kNackMsgLength, 0, 0, fseq);
  header.Encode(data);

  Nack nack(svc_id, fseq);
  nack.Encode(data);
  Send(data, Nack::kNackMsgLength);
  m_MDS_LOG_NOTIFY("FCTRL: [me] --> [node:%x, ref:%u], "
      "SndNack[fseq:%u]", id_.node, id_.ref, fseq);
}

void TipcPortId::SendIntro() {
  uint8_t data[Intro::kIntroMsgLength] = {0};

  HeaderMessage header(Intro::kIntroMsgLength, 0, 0, 0);
  header.Encode(data);

  Intro intro;
  intro.Encode(data);
  Send(data, Intro::kIntroMsgLength);
  m_MDS_LOG_NOTIFY("FCTRL: [me] --> [node:%x, ref:%u], "
      "SndIntro", id_.node, id_.ref);
}

uint32_t TipcPortId::ReceiveData(uint32_t mseq, uint16_t mfrag,
    uint16_t fseq, uint16_t svc_id, uint8_t snd_type, bool mcast_enabled) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  if (state_ == State::kDisabled) {
    m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
        "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
        "Warning[Invalid state:%u]",
        id_.node, id_.ref,
        mseq, mfrag, fseq,
        rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_,
        (uint8_t)state_);
    return rc;
  }
  // update state
  if (state_ == State::kTxProb || state_ == State::kStartup) {
    ChangeState(State::kEnabled);
  }
  // if tipc multicast is enabled, receiver does not inspect sequence number
  // for both fragment/unfragment multicast/broadcast message
  if (mcast_enabled) {
    if (mfrag == 0) {
      // this is not fragment, the snd_type field is always present in message
      rcving_mbcast_ = false;
      if (snd_type == MDS_SENDTYPE_BCAST ||
          snd_type == MDS_SENDTYPE_RBCAST) {
        rcving_mbcast_ = true;
      }
    } else {
      // this is fragment, the snd_type is only present in the first fragment
      if ((mfrag & 0x7fff) == 1 &&
          (snd_type == MDS_SENDTYPE_BCAST ||
           snd_type == MDS_SENDTYPE_RBCAST)) {
        rcving_mbcast_ = true;
      }
    }
    if (rcving_mbcast_ == true) {
      m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
          "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
          "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
          "Ignore bcast/mcast ",
          id_.node, id_.ref,
          mseq, mfrag, fseq,
          rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_);
      return rc;
    }
  }

  // update receiver sequence window
  if (rcvwnd_.acked_ < Seq16(fseq) && rcvwnd_.rcv_ + 1 == Seq16(fseq)) {
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
    if (rcvwnd_.rcv_ + 1 < Seq16(fseq)) {
      if (rcvwnd_.rcv_ == 0 && rcvwnd_.acked_ == 0) {
        // peer does not realize that this portid reset
        m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
            "RcvData[mseq:%u, mfrag:%u, fseq:%u], "
            "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
            "[portid reset]",
            id_.node, id_.ref,
            mseq, mfrag, fseq,
            rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_);

        SendChunkAck(fseq, svc_id, 1);
        rcvwnd_.rcv_ = Seq16(fseq);
        rcvwnd_.acked_ = rcvwnd_.rcv_;
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
        // send nack
        SendNack((rcvwnd_.rcv_ + 1).v(), svc_id);
      }
    } else if (Seq16(fseq) <= rcvwnd_.rcv_) {
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
    ChangeState(State::kEnabled);
  }
  // update sender sequence window
  if (sndwnd_.acked_ < Seq16(fseq)) {
    // The fseq_ should always be less then sndwnd_.send_, hence
    // mds should check the sender being capable of sending more
    // message only if D = sndwnd_.send_ - sndwnd_.acked_ < 2^15 - 1 = 32767
    // If a burst of message is sent, D could be > 32767
    // mds in this case should notify the sender try to send again
    // later; which however could leads to a backward compatibility
    // For now mds logs a warning and let the transmission continue
    // (mds could be changed to return try again if it is not a backward
    // compatibility problem to a specific client).
    if (Seq16(fseq) >= sndwnd_.send_) {
      m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
          "RcvChkAck[fseq:%u, chunk:%u], "
          "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
          "queue[size:%" PRIu64 "], "
          "Warning[ack sequence out of window]",
          id_.node, id_.ref,
          fseq, chksize,
          sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_,
          sndqueue_.Size());
    }

    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvChkAck[fseq:%u, chunk:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
        "queue[size:%" PRIu64 "]",
        id_.node, id_.ref,
        fseq, chksize,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_,
        sndqueue_.Size());

    // remove a number @chksize messages out of sndqueue_ and decrease
    // the nacked_space_ of sender
    uint64_t acked_bytes = sndqueue_.Erase(sndwnd_.acked_ + 1, Seq16(fseq));
    // fast forward the sndwnd_.acked_ sequence to fseq
    sndwnd_.acked_ = Seq16(fseq);

    assert(sndwnd_.nacked_space_ >= acked_bytes);
    sndwnd_.nacked_space_ -= acked_bytes;

    bool res = SendUnsentMsg(true);
    // no more unsent message, back to kEnabled
    if (res == false && state_ == State::kRcvBuffOverflow) {
      ChangeState(State::kEnabled);
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

  if (Seq16(fseq) <= sndwnd_.acked_) {
    m_MDS_LOG_NOTIFY("FCTRL: [me] <-- [node:%x, ref:%u], "
        "RcvNack[fseq:%u], "
        "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
        "Warning[Invalid Nack]",
        id_.node, id_.ref, fseq,
        sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
    return;
  }

  if (state_ == State::kRcvBuffOverflow) {
    sndqueue_.MarkUnsentFrom(Seq16(fseq));
    if (Seq16(fseq) - sndwnd_.acked_ > 1) {
      m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
          "RcvNack[fseq:%u], "
          "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
          "queue[size:%" PRIu64 "], "
          "Warning[Ignore Nack]",
          id_.node, id_.ref, fseq,
          sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_,
          sndqueue_.Size());
      return;
    }
  }
  if (state_ != State::kRcvBuffOverflow) {
    ChangeState(State::kRcvBuffOverflow);
    sndqueue_.MarkUnsentFrom(Seq16(fseq));
  }
  DataMessage* msg = sndqueue_.Find(Seq16(fseq));
  if (msg != nullptr) {
    // Resend the msg found
    if (Send(msg->msg_data_, msg->header_.msg_len_) == NCSCC_RC_SUCCESS) {
      msg->is_sent_ = true;
      m_MDS_LOG_NOTIFY("FCTRL: [me] --> [node:%x, ref:%u], "
          "RsndData[mseq:%u, mfrag:%u, fseq:%u], "
          "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "]",
          id_.node, id_.ref,
          mseq, mfrag, fseq,
          sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_);
    }
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
  if (state_ == State::kDisabled) return restart_txprob;
  if (state_ == State::kTxProb || state_ == State::kRcvBuffOverflow) {
    txprob_cnt_++;
    if (txprob_cnt_ >= max_txprob) {
      ChangeState(State::kDisabled);
      restart_txprob = false;
    } else {
      restart_txprob = true;
    }
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

void TipcPortId::ChangeState(State newState) {
  if (state_ == newState) return;

  if (newState == State::kDisabled) {
    // at kDisabled state, clear all message in sndqueue_,
    // receiver is at old mds version
    FlushData();
  }
  // state changes so print all counters
  m_MDS_LOG_NOTIFY("FCTRL: [node:%x, ref:%u], "
      "ChangeState[%u -> %u], "
      "TxProb[retries:%u], "
      "sndwnd[acked:%u, send:%u, nacked:%" PRIu64 "], "
      "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
      "queue[size:%" PRIu64 "]",
      id_.node, id_.ref,
      (uint8_t)state_, (uint8_t)newState,
      txprob_cnt_,
      sndwnd_.acked_.v(), sndwnd_.send_.v(), sndwnd_.nacked_space_,
      rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_,
      sndqueue_.Size());
  state_ = newState;
}

void TipcPortId::ReceiveIntro() {
  m_MDS_LOG_NOTIFY("FCTRL: [me] <-- [node:%x, ref:%u], RcvIntro",
      id_.node, id_.ref);
  if (state_ == State::kStartup || state_ == State::kTxProb) {
    ChangeState(State::kEnabled);
  }
  if (rcvwnd_.rcv_ > Seq16(0)) {
    // sender realize me as portid reset
    m_MDS_LOG_DBG("FCTRL: [me] <-- [node:%x, ref:%u], "
        "rcvwnd[acked:%u, rcv:%u, nacked:%" PRIu64 "], "
        "[portid reset on sender]",
        id_.node, id_.ref,
        rcvwnd_.acked_.v(), rcvwnd_.rcv_.v(), rcvwnd_.nacked_space_);
    rcvwnd_.rcv_ = Seq16(0);
    rcvwnd_.acked_ = rcvwnd_.rcv_;
  }
}

}  // end namespace mds
