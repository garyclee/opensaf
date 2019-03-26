/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2008, 2017 - All Rights Reserved.
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

#include "log/agent/lga_mds.h"
#include <stdlib.h>
#include "base/ncsencdec_pub.h"
#include "base/saf_error.h"
#include "log/agent/lga_state.h"
#include "log/agent/lga_util.h"
#include "base/osaf_extended_name.h"
#include "base/ncs_util.h"
#include "log/agent/lga_agent.h"
#include "log/agent/lga_client.h"
#include "log/agent/lga_common.h"
#include "log/common/lgsv_defs.h"

#define LGA_SVC_PVT_SUBPART_VERSION 1
#define LGA_WRT_LGS_SUBPART_VER_AT_MIN_MSG_FMT 1
#define LGA_WRT_LGS_SUBPART_VER_AT_MAX_MSG_FMT 1
#define LGA_WRT_LGS_SUBPART_VER_RANGE       \
  (LGA_WRT_LGS_SUBPART_VER_AT_MAX_MSG_FMT - \
   LGA_WRT_LGS_SUBPART_VER_AT_MIN_MSG_FMT + 1)

// msg format version for LGA subpart version 1
static MDS_CLIENT_MSG_FORMAT_VER
    LGA_WRT_LGS_MSG_FMT_ARRAY[LGA_WRT_LGS_SUBPART_VER_RANGE] = {1};

