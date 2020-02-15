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

#include "log/agent/lga_client.h"
#include <string.h>
#include <assert.h>
#include <algorithm>
#include "base/time.h"
#include "base/ncs_hdl_pub.h"
#include "base/saf_error.h"
#include "base/mutex.h"
#include "log/agent/lga_mds.h"
#include "log/common/lgsv_defs.h"
#include "log/agent/lga_common.h"
#include "log/agent/lga_agent.h"

//------------------------------------------------------------------------------
// LogClient
//------------------------------------------------------------------------------
LogClient::LogClient(const SaLogCallbacksT* cb, uint32_t id, SaVersionT ver)
    : client_id_{id}, ref_counter_object_{} {
  TRACE_ENTER();
  // Reset registered callback info
  memset(&callbacks_, 0, sizeof(callbacks_));
  stream_list_.clear();

  handle_ = 0;
  version_ = ver;

  if (cb != nullptr) {
    memcpy(&callbacks_, cb, sizeof(SaLogCallbacksT));
  }

  // Initialize @handle_ for this client
  if ((handle_ = ncshm_create_hdl(NCS_HM_POOL_ID_COMMON, NCS_SERVICE_ID_LGA,
                                  this)) == 0) {
    assert(handle_ != 0 && "ncshm_create_hdl failed");
  }

  // Setup the mailbox for this client
  uint32_t rc;
  if ((rc = m_NCS_IPC_CREATE(&mailbox_)) != NCSCC_RC_SUCCESS) {
    assert(rc == NCSCC_RC_SUCCESS && "Creating mbx failed");
  }
  if ((rc = m_NCS_IPC_ATTACH(&mailbox_)) != NCSCC_RC_SUCCESS) {
    assert(rc != NCSCC_RC_SUCCESS && "Attach the mbx failed");
  }

  // As there is high risk of calling of one @LogClient method
  // in the body of other method, such case would cause deadlock
  // even they are in the same thread context.
  // To avoid such risk, use RECURSIVE MUTEX for @LogClient
  pthread_mutexattr_t attr;
  int result = pthread_mutexattr_init(&attr);
  assert(result == 0 && "Failed to init mutex attribute");

  result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  assert(result == 0 && "Failed to set mutex type");

  result = pthread_mutex_init(&mutex_, nullptr);
  assert(result == 0 && "Failed to init mutex");

  pthread_mutexattr_destroy(&attr);
}

LogClient::~LogClient() {
  TRACE_ENTER();

  // Free all log streams belong to this client
  for (auto& stream : stream_list_) {
    if (stream != nullptr) delete stream;
  }

  stream_list_.clear();
  CleanUnackedList();

  // Free the client handle allocated to this log client
  if (handle_ != 0) {
    ncshm_give_hdl(handle_);
    ncshm_destroy_hdl(NCS_SERVICE_ID_LGA, handle_);
    handle_ = 0;
  }

  m_NCS_IPC_DETACH(&mailbox_, LogClient::ClearMailBox, nullptr);
  // Free the allocated mailbox for this client
  m_NCS_IPC_RELEASE(&mailbox_, nullptr);

  pthread_mutex_destroy(&mutex_);
}

bool LogClient::ClearMailBox(NCSCONTEXT arg, NCSCONTEXT msg) {
  TRACE_ENTER();
  lgsv_msg_t *cbk, *pnext;

  pnext = cbk = static_cast<lgsv_msg_t*>(msg);
  while (pnext) {
    pnext = cbk->next;
    lga_msg_destroy(cbk);
    cbk = pnext;
  }
  return true;
}

SaSelectionObjectT LogClient::GetSelectionObject() {
  TRACE_ENTER();
  // Obtain the selection object from the IPC queue
  const NCS_SEL_OBJ sel_obj = m_NCS_IPC_GET_SEL_OBJ(&mailbox_);
  // Get selection fd
  return m_GET_FD_FROM_SEL_OBJ(sel_obj);
}

void LogClient::InvokeCallback(const lgsv_msg_t* msg) {
  TRACE_ENTER();
  assert(msg != nullptr);
  const lgsv_cbk_info_t* cbk_info = &msg->info.cbk_info;

  // Invoke the corresponding callback
  switch (cbk_info->type) {
    case LGSV_WRITE_LOG_CALLBACK_IND: {
      if (callbacks_.saLogWriteLogCallback) {
        RemoveTrack(cbk_info->inv);
        callbacks_.saLogWriteLogCallback(cbk_info->inv,
                                         cbk_info->write_cbk.error);
      }
    } break;

    case LGSV_SEVERITY_FILTER_CALLBACK: {
      if (callbacks_.saLogFilterSetCallback) {
        LogStreamInfo* stream;
        stream = SearchLogStreamInfoById(cbk_info->lgs_stream_id);
        ScopeLock scopeLock(mutex_);
        if (stream != nullptr) {
          callbacks_.saLogFilterSetCallback(
              stream->GetHandle(), cbk_info->serverity_filter_cbk.log_severity);
        }
      }
    } break;

    default:
      TRACE("Unknown callback type: %d", cbk_info->type);
      break;
  }
}

