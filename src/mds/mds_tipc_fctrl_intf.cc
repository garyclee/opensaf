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
#include "mds/mds_tipc_fctrl_intf.h"

#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/poll.h>
#include <unistd.h>

#include <map>
#include <mutex>

#include "base/ncssysf_def.h"
#include "base/ncssysf_tsk.h"

#include "mds/mds_log.h"
#include "mds/mds_tipc_fctrl_portid.h"
#include "mds/mds_tipc_fctrl_msg.h"

using mds::Event;
using mds::TipcPortId;
using mds::Timer;
using mds::DataMessage;
using mds::ChunkAck;
using mds::HeaderMessage;
using mds::Nack;

namespace {
// flow control enabled/disabled
bool is_fctrl_enabled = false;

// multicast/broadcast enabled
// todo: to be removed if flow control support it
bool is_mcast_enabled = true;

// mailbox handles for events
SYSF_MBX mbx_events;

// ncs task handle
NCSCONTEXT p_task_hdl = nullptr;

// data socket associated with port id
int data_sock_fd = 0;
struct tipc_portid snd_rcv_portid;

// socket receiver's buffer size
// todo: This buffer size is read from the local node as an estimated
// buffer size of the receiver sides, in facts that could be different
// at the receiver's sides (in the other nodes). At this moment, we
// assume all nodes in cluster have set the same tipc buffer size
uint64_t sock_buf_size = 0;

// map of key:unique id(adest), value: TipcPortId instance
// unique id is derived from struct tipc_portid
std::map<uint64_t, TipcPortId*> portid_map;
std::mutex portid_map_mutex;

// probation timer event to enable flow control at receivers
const int64_t kBaseTimerInt = 100;  // in centisecond
const uint8_t kTxProbMaxRetries = 10;
Timer txprob_timer(Event::Type::kEvtTmrTxProb);

// chunk ack parameters
// todo: The chunk ack timeout and chunk ack size should be configurable
int kChunkAckTimeout = 1000;  // in miliseconds
uint16_t kChunkAckSize = 3;

TipcPortId* portid_lookup(struct tipc_portid id) {
  uint64_t uid = TipcPortId::GetUniqueId(id);
  if (portid_map.find(uid) == portid_map.end()) return nullptr;
  return portid_map[uid];
}

void tmr_exp_cbk(void* uarg) {
  Timer* timer = reinterpret_cast<Timer*>(uarg);
  if (timer != nullptr) {
    timer->is_active_ = false;
    // send to fctrl thread
    if (m_NCS_IPC_SEND(&mbx_events, new Event(timer->type_),
        NCS_IPC_PRIORITY_HIGH) != NCSCC_RC_SUCCESS) {
      m_MDS_LOG_ERR("FCTRL: Failed to send msg to mbx_events, Error[%s]",
          strerror(errno));
    }
  }
}

void process_timer_event(const Event& evt) {
  bool txprob_restart = false;
  for (auto i : portid_map) {
    TipcPortId* portid = i.second;

    if (evt.type_ == Event::Type::kEvtTmrTxProb) {
      if (portid->ReceiveTmrTxProb(kTxProbMaxRetries) == true) {
        txprob_restart = true;
      }
    }

    if (evt.type_ == Event::Type::kEvtTmrChunkAck) {
      portid->ReceiveTmrChunkAck();
    }
  }
  if (txprob_restart) {
    txprob_timer.Start(kBaseTimerInt, tmr_exp_cbk);
    m_MDS_LOG_DBG("FCTRL: Restart txprob");
  }
}

uint32_t process_flow_event(const Event& evt) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  TipcPortId *portid = portid_lookup(evt.id_);
  if (portid == nullptr) {
    if (evt.type_ == Event::Type::kEvtRcvData) {
      portid = new TipcPortId(evt.id_, data_sock_fd,
          kChunkAckSize, sock_buf_size);
      portid_map[TipcPortId::GetUniqueId(evt.id_)] = portid;
      rc = portid->ReceiveData(evt.mseq_, evt.mfrag_,
            evt.fseq_, evt.svc_id_);
    } else {
      m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
          "RcvEvt[evt:%d], Error[PortId not found]",
          evt.id_.node, evt.id_.ref, (int)evt.type_);
    }
  } else {
    if (evt.type_ == Event::Type::kEvtRcvData) {
      rc = portid->ReceiveData(evt.mseq_, evt.mfrag_,
          evt.fseq_, evt.svc_id_);
    }
    if (evt.type_ == Event::Type::kEvtRcvChunkAck) {
      portid->ReceiveChunkAck(evt.fseq_, evt.chunk_size_);
    }
    if (evt.type_ == Event::Type::kEvtSendChunkAck) {
      portid->SendChunkAck(evt.fseq_, evt.svc_id_, evt.chunk_size_);
    }
    if (evt.type_ == Event::Type::kEvtDropData ||
        evt.type_ == Event::Type::kEvtRcvNack) {
      portid->ReceiveNack(evt.mseq_, evt.mfrag_,
          evt.fseq_);
    }
  }
  return rc;
}