/****************************************************************************
  Name          : lga_enc_initialize_msg

  Description   : This routine encodes an initialize API msg

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_enc_initialize_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_initialize_req_t *param = &msg->info.api_info.param.init;

  TRACE_ENTER();
  osafassert(uba != nullptr);

  // Encode the contents
  p8 = ncs_enc_reserve_space(uba, 3);
  if (!p8) {
    TRACE("nullptr pointer");
    return 0;
  }
  ncs_encode_8bit(&p8, param->version.releaseCode);
  ncs_encode_8bit(&p8, param->version.majorVersion);
  ncs_encode_8bit(&p8, param->version.minorVersion);
  ncs_enc_claim_space(uba, 3);
  total_bytes += 3;

  TRACE_LEAVE();
  return total_bytes;
}

/****************************************************************************
  Name          : lga_enc_finalize_msg

  Description   : This routine encodes a finalize API msg

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_enc_finalize_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_finalize_req_t *param = &msg->info.api_info.param.finalize;

  TRACE_ENTER();

  osafassert(uba != nullptr);

  // Encode the contents
  p8 = ncs_enc_reserve_space(uba, 4);
  if (!p8) {
    TRACE("nullptr pointer");
    return 0;
  }
  ncs_encode_32bit(&p8, param->client_id);
  ncs_enc_claim_space(uba, 4);
  total_bytes += 4;

  TRACE_LEAVE();
  return total_bytes;
}

static uint32_t encode_sanamet(NCS_UBAID *uba, uint8_t *p8, SaNameT *name) {
  uint32_t total_bytes = 0;
  p8 = ncs_enc_reserve_space(uba, 2);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }

  SaConstStringT value = osaf_extended_name_borrow(name);
  size_t length = strlen(value);
  ncs_encode_16bit(&p8, length);
  ncs_enc_claim_space(uba, 2);
  total_bytes += 2;
  ncs_encode_n_octets_in_uba(
      uba, reinterpret_cast<uint8_t *>(const_cast<char *>(value)), length);
  total_bytes += length;
  return total_bytes;
}

/****************************************************************************
  Name          : lga_enc_lstr_open_sync_msg

  Description   : This routine encodes a log stream open sync API msg

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_enc_lstr_open_sync_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  int len;
  uint8_t *p8;
  uint32_t total_bytes = 0, bytes = 0;
  lgsv_stream_open_req_t *param = &msg->info.api_info.param.lstr_open_sync;

  TRACE_ENTER();
  osafassert(uba != nullptr);

  // Encode the contents
  p8 = ncs_enc_reserve_space(uba, 4);
  if (!p8) {
    TRACE("p8 nullptr!!!");
    return 0;
  }

  ncs_encode_32bit(&p8, param->client_id);
  ncs_enc_claim_space(uba, 4);
  total_bytes += 4;

  // Encode logStreamName
  bytes = encode_sanamet(uba, p8, &param->lstr_name);
  if (bytes == 0) {
    goto done;
  }
  total_bytes += bytes;

  // Encode logFileName if initiated
  p8 = ncs_enc_reserve_space(uba, 2);
  if (!p8) {
    TRACE("p8 nullptr!!!");
    goto done;
  }
  if (param->logFileName != nullptr) {
    len = strlen(param->logFileName) + 1;
  } else {
    // Workaround to keep backward compatible [#1686]
    // The problem was that standby node (OpenSAF 5.0) failed to come up
    // when active node ran with older OpenSAF version (e.g OpenSAF 4.7).
    //
    // In OpenSAF 4.7 or older, logFileName is declared as an static
    // array of chars and always passed to MDS layer for encoding.
    // The length (len) is at least one.
    // Therefore, in log service side, there is an precondition check in MDS
    // decoding callback if the length of logFileName data is invalid (0).
    //
    // In OpenSAF 5.0 or later, logFileName is declared as an dynamic one
    // and then there is posibility to be nullptr when opening one of
    // dedicated log streams. Suppose we did not pass any data for
    // encoding when logFileName is nullptr, when OpenSAF 5.0 log client
    // communicates with OpenSAF 4.7 active log service, it would get
    // failed at saLogStreamOpen() because of log service unsuccesfully
    // deeode the message at MDS layer (len = 0).
    len = 1;
  }

  ncs_encode_16bit(&p8, len);
  ncs_enc_claim_space(uba, 2);
  total_bytes += 2;

  if (param->logFileName != nullptr) {
    ncs_encode_n_octets_in_uba(
        uba,
        reinterpret_cast<uint8_t *>(static_cast<char *>(param->logFileName)),
        len);
    total_bytes += len;
  } else {
    // Keep backward compatible
    ncs_encode_n_octets_in_uba(
        uba, reinterpret_cast<uint8_t *>(const_cast<char *>("")), len);
    total_bytes += len;
  }

  // Encode logFilePathName if initiated
  p8 = ncs_enc_reserve_space(uba, 2);
  if (!p8) {
    TRACE("p8 nullptr!!!");
    goto done;
  }
  if (param->logFilePathName) {
    len = strlen(param->logFilePathName) + 1;
  } else {
    // Workaround to keep backward compatible
    len = 1;
  }

  ncs_encode_16bit(&p8, len);
  ncs_enc_claim_space(uba, 2);
  total_bytes += 2;

  if (param->logFilePathName != nullptr) {
    ncs_encode_n_octets_in_uba(
        uba,
        reinterpret_cast<uint8_t *>(const_cast<char *>(param->logFilePathName)),
        len);
    total_bytes += len;
  } else {
    // Workaround to keep backward compatible
    ncs_encode_n_octets_in_uba(
        uba, reinterpret_cast<uint8_t *>(const_cast<char *>("")), len);
    total_bytes += len;
  }

  // Encode format string if initiated
  p8 = ncs_enc_reserve_space(uba, 24);
  if (!p8) {
    TRACE("p8 nullptr!!!");
    goto done;
  }
  if (param->logFileFmt != nullptr)
    len = strlen(param->logFileFmt) + 1;
  else
    len = 0;
  ncs_encode_64bit(&p8, param->maxLogFileSize);
  ncs_encode_32bit(&p8, param->maxLogRecordSize);
  ncs_encode_32bit(&p8, param->haProperty);
  ncs_encode_32bit(&p8, param->logFileFullAction);
  ncs_encode_16bit(&p8, param->maxFilesRotated);
  ncs_encode_16bit(&p8, len);
  ncs_enc_claim_space(uba, 24);
  total_bytes += 24;

  if (len > 0) {
    ncs_encode_n_octets_in_uba(
        uba,
        reinterpret_cast<uint8_t *>(static_cast<char *>(param->logFileFmt)),
        len);
    total_bytes += len;
  }

  // Encode last item in struct => open flags!!!!
  p8 = ncs_enc_reserve_space(uba, 1);
  if (!p8) {
    TRACE("p8 nullptr!!!");
    goto done;
  }

  ncs_encode_8bit(&p8, param->lstr_open_flags);
  ncs_enc_claim_space(uba, 1);
  total_bytes += 1;

done:
  TRACE_LEAVE();
  return total_bytes;
}

/****************************************************************************
  Name          : lga_enc_lstr_close_msg

  Description   : This routine encodes a log stream close API msg

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_enc_lstr_close_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_stream_close_req_t *param = &msg->info.api_info.param.lstr_close;

  osafassert(uba != nullptr);

  // encode the contents
  p8 = ncs_enc_reserve_space(uba, 8);
  if (!p8) {
    TRACE("p8 nullptr!!!");
    return 0;
  }
  ncs_encode_32bit(&p8, param->client_id);
  ncs_encode_32bit(&p8, param->lstr_id);
  ncs_enc_claim_space(uba, 8);
  total_bytes += 8;

  return total_bytes;
}

static uint32_t lga_enc_write_ntf_header(NCS_UBAID *uba,
                                         const SaLogNtfLogHeaderT *ntfLogH) {
  uint8_t *p8;
  uint32_t total_bytes = 0, bytes = 0;

  p8 = ncs_enc_reserve_space(uba, 12);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }
  ncs_encode_64bit(&p8, ntfLogH->notificationId);
  ncs_encode_32bit(&p8, ntfLogH->eventType);
  ncs_enc_claim_space(uba, 12);
  total_bytes += 12;


  // Encode notificationObject
  bytes = encode_sanamet(uba, p8, ntfLogH->notificationObject);
  if (bytes == 0) {
    return 0;
  }
  total_bytes += bytes;

  // Encode notifyingObject
  bytes = encode_sanamet(uba, p8, ntfLogH->notifyingObject);
  if (bytes == 0) {
    return 0;
  }
  total_bytes += bytes;

  p8 = ncs_enc_reserve_space(uba, 16);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }
  ncs_encode_32bit(&p8, ntfLogH->notificationClassId->vendorId);
  ncs_encode_16bit(&p8, ntfLogH->notificationClassId->majorId);
  ncs_encode_16bit(&p8, ntfLogH->notificationClassId->minorId);
  ncs_encode_64bit(&p8, ntfLogH->eventTime);
  ncs_enc_claim_space(uba, 16);
  total_bytes += 16;

  return total_bytes;
}

static uint32_t lga_enc_write_gen_header(
    NCS_UBAID *uba, const lgsv_write_log_async_req_t *param,
    const SaLogGenericLogHeaderT *genLogH) {
  uint8_t *p8;
  uint32_t total_bytes = 0, bytes = 0;

  p8 = ncs_enc_reserve_space(uba, 8);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }
  ncs_encode_32bit(&p8, 0);
  ncs_encode_16bit(&p8, 0);
  ncs_encode_16bit(&p8, 0);
  ncs_enc_claim_space(uba, 8);
  total_bytes += 8;

  // Encode logSvcUsrName
  bytes = encode_sanamet(uba, p8, param->logSvcUsrName);
  if (bytes == 0) {
    return 0;
  }
  total_bytes += bytes;

  p8 = ncs_enc_reserve_space(uba, 2);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }
  ncs_encode_16bit(&p8, genLogH->logSeverity);
  total_bytes += 2;

  return total_bytes;
}

/****************************************************************************
  Name          : lga_enc_write_log_async_msg

  Description   : This routine encodes a write log async API msg

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_enc_write_log_async_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_write_log_async_req_t *param = &msg->info.api_info.param.write_log_async;
  const SaLogNtfLogHeaderT *ntfLogH;
  const SaLogGenericLogHeaderT *genLogH;

  osafassert(uba != nullptr);

  // Encode the contents
  p8 = ncs_enc_reserve_space(uba, 20);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }
  ncs_encode_64bit(&p8, param->invocation);
  ncs_encode_32bit(&p8, param->ack_flags);
  ncs_encode_32bit(&p8, param->client_id);
  ncs_encode_32bit(&p8, param->lstr_id);
  ncs_enc_claim_space(uba, 20);
  total_bytes += 20;

  p8 = ncs_enc_reserve_space(uba, 12);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }
  ncs_encode_64bit(&p8, *param->logTimeStamp);
  ncs_encode_32bit(&p8, param->logRecord->logHdrType);
  ncs_enc_claim_space(uba, 12);
  total_bytes += 12;

  // Alarm and application streams so far
  uint32_t bytecount = 0;
  switch (param->logRecord->logHdrType) {
    case SA_LOG_NTF_HEADER:
      ntfLogH = &param->logRecord->logHeader.ntfHdr;

      bytecount = lga_enc_write_ntf_header(uba, ntfLogH);
      if (bytecount == 0) return 0;

      total_bytes += bytecount;
      break;

    case SA_LOG_GENERIC_HEADER:
      genLogH = &param->logRecord->logHeader.genericHdr;

      bytecount = lga_enc_write_gen_header(uba, param, genLogH);
      if (bytecount == 0) return 0;

      total_bytes += bytecount;
      break;

    default:
      TRACE("ERROR IN logHdrType in logRecord!!!");
      break;
  }

  p8 = ncs_enc_reserve_space(uba, 4);
  if (!p8) {
    TRACE("Could not reserve space");
    return 0;
  }

  if (param->logRecord->logBuffer == nullptr) {
    ncs_encode_32bit(&p8, 0);
  } else {
    ncs_encode_32bit(&p8, param->logRecord->logBuffer->logBufSize);
  }
  ncs_enc_claim_space(uba, 4);
  total_bytes += 4;

  if ((param->logRecord->logBuffer != nullptr) &&
      (param->logRecord->logBuffer->logBuf != nullptr)) {
    ncs_encode_n_octets_in_uba(uba, param->logRecord->logBuffer->logBuf,
                               param->logRecord->logBuffer->logBufSize);
    total_bytes += param->logRecord->logBuffer->logBufSize;
  }
  return total_bytes;
}

/****************************************************************************
  Name          : lga_lgs_msg_proc

  Description   : This routine is used to process the ASYNC incoming
                  LGS messages.

  Arguments     : pointer to struct ncsmds_callback_info

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
static uint32_t lga_lgs_msg_proc(lgsv_msg_t *lgsv_msg,
                                 MDS_SEND_PRIORITY_TYPE prio) {
  TRACE_ENTER();
  int rc = NCSCC_RC_SUCCESS;

  assert(lgsv_msg != nullptr);

  // Lookup the hdl rec by client_id
  LogClient *client = nullptr;
  uint32_t id = lgsv_msg->info.cbk_info.lgs_client_id;
  LogAgent::instance()->EnterCriticalSection();
  if (nullptr == (client = LogAgent::instance()->SearchClientById(id))) {
    LogAgent::instance()->LeaveCriticalSection();
    TRACE("regid not found");
    lga_msg_destroy(lgsv_msg);
    return NCSCC_RC_FAILURE;
  }

  // @client is being deleted in other thread. DO NOT touch this.
  bool updated = false;
  if (client->FetchAndIncreaseRefCounter(__func__, &updated) == -1) {
    LogAgent::instance()->LeaveCriticalSection();
    lga_msg_destroy(lgsv_msg);
    TRACE_LEAVE();
    return rc;
  }
  LogAgent::instance()->LeaveCriticalSection();

  switch (lgsv_msg->type) {
    case LGSV_LGS_CBK_MSG:
      switch (lgsv_msg->info.cbk_info.type) {
        case LGSV_WRITE_LOG_CALLBACK_IND: {
          TRACE_2("LGSV_LGS_WRITE_LOG_CBK: inv_id = %d, cbk_error %s",
                  (int)lgsv_msg->info.cbk_info.inv,
                  saf_error(lgsv_msg->info.cbk_info.write_cbk.error));

          // Enqueue this message
          if (client->SendMsgToMbx(lgsv_msg, prio) != NCSCC_RC_SUCCESS) {
            rc = NCSCC_RC_FAILURE;
          }
        } break;

        case LGSV_CLM_NODE_STATUS_CALLBACK: {
          SaClmClusterChangesT status;
          status = lgsv_msg->info.cbk_info.clm_node_status_cbk.clm_node_status;

          TRACE_2("LGSV_CLM_NODE_STATUS_CALLBACK clm_node_status: %d", status);
          std::atomic<SaClmClusterChangesT> &clm_node_state =
              LogAgent::instance()->atomic_get_clm_node_state();
          clm_node_state =
              lgsv_msg->info.cbk_info.clm_node_status_cbk.clm_node_status;
          // A client becomes stale if Node loses CLM Membership.
          if (clm_node_state != SA_CLM_NODE_JOINED) {
            // If the node rejoins the cluster membership, processes executing
            // on the node will be able to reinitialize new library handles
            // and use the entire set of Log Service APIs that operate on
            // these new handles; however, invocation of APIs that
            // operate on handles acquired by any process before the node
            // left the membership will continue to fail with
            // SA_AIS_ERR_UNAVAILABLE (or with the special
            // treatment described above for asynchronous calls)
            // with the exception of saLogFinalize(),
            // which is used to free the library handles and all resources
            // associated with these handles. Hence, it is recommended
            // for the processes to finalize the library handles
            // as soon as the processes detect that
            // the node left the membership

            // Old LGA clients A.02.01 are always clm member
            client->atomic_set_stale_flag(true);
            TRACE("CLM_NODE callback is_stale_client: %d clm_node_state: %d",
                  client->is_stale_client(), clm_node_state.load());
          }
          lga_msg_destroy(lgsv_msg);
        } break;

        case LGSV_SEVERITY_FILTER_CALLBACK: {
          // Check if client did not set filter callback
          if (client->GetCallback()->saLogFilterSetCallback == nullptr) {
            lga_msg_destroy(lgsv_msg);
            break;
          }

          TRACE_2(
              "LGSV_SEVERITY_FILTER_CALLBACK: "
              "client_id = %d, stream_id %d, severity = %d",
              lgsv_msg->info.cbk_info.lgs_client_id,
              lgsv_msg->info.cbk_info.lgs_stream_id,
              lgsv_msg->info.cbk_info.serverity_filter_cbk.log_severity);

          // Enqueue this message
          if (NCSCC_RC_SUCCESS != client->SendMsgToMbx(lgsv_msg, prio)) {
            rc = NCSCC_RC_FAILURE;
          }
        } break;

        default:
          TRACE("unknown type %d", lgsv_msg->info.cbk_info.type);
          lga_msg_destroy(lgsv_msg);
          rc = NCSCC_RC_FAILURE;
          break;
      }
      break;

    default:
      // Unexpected message
      TRACE_2("Unexpected message type: %d", lgsv_msg->type);
      lga_msg_destroy(lgsv_msg);
      rc = NCSCC_RC_FAILURE;
      break;
  }

  client->RestoreRefCounter(__func__, RefCounter::Degree::kIncOne, updated);
  TRACE_LEAVE();
  return rc;
}

/****************************************************************************
  Name          : lga_mds_svc_evt

  Description   : This is a callback routine that is invoked to inform LGA
                  of MDS events. LGA had subscribed to these events during
                  through MDS subscription.

  Arguments     : pointer to struct ncsmds_callback_info

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
static uint32_t lga_mds_svc_evt(struct ncsmds_callback_info *mds_cb_info) {
  TRACE_ENTER();

  switch (mds_cb_info->info.svc_evt.i_change) {
    case NCSMDS_NO_ACTIVE:
      TRACE("%s\t NCSMDS_NO_ACTIVE", __func__);
      // This is a temporary server down e.g. during switch/fail over
      if (mds_cb_info->info.svc_evt.i_svc_id == NCSMDS_SVC_ID_LGS) {
        LogAgent::instance()->NoActiveLogServer();
      }
      break;

    case NCSMDS_DOWN:
      TRACE("%s\t NCSMDS_DOWN", __func__);
      // This may be a loss of server where all client and stream information
      // is lost on server side. In this situation client info in agent is
      // no longer valid and clients must register again (initialize)
      if (mds_cb_info->info.svc_evt.i_svc_id == NCSMDS_SVC_ID_LGS) {
        TRACE("LGS down");
        // Notify to LOG Agent that no LOG server exist.
        // In turn, it will inform to all its log clients
        // and all log streams belong to each log client.
        LogAgent::instance()->NoLogServer();
        // Stop the recovery thread if it is running
        lga_no_server_state_set();
      }
      break;

    case NCSMDS_NEW_ACTIVE:
    case NCSMDS_UP:
      switch (mds_cb_info->info.svc_evt.i_svc_id) {
        case NCSMDS_SVC_ID_LGS:
          TRACE("%s\t NCSMDS_UP", __func__);
          // Inform to LOG agent that LOG server is up from headless
          // and provide it the LOG server destination address too.
          LogAgent::instance()->HasActiveLogServer(
              mds_cb_info->info.svc_evt.i_dest);
          if (LogAgent::instance()->waiting_log_server_up() == true) {
            // Signal waiting thread
            m_NCS_SEL_OBJ_IND(LogAgent::instance()->get_lgs_sync_sel());
          }
          // Start recovery
          lga_serv_recov1state_set();
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
  Name          : lga_mds_rcv

  Description   : This is a callback routine that is invoked when LGA message
                  is received from LGS.

  Arguments     : pointer to struct ncsmds_callback_info

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
static uint32_t lga_mds_rcv(struct ncsmds_callback_info *mds_cb_info) {
  lgsv_msg_t *lgsv_msg =
      static_cast<lgsv_msg_t *>(mds_cb_info->info.receive.i_msg);
  uint32_t rc;

  // Process the message
  rc = lga_lgs_msg_proc(lgsv_msg, mds_cb_info->info.receive.i_priority);
  if (rc != NCSCC_RC_SUCCESS) {
    TRACE_2("lga_lgs_msg_proc returned: %d", rc);
  }

  return rc;
}

/****************************************************************************
  Name          : lga_mds_enc

  Description   : This is a callback routine that is invoked to encode LGS
                  messages.

  Arguments     : pointer to struct ncsmds_callback_info

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
static uint32_t lga_mds_enc(struct ncsmds_callback_info *info) {
  lgsv_msg_t *msg;
  NCS_UBAID *uba;
  uint8_t *p8;
  uint32_t total_bytes = 0;

  MDS_CLIENT_MSG_FORMAT_VER msg_fmt_version;

  TRACE_ENTER();
  msg_fmt_version = m_NCS_ENC_MSG_FMT_GET(
      info->info.enc.i_rem_svc_pvt_ver, LGA_WRT_LGS_SUBPART_VER_AT_MIN_MSG_FMT,
      LGA_WRT_LGS_SUBPART_VER_AT_MAX_MSG_FMT, LGA_WRT_LGS_MSG_FMT_ARRAY);
  if (0 == msg_fmt_version) {
    TRACE("Wrong msg_fmt_version!!\n");
    TRACE_LEAVE();
    return NCSCC_RC_FAILURE;
  }
  info->info.enc.o_msg_fmt_ver = msg_fmt_version;

  msg = static_cast<lgsv_msg_t *>(info->info.enc.i_msg);
  uba = info->info.enc.io_uba;

  if (uba == nullptr) {
    TRACE("uba=nullptr");
    TRACE_LEAVE();
    return NCSCC_RC_FAILURE;
  }

  // Encode the type of message
  p8 = ncs_enc_reserve_space(uba, 4);
  if (!p8) {
    TRACE("nullptr pointer");
    TRACE_LEAVE();
    return NCSCC_RC_FAILURE;
  }
  ncs_encode_32bit(&p8, msg->type);
  ncs_enc_claim_space(uba, 4);
  total_bytes += 4;

  TRACE_2("msgtype: %d", msg->type);
  if (LGSV_LGA_API_MSG == msg->type) {
    // Encode the API msg subtype
    p8 = ncs_enc_reserve_space(uba, 4);
    if (!p8) {
      TRACE("encode API msg subtype FAILED");
      // NOTE: ok to do return fail??
      TRACE_LEAVE();
      return NCSCC_RC_FAILURE;
    }
    ncs_encode_32bit(&p8, msg->info.api_info.type);
    ncs_enc_claim_space(uba, 4);
    total_bytes += 4;

    TRACE_2("api_info.type: %d\n", msg->info.api_info.type);
    switch (msg->info.api_info.type) {
      case LGSV_INITIALIZE_REQ:
        total_bytes += lga_enc_initialize_msg(uba, msg);
        break;

      case LGSV_FINALIZE_REQ:
        total_bytes += lga_enc_finalize_msg(uba, msg);
        break;

      case LGSV_STREAM_OPEN_REQ:
        total_bytes += lga_enc_lstr_open_sync_msg(uba, msg);
        break;

      case LGSV_STREAM_CLOSE_REQ:
        total_bytes += lga_enc_lstr_close_msg(uba, msg);
        break;

      case LGSV_WRITE_LOG_ASYNC_REQ:
        total_bytes += lga_enc_write_log_async_msg(uba, msg);
        break;

      default:
        TRACE("Unknown API type = %d", msg->info.api_info.type);
        break;
    }
  }

  TRACE_LEAVE2("total_bytes = %d", total_bytes);
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
  Name          : lga_dec_initialize_rsp_msg

  Description   : This routine decodes an initialize sync response message

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_dec_initialize_rsp_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_initialize_rsp_t *param = &msg->info.api_resp_info.param.init_rsp;
  uint8_t local_data[100];

  osafassert(uba != nullptr);

  p8 = ncs_dec_flatten_space(uba, local_data, 4);
  param->client_id = ncs_decode_32bit(&p8);
  ncs_dec_skip_space(uba, 4);
  total_bytes += 4;

  return total_bytes;
}

/****************************************************************************
  Name          : lga_dec_finalize_rsp_msg

  Description   : This routine decodes an finalize sync response message

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_dec_finalize_rsp_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_finalize_rsp_t *param = &msg->info.api_resp_info.param.finalize_rsp;
  uint8_t local_data[100];

  osafassert(uba != nullptr);

  p8 = ncs_dec_flatten_space(uba, local_data, 4);
  param->client_id = ncs_decode_32bit(&p8);
  ncs_dec_skip_space(uba, 4);
  total_bytes += 4;

  return total_bytes;
}

/****************************************************************************
  Name          : lga_dec_lstr_close_rsp_msg

  Description   : This routine decodes a close response message

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_dec_lstr_close_rsp_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_stream_close_rsp_t *param = &msg->info.api_resp_info.param.close_rsp;
  uint8_t local_data[100];

  osafassert(uba != nullptr);

  p8 = ncs_dec_flatten_space(uba, local_data, 8);
  param->client_id = ncs_decode_32bit(&p8);
  param->lstr_id = ncs_decode_32bit(&p8);
  ncs_dec_skip_space(uba, 8);
  total_bytes += 8;

  return total_bytes;
}

/****************************************************************************
  Name          : lga_dec_write_ckb_msg

  Description   : This routine decodes an initialize sync response message

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_dec_write_cbk_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_write_log_callback_ind_t *param = &msg->info.cbk_info.write_cbk;
  uint8_t local_data[100];

  osafassert(uba != nullptr);

  p8 = ncs_dec_flatten_space(uba, local_data, 4);
  param->error = static_cast<SaAisErrorT>(ncs_decode_32bit(&p8));
  ncs_dec_skip_space(uba, 4);
  total_bytes += 4;

  return total_bytes;
}

/****************************************************************************
Name          : lga_dec_clm_node_status_cbk_msg

Description   : This routine decodes  message

Arguments     : NCS_UBAID *msg,
LGSV_MSG *msg

Return Values : uint32_t

Notes         : None.
******************************************************************************/
static uint32_t lga_dec_clm_node_status_cbk_msg(NCS_UBAID *uba,
                                                lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  logsv_lga_clm_status_cbk_t *param = &msg->info.cbk_info.clm_node_status_cbk;
  uint8_t local_data[100];

  osafassert(uba != nullptr);

  p8 = ncs_dec_flatten_space(uba, local_data, 4);
  param->clm_node_status =
      static_cast<SaClmClusterChangesT>(ncs_decode_32bit(&p8));
  ncs_dec_skip_space(uba, 4);
  total_bytes += 4;

  return total_bytes;
}