SaAisErrorT LogClient::Dispatch(SaDispatchFlagsT flag) {
  TRACE_ENTER();
  SaAisErrorT rc;
  switch (flag) {
    case SA_DISPATCH_ONE:
      rc = DispatchOne();
      break;

    case SA_DISPATCH_ALL:
      rc = DispatchAll();
      break;

    case SA_DISPATCH_BLOCKING:
      rc = DispatchBlocking();
      break;

    default:
      TRACE("Dispatch flag not valid");
      rc = SA_AIS_ERR_INVALID_PARAM;
      break;
  }
  return rc;
}

SaAisErrorT LogClient::DispatchOne() {
  TRACE_ENTER();
  lgsv_msg_t* cbk_msg;
  SaAisErrorT rc = SA_AIS_OK;

  // Nonblk receive to obtain the message from priority queue
  while (nullptr != (cbk_msg = reinterpret_cast<lgsv_msg_t*>(
                         m_NCS_IPC_NON_BLK_RECEIVE(&mailbox_, cbk_msg)))) {
    if (cbk_msg->info.cbk_info.type == LGSV_WRITE_LOG_CALLBACK_IND ||
        cbk_msg->info.cbk_info.type == LGSV_SEVERITY_FILTER_CALLBACK) {
      InvokeCallback(cbk_msg);
      lga_msg_destroy(cbk_msg);
      return rc;
    }

    TRACE("Unsupported callback type = %d", cbk_msg->info.cbk_info.type);
    rc = SA_AIS_ERR_LIBRARY;
    lga_msg_destroy(cbk_msg);
  }
  return rc;
}

SaAisErrorT LogClient::DispatchAll() {
  TRACE_ENTER();
  lgsv_msg_t* cbk_msg;
  SaAisErrorT rc = SA_AIS_OK;

  // Recv all the cbk notifications from the queue & process them
  do {
    cbk_msg = reinterpret_cast<lgsv_msg_t*>(
        m_NCS_IPC_NON_BLK_RECEIVE(&mailbox_, cbk_msg));
    if (cbk_msg == nullptr) return rc;

    if (cbk_msg->info.cbk_info.type == LGSV_WRITE_LOG_CALLBACK_IND ||
        cbk_msg->info.cbk_info.type == LGSV_SEVERITY_FILTER_CALLBACK) {
      TRACE_2("LGSV_LGS_DELIVER_EVENT");
      InvokeCallback(cbk_msg);
    } else {
      TRACE("Unsupported callback type %d", cbk_msg->info.cbk_info.type);
    }
    // Now that we are done with this rec, free the resources
    lga_msg_destroy(cbk_msg);
  } while (1);

  return rc;
}

SaAisErrorT LogClient::DispatchBlocking() {
  TRACE_ENTER();
  lgsv_msg_t* cbk_msg;
  SaAisErrorT rc = SA_AIS_OK;

  for (;;) {
    if (nullptr != (cbk_msg = reinterpret_cast<lgsv_msg_t*>(
                        m_NCS_IPC_RECEIVE(&mailbox_, cbk_msg)))) {
      if (cbk_msg->info.cbk_info.type == LGSV_WRITE_LOG_CALLBACK_IND ||
          cbk_msg->info.cbk_info.type == LGSV_SEVERITY_FILTER_CALLBACK) {
        TRACE_2("LGSV_LGS_DELIVER_EVENT");
        InvokeCallback(cbk_msg);
      } else {
        TRACE("unsupported callback type %d", cbk_msg->info.cbk_info.type);
      }
      lga_msg_destroy(cbk_msg);
    } else {
      // FIX to handle finalize clean up of mbx
      return rc;
    }
  }
  return rc;
}

// Add @stream from @stream_list_ and should be called
// when getting @saLogStreamOpen()
void LogClient::AddLogStreamInfo(LogStreamInfo* stream,
                                 SaLogStreamOpenFlagsT flag,
                                 SaLogHeaderTypeT htype) {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  assert(stream != nullptr);
  // Give additional info to @stream
  stream->SetOpenFlags(flag);
  stream->SetHeaderType(htype);
  stream->SetClientHandle(handle_);
  stream_list_.push_back(stream);
}