uint32_t process_all_events(void) {
  enum { FD_FCTRL = 0, NUM_FDS };

  int poll_tmo = kChunkAckTimeout;
  struct pollfd pfd[NUM_FDS] = {{0, 0, 0}};

  pfd[FD_FCTRL].fd =
      ncs_ipc_get_sel_obj(&mbx_events).rmv_obj;
  pfd[FD_FCTRL].events = POLLIN;

  while (true) {
    int pollres;

    pollres = poll(pfd, NUM_FDS, poll_tmo);

    if (pollres == -1) {
      if (errno == EINTR) continue;
      m_MDS_LOG_ERR("FCTRL: poll() failed, Error[%s]", strerror(errno));
      break;
    }

    if (pollres > 0) {
      if (pfd[FD_FCTRL].revents == POLLIN) {
        portid_map_mutex.lock();

        Event *evt = reinterpret_cast<Event*>(ncs_ipc_non_blk_recv(
            &mbx_events));

        if (evt == nullptr) {
          portid_map_mutex.unlock();
          continue;
        }
        if (evt->IsTimerEvent()) {
          process_timer_event(*evt);
        }
        if (evt->IsFlowEvent()) {
          process_flow_event(*evt);
        }

        delete evt;
        portid_map_mutex.unlock();
      }
    }
    // timeout, scan all portid and send ack msgs
    if (pollres == 0) {
      portid_map_mutex.lock();
      process_timer_event(Event(Event::Type::kEvtTmrChunkAck));
      portid_map_mutex.unlock();
    }
  }  /* while */
  return NCSCC_RC_SUCCESS;
}

uint32_t create_ncs_task(void *task_hdl) {
  int policy = SCHED_OTHER; /*root defaults */
  int max_prio = sched_get_priority_max(policy);
  int min_prio = sched_get_priority_min(policy);
  int prio_val = ((max_prio - min_prio) * 0.87);

  if (m_NCS_IPC_CREATE(&mbx_events) != NCSCC_RC_SUCCESS) {
    m_MDS_LOG_ERR("FCTRL: m_NCS_IPC_CREATE failed, Error[%s]",
        strerror(errno));
    return NCSCC_RC_FAILURE;
  }
  if (m_NCS_IPC_ATTACH(&mbx_events) != NCSCC_RC_SUCCESS) {
    m_MDS_LOG_ERR("FCTRL: m_NCS_IPC_ATTACH failed, Error[%s]",
        strerror(errno));
    m_NCS_IPC_RELEASE(&mbx_events, nullptr);
    return NCSCC_RC_FAILURE;
  }
  if (ncs_task_create((NCS_OS_CB)process_all_events, 0,
          "OSAF_MDS", prio_val, policy, NCS_MDTM_STACKSIZE,
          &task_hdl) != NCSCC_RC_SUCCESS) {
    m_MDS_LOG_ERR("FCTRL: ncs_task_create(), Error[%s]",
        strerror(errno));
    m_NCS_IPC_RELEASE(&mbx_events, nullptr);
    return NCSCC_RC_FAILURE;
  }

  m_MDS_LOG_NOTIFY("FCTRL: Start process_all_events");
  return NCSCC_RC_SUCCESS;
}

}  // end local namespace