/****************************************************************************
Name          : lga_dec_serverity_cbk_msg

Description   : This routine decodes message

Arguments     : NCS_UBAID *uba,
                LGSV_MSG *msg

Return Values : uint32_t

Notes         : None.
******************************************************************************/
static uint32_t lga_dec_serverity_cbk_msg(NCS_UBAID *uba, lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_cbk_info_t *cbk_infos = &msg->info.cbk_info;
  uint8_t local_data[100];

  osafassert(uba != nullptr);

  p8 = ncs_dec_flatten_space(uba, local_data, 6);
  cbk_infos->lgs_stream_id = ncs_decode_32bit(&p8);
  cbk_infos->serverity_filter_cbk.log_severity = ncs_decode_16bit(&p8);
  ncs_dec_skip_space(uba, 6);
  total_bytes += 6;

  return total_bytes;
}

/****************************************************************************
  Name          : lga_dec_lstr_open_sync_rsp_msg

  Description   : This routine decodes a log stream open sync response message

  Arguments     : NCS_UBAID *msg,
                  LGSV_MSG *msg

  Return Values : uint32_t

  Notes         : None.
******************************************************************************/
static uint32_t lga_dec_lstr_open_sync_rsp_msg(NCS_UBAID *uba,
                                               lgsv_msg_t *msg) {
  uint8_t *p8;
  uint32_t total_bytes = 0;
  lgsv_stream_open_rsp_t *param = &msg->info.api_resp_info.param.lstr_open_rsp;
  uint8_t local_data[100];

  osafassert(uba != nullptr);

  p8 = ncs_dec_flatten_space(uba, local_data, 4);
  param->lstr_id = ncs_decode_32bit(&p8);
  ncs_dec_skip_space(uba, 4);
  total_bytes += 4;

  return total_bytes;
}