// Remove @stream from the @stream_list_ based on @handle
// and should be called after getting @saLogStreamClose() or @saLogFinalize().
void LogClient::RemoveLogStreamInfo(LogStreamInfo** stream) {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  assert(*stream != nullptr);
  auto i = std::remove(stream_list_.begin(), stream_list_.end(), *stream);
  if (i != stream_list_.end()) {
    stream_list_.erase(i, stream_list_.end());
    delete *stream;
    *stream = nullptr;
  }
}

LogStreamInfo* LogClient::SearchLogStreamInfoById(uint32_t id) {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  LogStreamInfo* stream = nullptr;
  for (auto const& s : stream_list_) {
    if (s != nullptr && s->GetStreamId() == id) {
      stream = s;
      break;
    }
  }
  return stream;
}

bool LogClient::SendInitializeMsg() {
  TRACE_ENTER();
  SaAisErrorT ais_rc = SA_AIS_ERR_TRY_AGAIN;
  uint32_t ncs_rc = NCSCC_RC_SUCCESS;
  lgsv_msg_t i_msg, *o_msg = nullptr;

  // Populate the message to be sent to the Log server
  memset(&i_msg, 0, sizeof(lgsv_msg_t));
  i_msg.type = LGSV_LGA_API_MSG;
  i_msg.info.api_info.type = LGSV_INITIALIZE_REQ;
  i_msg.info.api_info.param.init.version = version_;

  //>
  // Send a message to Log server to obtain @client_id_
  //<
  // Default wait time for retries (30s)
  const uint64_t kWaitTime = 30 * 1000;
  base::Timer wtime(kWaitTime);
  while (wtime.is_timeout() == false) {
    ncs_rc = lga_mds_msg_sync_send(&i_msg, &o_msg, LGS_WAIT_TIME,
                                   MDS_SEND_PRIORITY_HIGH);
    if (ncs_rc != NCSCC_RC_SUCCESS) {
      LOG_NO("%s lga_mds_msg_sync_send() fail %d", __func__, ncs_rc);
      return false;
    }
    assert(o_msg != nullptr);
    ais_rc = o_msg->info.api_resp_info.rc;
    if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(base::kOneHundredMilliseconds);
      continue;
    }
    break;
  }

  if (SA_AIS_OK != ais_rc) {
    TRACE("%s LGS error response %s", __func__, saf_error(ais_rc));
    lga_msg_destroy(o_msg);
    return false;
  }

  // Initialize succeeded
  client_id_ = o_msg->info.api_resp_info.param.init_rsp.client_id;
  // Free up the response message
  lga_msg_destroy(o_msg);

  return true;
}

bool LogClient::InitializeClient() {
  TRACE_ENTER();
  if (atomic_data_.initialized_flag == true) return true;
  if (SendInitializeMsg() == false) return false;
  atomic_data_.initialized_flag = true;
  return true;
}

bool LogClient::RecoverMe() {
  TRACE_ENTER();
  if (atomic_data_.recovered_flag == true) return true;
  if (InitializeClient() == false) {
    TRACE("%s Failed to initialize client id %d", __func__, client_id_);
    return false;
  }

  ScopeLock scopeLock(mutex_);
  for (const auto& stream : stream_list_) {
    if (stream == nullptr) continue;
    TRACE("Recover client = %d, stream = %d", client_id_,
          stream->GetStreamId());
    if (stream->RecoverMeForClientId(client_id_) == false) {
      TRACE("RecoverMeForClientId failed: client id (%d), stream id (%d)",
            client_id_, stream->GetStreamId());
      return false;
    }
  }

  // Succesfully recover
  atomic_data_.recovered_flag = true;
  return true;
}

bool LogClient::HaveLogStreamInUse() {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  for (const auto& s : stream_list_) {
    if ((s != nullptr) && (s->ref_counter_object_.ref_counter() > 0))
      return true;
  }
  return false;
}

void LogClient::NoLogServer() {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  // No SCs, no valid client id.
  client_id_ = 0;
  atomic_data_.recovered_flag = false;
  atomic_data_.initialized_flag = false;

  // Inform the SCs absence to all its log streams.
  for (auto& stream : stream_list_) {
    if (stream == nullptr) continue;
    // Mark its own log stream is "not yet recover."
    // When LOG server restart from headless, Log agent will do recover them.
    stream->SetRecoveryFlag(false);
  }

  NotifyClientAboutLostInvocations();
}

uint32_t LogClient::SendMsgToMbx(lgsv_msg_t* msg, MDS_SEND_PRIORITY_TYPE prio) {
  TRACE_ENTER();
  if (NCSCC_RC_SUCCESS !=
      m_NCS_IPC_SEND(&mailbox_, msg, static_cast<NCS_IPC_PRIORITY>(prio))) {
    TRACE("IPC SEND FAILED");
    lga_msg_destroy(msg);
    return NCSCC_RC_FAILURE;
  }
  return NCSCC_RC_SUCCESS;
}