uint32_t mds_tipc_fctrl_initialize(int dgramsock, struct tipc_portid id,
    uint64_t rcv_buf_size, int32_t ackto, int32_t acksize,
    bool mcast_enabled) {

  data_sock_fd = dgramsock;
  snd_rcv_portid = id;
  sock_buf_size = rcv_buf_size;
  is_mcast_enabled = mcast_enabled;
  if (ackto != -1) kChunkAckTimeout = ackto;
  if (acksize != -1) kChunkAckSize = acksize;

  if (create_ncs_task(&p_task_hdl) !=
      NCSCC_RC_SUCCESS) {
    m_MDS_LOG_ERR("FCTRL: create_ncs_task() failed, Error[%s]",
        strerror(errno));
    return NCSCC_RC_FAILURE;
  }
  is_fctrl_enabled = true;
  m_MDS_LOG_NOTIFY("FCTRL: Initialize [node:%x, ref:%u]",
      id.node, id.ref);

  return NCSCC_RC_SUCCESS;
}

uint32_t mds_tipc_fctrl_shutdown(void) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  portid_map_mutex.lock();

  if (ncs_task_release(p_task_hdl) != NCSCC_RC_SUCCESS) {
    m_MDS_LOG_ERR("FCTRL: Stop of the Created Task-failed, Error[%s]",
        strerror(errno));
  }

  m_NCS_IPC_DETACH(&mbx_events, nullptr, nullptr);
  m_NCS_IPC_RELEASE(&mbx_events, nullptr);

  for (auto i : portid_map) delete i.second;
  portid_map.clear();

  portid_map_mutex.unlock();
  is_fctrl_enabled = false;

  m_MDS_LOG_NOTIFY("FCTRL: Shutdown [node:%x, ref:%u]",
      snd_rcv_portid.node, snd_rcv_portid.ref);

  return NCSCC_RC_SUCCESS;
}

uint32_t mds_tipc_fctrl_sndqueue_capable(struct tipc_portid id,
          uint16_t* next_seq) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  uint32_t rc = NCSCC_RC_SUCCESS;

  portid_map_mutex.lock();

  TipcPortId *portid = portid_lookup(id);
  if (portid == nullptr) {
    m_MDS_LOG_ERR("FCTRL: [me] --> [node:%x, ref:%u], "
        "[line:%u], Error[PortId not found]",
        id.node, id.ref, __LINE__);
    rc = NCSCC_RC_FAILURE;
  } else {
    if (portid->state_ != TipcPortId::State::kDisabled) {
      // assign the sequence number of the outgoing message
      *next_seq = portid->GetCurrentSeq();
    }
  }

  portid_map_mutex.unlock();

  return rc;
}

uint32_t mds_tipc_fctrl_trysend(const uint8_t *buffer, uint16_t len,
    struct tipc_portid id) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  uint32_t rc = NCSCC_RC_SUCCESS;

  portid_map_mutex.lock();

  TipcPortId *portid = portid_lookup(id);
  if (portid == nullptr) {
    m_MDS_LOG_ERR("FCTRL: [me] --> [node:%x, ref:%u], "
        "[line:%u], Error[PortId not found]",
        id.node, id.ref, __LINE__);
    rc = NCSCC_RC_FAILURE;
  } else {
    if (portid->state_ != TipcPortId::State::kDisabled) {
      bool sendable = portid->ReceiveCapable(len);
      portid->Queue(buffer, len, sendable);
      // start txprob timer for the first msg sent out
      // do not start for other states
      if (sendable && portid->state_ == TipcPortId::State::kStartup) {
        txprob_timer.Start(kBaseTimerInt, tmr_exp_cbk);
        m_MDS_LOG_DBG("FCTRL: Start txprob");
        portid->state_ = TipcPortId::State::kTxProb;
      }
      if (sendable == false) rc = NCSCC_RC_FAILURE;
    }
  }

  portid_map_mutex.unlock();

  return rc;
}