/****************************************************************************
  Name          : lga_mds_dec

  Description   : This is a callback routine that is invoked to decode LGS
                  messages.

  Arguments     : pointer to struct ncsmds_callback_info

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
static uint32_t lga_mds_dec(struct ncsmds_callback_info *info) {
  uint8_t *p8;
  lgsv_msg_t *msg;
  NCS_UBAID *uba = info->info.dec.io_uba;
  uint8_t local_data[20];
  uint32_t total_bytes = 0;
  TRACE_ENTER();

  if (0 == m_NCS_MSG_FORMAT_IS_VALID(info->info.dec.i_msg_fmt_ver,
                                     LGA_WRT_LGS_SUBPART_VER_AT_MIN_MSG_FMT,
                                     LGA_WRT_LGS_SUBPART_VER_AT_MAX_MSG_FMT,
                                     LGA_WRT_LGS_MSG_FMT_ARRAY)) {
    TRACE("Invalid message format!!!\n");
    TRACE_LEAVE();
    return NCSCC_RC_FAILURE;
  }

  // Allocate a new msg in both sync/async cases
  if (nullptr ==
      (msg = static_cast<lgsv_msg_t *>(calloc(1, sizeof(lgsv_msg_t))))) {
    TRACE("calloc failed\n");
    return NCSCC_RC_FAILURE;
  }

  info->info.dec.o_msg = reinterpret_cast<uint8_t *>(msg);

  p8 = ncs_dec_flatten_space(uba, local_data, 4);
  msg->type = static_cast<lgsv_msg_type_t>(ncs_decode_32bit(&p8));
  ncs_dec_skip_space(uba, 4);
  total_bytes += 4;

  switch (msg->type) {
    case LGSV_LGA_API_RESP_MSG: {
      p8 = ncs_dec_flatten_space(uba, local_data, 8);
      msg->info.api_resp_info.type =
          static_cast<lgsv_api_resp_msg_type>(ncs_decode_32bit(&p8));
      msg->info.api_resp_info.rc =
          static_cast<SaAisErrorT>(ncs_decode_32bit(&p8));
      ncs_dec_skip_space(uba, 8);
      total_bytes += 8;
      TRACE_2("LGSV_LGA_API_RESP_MSG");

      switch (msg->info.api_resp_info.type) {
        case LGSV_INITIALIZE_RSP:
          total_bytes += lga_dec_initialize_rsp_msg(uba, msg);
          break;
        case LGSV_FINALIZE_RSP:
          total_bytes += lga_dec_finalize_rsp_msg(uba, msg);
          break;
        case LGSV_STREAM_OPEN_RSP:
          total_bytes += lga_dec_lstr_open_sync_rsp_msg(uba, msg);
          break;
        case LGSV_STREAM_CLOSE_RSP:
          total_bytes += lga_dec_lstr_close_rsp_msg(uba, msg);
          break;
        default:
          TRACE_2("Unknown API RSP type %d", msg->info.api_resp_info.type);
          break;
      }
    } break;
    case LGSV_LGS_CBK_MSG: {
      p8 = ncs_dec_flatten_space(uba, local_data, 16);
      msg->info.cbk_info.type =
          static_cast<lgsv_cbk_msg_type_t>(ncs_decode_32bit(&p8));
      msg->info.cbk_info.lgs_client_id = ncs_decode_32bit(&p8);
      msg->info.cbk_info.inv = ncs_decode_64bit(&p8);
      ncs_dec_skip_space(uba, 16);
      total_bytes += 16;
      TRACE_2("LGSV_LGS_CBK_MSG");
      switch (msg->info.cbk_info.type) {
        case LGSV_WRITE_LOG_CALLBACK_IND:
          TRACE_2("decode writelog message, lgs_client_id=%d",
                  msg->info.cbk_info.lgs_client_id);
          total_bytes += lga_dec_write_cbk_msg(uba, msg);
          break;
        case LGSV_CLM_NODE_STATUS_CALLBACK:
          TRACE_2("decode clm node status message, lgs_client_id=%d",
                  msg->info.cbk_info.lgs_client_id);
          total_bytes += lga_dec_clm_node_status_cbk_msg(uba, msg);
          break;
        case LGSV_SEVERITY_FILTER_CALLBACK:
          total_bytes += lga_dec_serverity_cbk_msg(uba, msg);
          TRACE_2(
              "decode severity filter message, lgs_client_id=%d"
              " lgs_stream_id=%d",
              msg->info.cbk_info.lgs_client_id,
              msg->info.cbk_info.lgs_stream_id);
          break;
        default:
          TRACE_2("Unknown callback type = %d!", msg->info.cbk_info.type);
          break;
      }
    } break;
    default:
      TRACE("Unknown MSG type %d", msg->type);
      break;
  }
  TRACE_LEAVE2("Total bytes: %d", total_bytes);
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 * Name          : lga_mds_enc_flat
 *
 * Description   : MDS encode and flatten
 *
 * Arguments     : pointer to ncsmds_callback_info
 *
 * Return Values : NCSCC_RC_SUCCESS/Error Code.
 *
 * Notes         : None.
 *****************************************************************************/
static uint32_t lga_mds_enc_flat(struct ncsmds_callback_info *info) {
  /* Modify the MDS_INFO to populate enc */
  info->info.enc = info->info.enc_flat;
  // Invoke the regular mds_enc routine
  return lga_mds_enc(info);
}

/****************************************************************************
 * Name          : lga_mds_dec_flat
 *
 * Description   : MDS decode and flatten
 *
 * Arguments     : pointer to ncsmds_callback_info
 *
 * Return Values : NCSCC_RC_SUCCESS/Error Code.
 *
 * Notes         : None.
 *****************************************************************************/
static uint32_t lga_mds_dec_flat(struct ncsmds_callback_info *info) {
  uint32_t rc;

  // Modify the MDS_INFO to populate dec
  info->info.dec = info->info.dec_flat;
  // Invoke the regular mds_dec routine
  rc = lga_mds_dec(info);
  if (rc != NCSCC_RC_SUCCESS) TRACE("lga_mds_dec rc = %d", rc);

  return rc;
}

/****************************************************************************
  Name          : lga_mds_cpy

  Description   : This function copies an events sent from LGS.

  Arguments     :pointer to struct ncsmds_callback_info

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
static uint32_t lga_mds_cpy(struct ncsmds_callback_info *info) {
  TRACE_ENTER();
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 * Name          : lga_mds_callback
 *
 * Description   : Callback Dispatcher for various MDS operations.
 *
 * Arguments     : cb   : LGS control Block pointer.
 *
 * Return Values : NCSCC_RC_SUCCESS/Error Code.
 *
 * Notes         : None.
 *****************************************************************************/