uint32_t mds_tipc_fctrl_portid_up(struct tipc_portid id, uint32_t type) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  MDS_SVC_ID svc_id = (uint16_t)(type & MDS_EVENT_MASK_FOR_SVCID);

  portid_map_mutex.lock();

  // Add this new tipc portid to the map
  TipcPortId *portid = portid_lookup(id);
  uint64_t uid = TipcPortId::GetUniqueId(id);
  if (portid == nullptr) {
    portid_map[uid] = portid = new TipcPortId(id, data_sock_fd,
        kChunkAckSize, sock_buf_size);
    m_MDS_LOG_NOTIFY("FCTRL: Add portid[node:%x, ref:%u svc_id:%u], svc_cnt:%u",
        id.node, id.ref, svc_id, portid->svc_cnt_);
  } else {
    portid->svc_cnt_++;
    m_MDS_LOG_NOTIFY("FCTRL: Add svc[node:%x, ref:%u svc_id:%u], svc_cnt:%u",
        id.node, id.ref, svc_id, portid->svc_cnt_);
  }

  portid_map_mutex.unlock();

  return NCSCC_RC_SUCCESS;
}

uint32_t mds_tipc_fctrl_portid_down(struct tipc_portid id, uint32_t type) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  MDS_SVC_ID svc_id = (uint16_t)(type & MDS_EVENT_MASK_FOR_SVCID);

  portid_map_mutex.lock();

  // Delete this tipc portid out of the map
  TipcPortId *portid = portid_lookup(id);
  if (portid != nullptr) {
    portid->svc_cnt_--;
    m_MDS_LOG_NOTIFY("FCTRL: Remove svc[node:%x, ref:%u svc_id:%u], svc_cnt:%u",
        id.node, id.ref, svc_id, portid->svc_cnt_);
  }
  portid_map_mutex.unlock();

  return NCSCC_RC_SUCCESS;
}

uint32_t mds_tipc_fctrl_portid_terminate(struct tipc_portid id) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  portid_map_mutex.lock();

  // Delete this tipc portid out of the map
  TipcPortId *portid = portid_lookup(id);
  if (portid != nullptr) {
    delete portid;
    portid_map.erase(TipcPortId::GetUniqueId(id));
    m_MDS_LOG_NOTIFY("FCTRL: Remove portid[node:%x, ref:%u]", id.node, id.ref);
  }

  portid_map_mutex.unlock();

  return NCSCC_RC_SUCCESS;
}

uint32_t mds_tipc_fctrl_drop_data(uint8_t *buffer, uint16_t len,
    struct tipc_portid id) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  HeaderMessage header;
  header.Decode(buffer);
  // if mds support flow control
  if ((header.pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_FCTRL) {
    if (header.pro_id_ == MDS_PROT_FCTRL_ID) {
      if (header.msg_type_ == ChunkAck::kChunkAckMsgType) {
        // receive single ack message
        ChunkAck ack;
        ack.Decode(buffer);
        // send to the event thread
        if (m_NCS_IPC_SEND(&mbx_events,
            new Event(Event::Type::kEvtSendChunkAck, id, ack.svc_id_,
                header.mseq_, header.mfrag_, ack.acked_fseq_, ack.chunk_size_),
            NCS_IPC_PRIORITY_HIGH) != NCSCC_RC_SUCCESS) {
          m_MDS_LOG_ERR("FCTRL: Failed to send msg to mbx_events, Error[%s]",
              strerror(errno));
          return NCSCC_RC_FAILURE;
        }
        return NCSCC_RC_SUCCESS;
      }
    } else {
      // receive data message
      DataMessage data;
      data.Decode(buffer);
      // send to the event thread
      if (m_NCS_IPC_SEND(&mbx_events,
          new Event(Event::Type::kEvtDropData, id, data.svc_id_,
              header.mseq_, header.mfrag_, header.fseq_),
          NCS_IPC_PRIORITY_HIGH) != NCSCC_RC_SUCCESS) {
        m_MDS_LOG_ERR("FCTRL: Failed to send msg to mbx_events, Error[%s]",
            strerror(errno));
        return NCSCC_RC_FAILURE;
      }
      return NCSCC_RC_SUCCESS;
    }
  }

  return NCSCC_RC_SUCCESS;
}

uint32_t mds_tipc_fctrl_rcv_data(uint8_t *buffer, uint16_t len,
    struct tipc_portid id) {
  if (is_fctrl_enabled == false) return NCSCC_RC_SUCCESS;

  HeaderMessage header;
  header.Decode(buffer);
  // if mds support flow control
  if ((header.pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_FCTRL) {
    if (header.pro_id_ == MDS_PROT_FCTRL_ID) {
      if (header.msg_type_ == ChunkAck::kChunkAckMsgType) {
        // receive single ack message
        ChunkAck ack;
        ack.Decode(buffer);
        // send to the event thread
        if (m_NCS_IPC_SEND(&mbx_events,
            new Event(Event::Type::kEvtRcvChunkAck, id, ack.svc_id_,
                header.mseq_, header.mfrag_, ack.acked_fseq_, ack.chunk_size_),
                NCS_IPC_PRIORITY_HIGH) != NCSCC_RC_SUCCESS) {
          m_MDS_LOG_ERR("FCTRL: Failed to send msg to mbx_events, Error[%s]",
              strerror(errno));
        }
      } else if (header.msg_type_ == Nack::kNackMsgType) {
        // receive nack message
        Nack nack;
        nack.Decode(buffer);
        // send to the event thread
        if (m_NCS_IPC_SEND(&mbx_events,
            new Event(Event::Type::kEvtRcvNack, id, nack.svc_id_,
                header.mseq_, header.mfrag_, nack.nacked_fseq_),
                NCS_IPC_PRIORITY_HIGH) != NCSCC_RC_SUCCESS) {
          m_MDS_LOG_ERR("FCTRL: Failed to send msg to mbx_events, Error[%s]",
              strerror(errno));
        }
      } else {
        m_MDS_LOG_ERR("FCTRL: [me] <-- [node:%x, ref:%u], "
            "[msg_type:%u], Error[not supported message type]",
            id.node, id.ref, header.msg_type_);
      }
      // return NCSCC_RC_FAILURE, so the tipc receiving thread (legacy) will
      // skip this data msg
      return NCSCC_RC_FAILURE;
    } else {
      // receive data message
      DataMessage data;
      data.Decode(buffer);
      // todo: skip mcast/bcast, revisit
      if ((data.snd_type_ == MDS_SENDTYPE_BCAST ||
          data.snd_type_ == MDS_SENDTYPE_RBCAST) && is_mcast_enabled) {
        return NCSCC_RC_SUCCESS;
      }
      portid_map_mutex.lock();
      uint32_t rc = process_flow_event(Event(Event::Type::kEvtRcvData,
          id, data.svc_id_, header.mseq_, header.mfrag_, header.fseq_));
      if (rc == NCSCC_RC_CONTINUE) {
        process_timer_event(Event(Event::Type::kEvtTmrChunkAck));
        rc = NCSCC_RC_SUCCESS;
      }
      portid_map_mutex.unlock();
      return rc;
    }
  }
  return NCSCC_RC_SUCCESS;
}