static uint32_t lga_mds_callback(struct ncsmds_callback_info *info) {
  static NCSMDS_CALLBACK_API cb_set[MDS_CALLBACK_SVC_MAX] = {
      lga_mds_cpy,      /* MDS_CALLBACK_COPY      0 */
      lga_mds_enc,      /* MDS_CALLBACK_ENC       1 */
      lga_mds_dec,      /* MDS_CALLBACK_DEC       2 */
      lga_mds_enc_flat, /* MDS_CALLBACK_ENC_FLAT  3 */
      lga_mds_dec_flat, /* MDS_CALLBACK_DEC_FLAT  4 */
      lga_mds_rcv,      /* MDS_CALLBACK_RECEIVE   5 */
      lga_mds_svc_evt   /* MDS_CALLBACK_SVC_EVENT 6 */
  };

  if (info->i_op <= MDS_CALLBACK_SVC_EVENT) {
    uint32_t rc = (*cb_set[info->i_op])(info);
    if (rc != NCSCC_RC_SUCCESS) TRACE("MDS_CALLBACK_SVC_EVENT not in range");

    return rc;
  } else {
    return NCSCC_RC_SUCCESS;
  }
}

/****************************************************************************
  Name          : lga_mds_init

  Description   : This routine registers the LGA Service with MDS.

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
uint32_t lga_mds_init() {
  NCSADA_INFO ada_info;
  NCSMDS_INFO mds_info;
  uint32_t rc = NCSCC_RC_SUCCESS;
  MDS_SVC_ID svc = NCSMDS_SVC_ID_LGS;
  std::atomic<MDS_HDL> &mds_hdl = LogAgent::instance()->atomic_get_mds_hdl();

  TRACE_ENTER();
  // Create the ADEST for LGA and get the pwe hdl
  memset(&ada_info, '\0', sizeof(ada_info));
  ada_info.req = NCSADA_GET_HDLS;

  if (NCSCC_RC_SUCCESS != (rc = ncsada_api(&ada_info))) {
    TRACE("NCSADA_GET_HDLS failed, rc = %d", rc);
    return NCSCC_RC_FAILURE;
  }

  // Store the info obtained from MDS ADEST creation
  mds_hdl = ada_info.info.adest_get_hdls.o_mds_pwe1_hdl;

  // Now install into mds
  memset(&mds_info, '\0', sizeof(NCSMDS_INFO));
  mds_info.i_mds_hdl = mds_hdl.load();
  mds_info.i_svc_id = NCSMDS_SVC_ID_LGA;
  mds_info.i_op = MDS_INSTALL;

  mds_info.info.svc_install.i_yr_svc_hdl = 0;
  mds_info.info.svc_install.i_install_scope = NCSMDS_SCOPE_NONE;
  mds_info.info.svc_install.i_svc_cb = lga_mds_callback;
  // LGA doesn't own the mds queue
  mds_info.info.svc_install.i_mds_q_ownership = false;
  mds_info.info.svc_install.i_mds_svc_pvt_ver = LGA_SVC_PVT_SUBPART_VERSION;

  if ((rc = ncsmds_api(&mds_info)) != NCSCC_RC_SUCCESS) {
    TRACE("mds api call failed");
    return NCSCC_RC_FAILURE;
  }

  // Now subscribe for events that will be generated by MDS
  memset(&mds_info, '\0', sizeof(NCSMDS_INFO));

  mds_info.i_mds_hdl = mds_hdl.load();
  mds_info.i_svc_id = NCSMDS_SVC_ID_LGA;
  mds_info.i_op = MDS_SUBSCRIBE;

  mds_info.info.svc_subscribe.i_scope = NCSMDS_SCOPE_NONE;
  mds_info.info.svc_subscribe.i_num_svcs = 1;
  mds_info.info.svc_subscribe.i_svc_ids = &svc;

  rc = ncsmds_api(&mds_info);
  if (rc != NCSCC_RC_SUCCESS) {
    TRACE("mds api call failed");
    return rc;
  }
  TRACE_LEAVE();
  return rc;
}

/****************************************************************************
  Name          : lga_mds_msg_sync_send

  Description   : This routine sends the LGA message to LGS. The send
                  operation is a synchronous call that
                  blocks until the response is received from LGS.

  Arguments     : i_msg - ptr to the LGSv message
                  o_msg - double ptr to LGSv message response

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE/timeout

  Notes         : None.
******************************************************************************/
uint32_t lga_mds_msg_sync_send(lgsv_msg_t *i_msg, lgsv_msg_t **o_msg,
                               SaTimeT timeout, uint32_t prio) {
  NCSMDS_INFO mds_info;
  uint32_t rc = NCSCC_RC_SUCCESS;
  std::atomic<MDS_HDL> &mds_hdl = LogAgent::instance()->atomic_get_mds_hdl();
  std::atomic<MDS_DEST> &lgs_mds_dest =
      LogAgent::instance()->atomic_get_lgs_mds_dest();

  TRACE_ENTER();

  osafassert(i_msg != nullptr && o_msg != nullptr);

  memset(&mds_info, '\0', sizeof(NCSMDS_INFO));
  mds_info.i_mds_hdl = mds_hdl.load();
  mds_info.i_svc_id = NCSMDS_SVC_ID_LGA;
  mds_info.i_op = MDS_SEND;

  // Fill the send structure
  mds_info.info.svc_send.i_msg = (NCSCONTEXT)i_msg;
  mds_info.info.svc_send.i_to_svc = NCSMDS_SVC_ID_LGS;
  mds_info.info.svc_send.i_sendtype = MDS_SENDTYPE_SNDRSP;
  // FIXME:
  mds_info.info.svc_send.i_priority = static_cast<MDS_SEND_PRIORITY_TYPE>(prio);
  // Fill the sub send rsp strcuture
  // FIXME: timeto wait in 10ms
  mds_info.info.svc_send.info.sndrsp.i_time_to_wait = timeout;
  mds_info.info.svc_send.info.sndrsp.i_to_dest = lgs_mds_dest.load();

  // Send the message
  if (NCSCC_RC_SUCCESS == (rc = ncsmds_api(&mds_info))) {
    // Retrieve the response and take ownership of the memory
    *o_msg =
        static_cast<lgsv_msg_t *>(mds_info.info.svc_send.info.sndrsp.o_rsp);
    mds_info.info.svc_send.info.sndrsp.o_rsp = nullptr;
  } else {
    TRACE("lga_mds_msg_sync_send FAILED: %u", rc);
  }

  TRACE_LEAVE();
  return rc;
}

/****************************************************************************
  Name          : lga_mds_msg_async_send

  Description   : This routine sends the LGA message to LGS.

  Arguments     : i_msg - ptr to the LGSv message

  Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

  Notes         : None.
******************************************************************************/
uint32_t lga_mds_msg_async_send(lgsv_msg_t *i_msg, uint32_t prio) {
  NCSMDS_INFO mds_info;
  std::atomic<MDS_HDL> &mds_hdl = LogAgent::instance()->atomic_get_mds_hdl();
  std::atomic<MDS_DEST> &lgs_mds_dest =
      LogAgent::instance()->atomic_get_lgs_mds_dest();

  TRACE_ENTER();
  osafassert(i_msg != nullptr);

  memset(&mds_info, '\0', sizeof(NCSMDS_INFO));
  mds_info.i_mds_hdl = mds_hdl.load();
  mds_info.i_svc_id = NCSMDS_SVC_ID_LGA;
  mds_info.i_op = MDS_SEND;

  // Fill the main send structure
  mds_info.info.svc_send.i_msg = (NCSCONTEXT)i_msg;
  mds_info.info.svc_send.i_priority = static_cast<MDS_SEND_PRIORITY_TYPE>(prio);
  mds_info.info.svc_send.i_to_svc = NCSMDS_SVC_ID_LGS;
  mds_info.info.svc_send.i_sendtype = MDS_SENDTYPE_SND;

  // Fill the sub send strcuture
  mds_info.info.svc_send.info.snd.i_to_dest = lgs_mds_dest.load();

  // Send the message
  uint32_t rc = ncsmds_api(&mds_info);
  if (rc != NCSCC_RC_SUCCESS) {
    TRACE("%s: async send failed", __func__);
  }

  TRACE_LEAVE();
  return rc;
}

/****************************************************************************
 * Name          : lga_msg_destroy
 *
 * Description   : This is the function which is called to destroy an LGSV msg.
 *
 * Arguments     : LGSV_MSG *.
 *
 * Return Values : NONE
 *
 * Notes         : None.
 *****************************************************************************/
void lga_msg_destroy(lgsv_msg_t *msg) {
  if (msg == nullptr) return;

  if (LGSV_LGA_API_MSG == msg->type) {
    TRACE("LGSV_LGA_API_MSG");
  }
  // There are no other pointers of the evt, so free the evt
  free(msg);
}
