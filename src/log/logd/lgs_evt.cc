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

#include "log/logd/lgs_evt.h"

#include <stdlib.h>
#include <cinttypes>
#include <string>
#include "base/osaf_time.h"
#include "base/osaf_extended_name.h"
#include "base/saf_error.h"
#include "log/logd/lgs_mbcsv_v1.h"
#include "log/logd/lgs_mbcsv_v2.h"
#include "log/logd/lgs_mbcsv_v6.h"
#include "log/logd/lgs_recov.h"
#include "log/logd/lgs_imm_gcfg.h"
#include "log/logd/lgs_clm.h"
#include "log/logd/lgs_dest.h"
#include "log/logd/lgs_oi_admin.h"
#include "log/logd/lgs_mbcsv.h"
#include "log/logd/lgs_cache.h"

void *client_db = nullptr; /* used for C++ STL map */

static uint32_t process_api_evt(lgsv_lgs_evt_t *evt);
static uint32_t proc_lga_updn_mds_msg(lgsv_lgs_evt_t *evt);
static uint32_t proc_mds_quiesced_ack_msg(lgsv_lgs_evt_t *evt);
static uint32_t proc_initialize_msg(lgs_cb_t *, lgsv_lgs_evt_t *evt);
static uint32_t proc_finalize_msg(lgs_cb_t *, lgsv_lgs_evt_t *evt);
static uint32_t proc_stream_open_msg(lgs_cb_t *, lgsv_lgs_evt_t *evt);
static uint32_t proc_stream_close_msg(lgs_cb_t *, lgsv_lgs_evt_t *evt);
static uint32_t proc_write_log_async_msg(lgs_cb_t *, lgsv_lgs_evt_t *evt);

static const LGSV_LGS_EVT_HANDLER lgs_lgsv_top_level_evt_dispatch_tbl[] = {
    process_api_evt, proc_lga_updn_mds_msg, proc_lga_updn_mds_msg,
    proc_mds_quiesced_ack_msg};

/* Dispatch table for LGA_API realted messages */
static const LGSV_LGS_LGA_API_MSG_HANDLER lgs_lga_api_msg_dispatcher[] = {
    proc_initialize_msg,   proc_finalize_msg,        proc_stream_open_msg,
    proc_stream_close_msg, proc_write_log_async_msg,
};

extern void rda_cb(uint32_t cb_hdl, PCS_RDA_CB_INFO *cb_info,
                   PCSRDA_RETURN_CODE error_code);
/**
 * Check if the log version is valid
 * @param version
 *
 * @return true if log version is valid
 */
static bool is_log_version_valid(const SaVersionT *version) {
  return ((version->releaseCode == LOG_RELEASE_CODE) &&
          (version->majorVersion <= LOG_MAJOR_VERSION) &&
          (version->minorVersion <= LOG_MINOR_VERSION));
}

/**
 * Name     : lgs_client_map_init
 * Description   : This routine is used to initialize the client_map
 * Arguments     : lgs_cb - pointer to the lgs Control Block
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE
 * Notes    : None
 */
uint32_t lgs_client_map_init() {
  TRACE_ENTER();
  if (client_db) {
    TRACE("Client Map already exists");
    return NCSCC_RC_FAILURE;
  }
  /*Client DB */
  client_db = new ClientMap;
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/**
 * Get client record from client ID
 * @param client_id
 *
 * @return log_client_t*
 */
log_client_t *lgs_client_get_by_id(uint32_t client_id) {
  log_client_t *rec = nullptr;

  /* Client DB */
  ClientMap *clientMap(reinterpret_cast<ClientMap *>(client_db));
  if (clientMap) {
    auto it = (clientMap->find(client_id));

    if (it != clientMap->end()) {
      rec = it->second;
    } else {
      TRACE("clm_node_id delete  failed : %x", client_id);
    }
  } else {
    TRACE("clm_node_id delete to map not exist failed : %x", client_id);
  }
  if (nullptr == rec) TRACE("client_id: %u lookup failed", client_id);

  return rec;
}

/**
 *
 * @param mds_dest
 * @param client_id set to zero if this is a new client
 * @param stream_list
 *
 * @return log_client_t*
 */
log_client_t *lgs_client_new(MDS_DEST mds_dest, uint32_t client_id,
                             lgs_stream_list_t *stream_list) {
  log_client_t *client;

  TRACE_ENTER2("MDS dest %" PRIx64, mds_dest);
  /* Client DB */
  ClientMap *clientMap(reinterpret_cast<ClientMap *>(client_db));
  if (client_id == 0) {
    lgs_cb->last_client_id++;
    if (lgs_cb->last_client_id == 0) lgs_cb->last_client_id++;
  }

  client = new log_client_t();

  if (nullptr == client) {
    LOG_WA("lgs_client_new calloc FAILED");
    goto done;
  }

  /** Initialize the record **/
  if ((lgs_cb->ha_state == SA_AMF_HA_STANDBY) ||
      (lgs_cb->ha_state == SA_AMF_HA_QUIESCED))
    lgs_cb->last_client_id = client_id;
  client->client_id = lgs_cb->last_client_id;
  client->mds_dest = mds_dest;
  client->stream_list_root = stream_list;

  if (clientMap) {
    std::pair<ClientMap::iterator, bool> p(
        clientMap->insert(std::make_pair(client->client_id, client)));

    if (!p.second) {
      TRACE("unable to add clm node info map - the id %x already existed",
            client->client_id);
      delete client;
      client = nullptr;
    }
  } else {
    TRACE("can't find local sec map in lgs_clm_node_add");
    delete client;
    client = nullptr;
  }

done:
  TRACE_LEAVE2("client_id %u", client_id);
  return client;
}

/**
 * Delete a client record and close all associated streams.
 * When closing streams open log files are closed.
 *
 * @param client_id[in]
 * @param stream_file_close_time_ptr[in]
 * @return uns32
 */
int lgs_client_delete(uint32_t client_id, time_t *closetime_ptr) {
  log_client_t *client;
  uint32_t status = 0;
  lgs_stream_list_t *cur_rec;
  time_t closetime = 0;
  struct timespec closetime_tspec;
  log_client_t *client_rec;

  TRACE_ENTER2("client_id %u", client_id);
  /* Client DB */
  ClientMap *clientMap(reinterpret_cast<ClientMap *>(client_db));
  /* Initiate close time value if not provided via closetime_ptr */
  if (closetime_ptr == nullptr) {
    osaf_clock_gettime(CLOCK_REALTIME, &closetime_tspec);
    closetime = closetime_tspec.tv_sec;
  } else {
    closetime = *closetime_ptr;
  }

  /* Get client data */
  if ((client = lgs_client_get_by_id(client_id)) == nullptr) {
    status = -1;
    goto done;
  }

  cur_rec = client->stream_list_root;
  while (nullptr != cur_rec) {
    lgs_stream_list_t *tmp_rec;
    log_stream_t *stream = log_stream_get_by_id(cur_rec->stream_id);
    TRACE_4("client_id: %u, REMOVE stream id: %u", client->client_id,
            cur_rec->stream_id);
    if (stream != nullptr) {
      log_stream_close(&stream, &closetime);
    }
    tmp_rec = cur_rec->next;
    free(cur_rec);
    cur_rec = tmp_rec;
  }

  if (clientMap) {
    auto it = (clientMap->find(client_id));

    if (it != clientMap->end()) {
      client_rec = it->second;
      clientMap->erase(it);
      delete client_rec;
    } else {
      TRACE("clm_node_id delete  failed : %x", client_id);
      status = -2;
    }
  } else {
    TRACE("clm_node_id delete to map not exist failed : %x", client_id);
    status = -2;
  }

done:
  TRACE_LEAVE();
  return status;
}

/**
 * Associate a stream with a client
 * @param client_id
 * @param stream_id
 *
 * @return int
 */
int lgs_client_stream_add(uint32_t client_id, uint32_t stream_id) {
  uint32_t rs = 0;
  log_client_t *client;
  lgs_stream_list_t *stream;

  TRACE_ENTER2("client_id %u, stream ID %u", client_id, stream_id);

  if ((client = lgs_client_get_by_id(client_id)) == nullptr) {
    rs = -1;
    goto err_exit;
  }

  stream = static_cast<lgs_stream_list_t *>(malloc(sizeof(lgs_stream_list_t)));
  if (stream == nullptr) {
    LOG_WA("malloc FAILED");
    rs = -1;
    goto err_exit;
  }

  stream->next = client->stream_list_root;
  stream->stream_id = stream_id;
  client->stream_list_root = stream;

err_exit:
  TRACE_LEAVE();
  return rs;
}

/**
 * Remove a stream association from a client
 * @param client_id
 * @param stream_id
 *
 * @return int
 */
int lgs_client_stream_rmv(uint32_t client_id, uint32_t stream_id) {
  int rc = 0;
  lgs_stream_list_t *cur_rec;
  lgs_stream_list_t *last_rec, *tmp_rec;
  log_client_t *client;

  TRACE_ENTER2("client_id %u, stream ID %u", client_id, stream_id);

  if ((client = lgs_client_get_by_id(client_id)) == nullptr) {
    rc = -1;
    goto done;
  }

  if (nullptr == client->stream_list_root) {
    rc = -2;
    goto done;
  }

  cur_rec = client->stream_list_root;
  last_rec = client->stream_list_root;
  do {
    if (stream_id == cur_rec->stream_id) {
      TRACE_4("client_id: %u, REMOVE stream id: %u", client_id,
              cur_rec->stream_id);
      tmp_rec = cur_rec->next;
      last_rec->next = tmp_rec;
      if (client->stream_list_root == cur_rec)
        client->stream_list_root = tmp_rec;
      free(cur_rec);
      break;
    }
    last_rec = cur_rec;
    cur_rec = last_rec->next;
  } while (nullptr != cur_rec);

done:
  TRACE_LEAVE();
  return rc;
}

/**
 * Search for a client that matches the MDS dest and delete all the associated
 * resources.
 * @param cb
 * @param mds_dest[in]
 * @param closetime_ptr[in]
 *
 * @return int
 */
int lgs_client_delete_by_mds_dest(MDS_DEST mds_dest, time_t *closetime_ptr) {
  uint32_t rc = 0;
  log_client_t *rp = nullptr;

  TRACE_ENTER2("mds_dest %" PRIx64, mds_dest);
  /* Loop through Client DB */
  ClientMap *clientMap(reinterpret_cast<ClientMap *>(client_db));
  for (auto it = clientMap->begin(); it != clientMap->end(); ) {
    rp = it->second;
    it++;
    if (m_NCS_MDS_DEST_EQUAL(&rp->mds_dest, &mds_dest))
      rc = lgs_client_delete(rp->client_id, closetime_ptr);
  }

  TRACE_LEAVE();
  return rc;
}

/****************************************************************************
 *
 * lgs_remove_lga_down_rec
 *
 *  Searches the LGA_DOWN_LIST for an entry whos MDS_DEST equals
 *  that passed in and removes the LGA rec.
 *
 * This routine is typically used to remove the lga down rec from standby
 * LGA_DOWN_LIST as  LGA client has gone away.
 *
 ****************************************************************************/
uint32_t lgs_remove_lga_down_rec(lgs_cb_t *cb, MDS_DEST mds_dest) {
  LGA_DOWN_LIST *lga_down_rec = cb->lga_down_list_head;
  LGA_DOWN_LIST *prev = nullptr;
  while (lga_down_rec) {
    if (m_NCS_MDS_DEST_EQUAL(&lga_down_rec->mds_dest, &mds_dest)) {
      /* Remove the LGA entry */
      /* Reset pointers */
      if (lga_down_rec == cb->lga_down_list_head) { /* 1st in the list? */
        if (lga_down_rec->next == nullptr) {        /* Only one in the list? */
          cb->lga_down_list_head = nullptr; /* Clear head sublist pointer */
          cb->lga_down_list_tail = nullptr; /* Clear tail sublist pointer */
        } else {                         /* 1st but not only one */
          cb->lga_down_list_head = lga_down_rec->next; /* Move next one up */
        }
      } else { /* Not 1st in the list */
        if (prev) {
          if (lga_down_rec->next == nullptr) cb->lga_down_list_tail = prev;
          prev->next = lga_down_rec->next; /* Link previous to next */
        }
      }

      /* Free the EDA_DOWN_REC */
      free(lga_down_rec);
      lga_down_rec = nullptr;
      break;
    }
    prev = lga_down_rec;               /* Remember address of this entry */
    lga_down_rec = lga_down_rec->next; /* Go to next entry */
  }
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 * Name          : proc_lga_updn_mds_msg
 *
 * Description   : This is the function which is called when lgs receives any
 *                 a LGA UP/DN message via MDS subscription.
 *
 * Arguments     : msg  - Message that was posted to the LGS Mail box.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE
 *
 * Notes         : None.
 *****************************************************************************/

static uint32_t proc_lga_updn_mds_msg(lgsv_lgs_evt_t *evt) {
  lgsv_ckpt_msg_v1_t ckpt_v1;
  lgsv_ckpt_msg_v2_t ckpt_v2;
  uint32_t async_rc = NCSCC_RC_SUCCESS;
  struct timespec closetime_tspec;

  TRACE_ENTER();

  switch (evt->evt_type) {
    case LGSV_LGS_EVT_LGA_UP:
      break;

    case LGSV_LGS_EVT_LGA_DOWN:
      TRACE("%s: LGSV_LGS_EVT_LGA_DOWN mds_dest = %" PRIx64, __FUNCTION__,
            evt->fr_dest);
      if ((lgs_cb->ha_state == SA_AMF_HA_ACTIVE) ||
          (lgs_cb->ha_state == SA_AMF_HA_QUIESCED)) {
        /* Remove this LGA entry from our processing lists */
        osaf_clock_gettime(CLOCK_REALTIME, &closetime_tspec);
        time_t closetime = closetime_tspec.tv_sec;
        (void)lgs_client_delete_by_mds_dest(evt->fr_dest, &closetime);
        /*Send an async checkpoint update to STANDBY EDS peer */
        if (lgs_cb->ha_state == SA_AMF_HA_ACTIVE) {
          void *ckpt_ptr;
          if (lgs_is_peer_v2()) {
            memset(&ckpt_v2, 0, sizeof(ckpt_v2));
            ckpt_v2.header.ckpt_rec_type = LGS_CKPT_CLIENT_DOWN;
            ckpt_v2.header.num_ckpt_records = 1;
            ckpt_v2.header.data_len = 1;
            ckpt_v2.ckpt_rec.agent_down.agent_dest = evt->fr_dest;
            ckpt_v2.ckpt_rec.agent_down.c_file_close_time_stamp = closetime;
            ckpt_ptr = &ckpt_v2;
          } else {
            memset(&ckpt_v1, 0, sizeof(ckpt_v1));
            ckpt_v1.header.ckpt_rec_type = LGS_CKPT_CLIENT_DOWN;
            ckpt_v1.header.num_ckpt_records = 1;
            ckpt_v1.header.data_len = 1;
            ckpt_v1.ckpt_rec.agent_dest = evt->fr_dest;
            ckpt_ptr = &ckpt_v1;
          }
          async_rc = lgs_ckpt_send_async(lgs_cb, ckpt_ptr, NCS_MBCSV_ACT_ADD);
          if (async_rc == NCSCC_RC_SUCCESS) {
            TRACE("ASYNC UPDATE SEND SUCCESS for LGA_DOWN event..");
          }
        }
      } else if (lgs_cb->ha_state == SA_AMF_HA_STANDBY) {
        LGA_DOWN_LIST *lga_down_rec = nullptr;
        if (lgs_lga_entry_valid(lgs_cb, evt->fr_dest)) {
          lga_down_rec =
              static_cast<LGA_DOWN_LIST *>(malloc(sizeof(LGA_DOWN_LIST)));
          if (nullptr == lga_down_rec) {
            /* Log it */
            LOG_WA("memory allocation for the LGA_DOWN_LIST failed");
            break;
          }
          memset(lga_down_rec, 0, sizeof(LGA_DOWN_LIST));
          lga_down_rec->mds_dest = evt->fr_dest;
          if (lgs_cb->lga_down_list_head == nullptr) {
            lgs_cb->lga_down_list_head = lga_down_rec;
          } else {
            if (lgs_cb->lga_down_list_tail)
              lgs_cb->lga_down_list_tail->next = lga_down_rec;
          }
          lgs_cb->lga_down_list_tail = lga_down_rec;
        }
      }
      break;

    default:
      TRACE("Unknown evt type!!!");
      break;
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 * Name          : proc_mds_quiesced_ack_msg
 *
 * Description   : This is the function which is called when lgs receives an
 *                       quiesced ack event from MDS
 *
 * Arguments     : evt  - Message that was posted to the LGS Mail box.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE
 *
 * Notes         : None.
 *****************************************************************************/
static uint32_t proc_mds_quiesced_ack_msg(lgsv_lgs_evt_t *evt) {
  TRACE_ENTER();

  if (lgs_cb->is_quiesced_set == true) {
    /* Update control block */
    lgs_cb->is_quiesced_set = false;
    lgs_cb->ha_state = SA_AMF_HA_QUIESCED;

    /* Inform MBCSV of HA state change */
    if (lgs_mbcsv_change_HA_state(lgs_cb, lgs_cb->ha_state) != NCSCC_RC_SUCCESS)
      TRACE("lgs_mbcsv_change_HA_state FAILED");

    /* Finally respond to AMF */
    saAmfResponse(lgs_cb->amf_hdl, lgs_cb->amf_invocation_id, SA_AIS_OK);
  } else {
    LOG_ER("Received LGSV_EVT_QUIESCED_ACK message but is_quiesced_set==false");
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/**
 * Clear any pending lga_down records
 *
 */
static void lgs_process_lga_down_list() {
  struct timespec closetime_tspec;
  if (lgs_cb->ha_state == SA_AMF_HA_ACTIVE) {
    LGA_DOWN_LIST *temp_lga_down_rec = nullptr;
    osaf_clock_gettime(CLOCK_REALTIME, &closetime_tspec);
    time_t closetime = closetime_tspec.tv_sec;

    LGA_DOWN_LIST *lga_down_rec = lgs_cb->lga_down_list_head;
    while (lga_down_rec) {
      /*Remove the LGA DOWN REC from the LGA_DOWN_LIST */
      /* Free the LGA_DOWN_REC */
      /* Remove this LGA entry from our processing lists */
      temp_lga_down_rec = lga_down_rec;
      (void)lgs_client_delete_by_mds_dest(lga_down_rec->mds_dest, &closetime);
      lga_down_rec = lga_down_rec->next;
      free(temp_lga_down_rec);
    }
    lgs_cb->lga_down_list_head = nullptr;
    lgs_cb->lga_down_list_tail = nullptr;
  }
}

static uint32_t proc_rda_cb_msg(lgsv_lgs_evt_t *evt) {
  uint32_t rc = NCSCC_RC_SUCCESS;

  TRACE_ENTER2("%d", (int)evt->info.rda_info.io_role);

  if ((rc = initialize_for_assignment(
           lgs_cb, (SaAmfHAStateT)evt->info.rda_info.io_role)) !=
      NCSCC_RC_SUCCESS) {
    LOG_ER("initialize_for_assignment FAILED %u", (unsigned)rc);
    exit(EXIT_FAILURE);
  }

  if (evt->info.rda_info.io_role == PCS_RDA_ACTIVE &&
      lgs_cb->ha_state != SA_AMF_HA_ACTIVE) {
    LOG_NO("ACTIVE request");
    lgs_cb->mds_role = V_DEST_RL_ACTIVE;
    lgs_cb->ha_state = SA_AMF_HA_ACTIVE;

    if ((rc = lgs_mds_change_role(lgs_cb)) != NCSCC_RC_SUCCESS) {
      LOG_ER("lgs_mds_change_role FAILED %u", rc);
      exit(EXIT_FAILURE);
    }

    if ((rc = lgs_mbcsv_change_HA_state(lgs_cb, lgs_cb->ha_state)) !=
        NCSCC_RC_SUCCESS) {
      LOG_ER("lgs_mbcsv_change_HA_state FAILED %u", rc);
      exit(EXIT_FAILURE);
    }

    /* fail over, become implementer */
    lgsOiCreateBackground();
    lgs_start_gcfg_applier();

    /* Agent down list has to be processed first */
    lgs_process_lga_down_list();

    /* Check existing streams */
    // Iterate all existing log streams in cluster.
    uint32_t count = 0;
    log_stream_t *stream;
    SaBoolT endloop = SA_FALSE, jstart = SA_TRUE;
    while ((stream = iterate_all_streams(endloop, jstart)) && !endloop) {
      jstart = SA_FALSE;
      *stream->p_fd = -1; /* Initialize fd */
      count++;
    }

    if (count == 0) LOG_ER("No streams exist!");
  }

  TRACE_LEAVE();
  return rc;
}

/****************************************************************************
 * Name          : lgs_cb_init
 *
 * Description   : This function initializes the LGS_CB including the
 *                 Maps.
 *
 *
 * Arguments     : lgs_cb * - Pointer to the LGS_CB.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * Notes         : None.
 *****************************************************************************/
uint32_t lgs_cb_init(lgs_cb_t *lgs_cb) {
  uint32_t rc = NCSCC_RC_SUCCESS;

  TRACE_ENTER();

  lgs_cb->fully_initialized = false;
  lgs_cb->amfSelectionObject = -1;
  lgs_cb->mbcsv_sel_obj = -1;
  lgs_cb->clm_hdl = 0;
  lgs_cb->clmSelectionObject = -1;

  /* Assign Version. Currently, hardcoded, This will change later */
  lgs_cb->log_version.releaseCode = LOG_RELEASE_CODE;
  lgs_cb->log_version.majorVersion = LOG_MAJOR_VERSION;
  lgs_cb->log_version.minorVersion = LOG_MINOR_VERSION;

  if ((rc = rda_register_callback(0, rda_cb, &lgs_cb->ha_state))
      != NCSCC_RC_SUCCESS) {
    LOG_ER("rda_register_callback FAILED %u", rc);
    goto done;
  }

  /* Initialize CLM Node map*/
  if (NCSCC_RC_SUCCESS != lgs_client_map_init()) {
    LOG_ER("LGS: CLM Client map_init FAILED");
    rc = NCSCC_RC_FAILURE;
  }

  /* Initialize CLM Node map*/
  if (NCSCC_RC_SUCCESS != lgs_clm_node_map_init(lgs_cb)) {
    LOG_ER("LGS: CLM Node map_init FAILED");
    rc = NCSCC_RC_FAILURE;
  }

done:
  TRACE_LEAVE();
  return rc;
}

/**
 * Initialized client checkpointing
 * @param cb
 * @param mds_dest
 * @param client
 * @return
 */
static uint32_t lgs_ckpt_initialized_client(lgs_cb_t *cb, MDS_DEST mds_dest,
                                            log_client_t *client) {
  uint32_t async_rc = NCSCC_RC_SUCCESS;
  lgsv_ckpt_msg_v1_t ckpt_v1;
  lgsv_ckpt_msg_v6_t ckpt_v6;
  void *ckpt_ptr = nullptr;
  lgsv_ckpt_header_t *header_ptr = nullptr;

  TRACE_ENTER();

  if (cb->ha_state == SA_AMF_HA_ACTIVE) {
    if (lgs_is_peer_v6()) {
      lgs_ckpt_initialize_msg_v6_t *ckpt_rec_ptr;
      memset(&ckpt_v6, 0, sizeof(ckpt_v6));
      header_ptr = &ckpt_v6.header;
      ckpt_rec_ptr = &ckpt_v6.ckpt_rec.initialize_client;
      ckpt_ptr = &ckpt_v6;

      ckpt_rec_ptr->client_id = client->client_id;
      ckpt_rec_ptr->mds_dest = mds_dest;
      ckpt_rec_ptr->client_ver = client->client_ver;
    } else {
      lgs_ckpt_initialize_msg_t *ckpt_rec_ptr;
      memset(&ckpt_v1, 0, sizeof(ckpt_v1));
      header_ptr = &ckpt_v1.header;
      ckpt_rec_ptr = &ckpt_v1.ckpt_rec.initialize_client;
      ckpt_ptr = &ckpt_v1;

      ckpt_rec_ptr->client_id = client->client_id;
      ckpt_rec_ptr->mds_dest = mds_dest;
    }

    header_ptr->ckpt_rec_type = LGS_CKPT_CLIENT_INITIALIZE;
    header_ptr->num_ckpt_records = 1;
    header_ptr->data_len = 1;
    async_rc = lgs_ckpt_send_async(cb, ckpt_ptr, NCS_MBCSV_ACT_ADD);
    if (async_rc == NCSCC_RC_SUCCESS) {
      TRACE_4("ASYNC UPDATE SEND SUCCESS for INITIALIZE ..");
    }
  }

  TRACE_LEAVE2("async_rc = %d", async_rc);
  return async_rc;
}

/**
 * Handle a initialize message
 * @param cb
 * @param evt
 *
 * @return uns32
 */
static uint32_t proc_initialize_msg(lgs_cb_t *cb, lgsv_lgs_evt_t *evt) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  SaAisErrorT ais_rc = SA_AIS_OK;
  SaVersionT *version;
  lgsv_msg_t msg;
  log_client_t *client = nullptr;

  TRACE_ENTER2("dest %" PRIx64, evt->fr_dest);

  // Client should try again when role changes is in transition.
  if (cb->is_quiesced_set) {
    TRACE("Log service is in quiesced state");
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    goto snd_rsp;
  }

  /* Validate the version */
  version = &(evt->info.msg.info.api_info.param.init.version);
  if (!is_log_version_valid(version)) {
    ais_rc = SA_AIS_ERR_VERSION;
    TRACE("version FAILED");
    goto snd_rsp;
  }

  if (is_client_clm_member(evt->fr_node_id, version) != true) {
    ais_rc = SA_AIS_ERR_UNAVAILABLE;
    TRACE("client not a CLM member FAILED");
    goto snd_rsp;
  }

  if ((client = lgs_client_new(evt->fr_dest, 0, nullptr)) == nullptr) {
    ais_rc = SA_AIS_ERR_NO_MEMORY;
    goto snd_rsp;
  }

  client->client_ver = *version;

  /* Checkpoint */
  lgs_ckpt_initialized_client(cb, evt->fr_dest, client);

snd_rsp:
  msg.type = LGSV_LGA_API_RESP_MSG;
  msg.info.api_resp_info.type = LGSV_INITIALIZE_RSP;
  msg.info.api_resp_info.rc = ais_rc;
  msg.info.api_resp_info.param.init_rsp.client_id =
      client ? client->client_id : 0;
  rc = lgs_mds_msg_send(cb, &msg, &evt->fr_dest, &evt->mds_ctxt,
                        MDS_SEND_PRIORITY_HIGH);

  TRACE_LEAVE2("client_id %u", client ? client->client_id : 0);
  return rc;
}

/**
 * Handle a finalize message
 * @param cb
 * @param evt
 *
 * @return uns32
 */
static uint32_t proc_finalize_msg(lgs_cb_t *cb, lgsv_lgs_evt_t *evt) {
  int rc;
  uint32_t client_id = evt->info.msg.info.api_info.param.finalize.client_id;
  lgsv_msg_t msg;
  SaAisErrorT ais_rc = SA_AIS_OK;
  lgsv_ckpt_msg_v1_t ckpt_v1;
  lgsv_ckpt_msg_v2_t ckpt_v2;
  void *ckpt_ptr;
  struct timespec closetime_tspec;
  time_t closetime;

  TRACE_ENTER2("client_id %u", client_id);

  // Client should try again when role changes is in transition.
  if (cb->is_quiesced_set) {
    TRACE("Log service is in quiesced state");
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    goto snd_rsp;
  }

  /* Free all resources allocated by this client. */
  osaf_clock_gettime(CLOCK_REALTIME, &closetime_tspec);
  closetime = closetime_tspec.tv_sec;
  if ((rc = lgs_client_delete(client_id, &closetime)) != 0) {
    TRACE("lgs_client_delete FAILED: %d", rc);
    ais_rc = SA_AIS_ERR_BAD_HANDLE;
    goto snd_rsp;
  }

  if (lgs_is_peer_v2()) {
    memset(&ckpt_v2, 0, sizeof(ckpt_v2));
    ckpt_v2.header.ckpt_rec_type = LGS_CKPT_CLIENT_FINALIZE;
    ckpt_v2.header.num_ckpt_records = 1;
    ckpt_v2.header.data_len = 1;
    ckpt_v2.ckpt_rec.finalize_client.client_id = client_id;
    ckpt_v2.ckpt_rec.finalize_client.c_file_close_time_stamp =
        static_cast<SaTimeT>(closetime);
    ckpt_ptr = &ckpt_v2;
  } else {
    memset(&ckpt_v1, 0, sizeof(ckpt_v1));
    ckpt_v1.header.ckpt_rec_type = LGS_CKPT_CLIENT_FINALIZE;
    ckpt_v1.header.num_ckpt_records = 1;
    ckpt_v1.header.data_len = 1;
    ckpt_v1.ckpt_rec.finalize_client.client_id = client_id;
    ckpt_ptr = &ckpt_v1;
  }
  (void)lgs_ckpt_send_async(lgs_cb, ckpt_ptr, NCS_MBCSV_ACT_RMV);

snd_rsp:
  msg.type = LGSV_LGA_API_RESP_MSG;
  msg.info.api_resp_info.type = LGSV_FINALIZE_RSP;
  msg.info.api_resp_info.rc = ais_rc;
  msg.info.api_resp_info.param.finalize_rsp.client_id = client_id;
  rc = lgs_mds_msg_send(cb, &msg, &evt->fr_dest, &evt->mds_ctxt,
                        MDS_SEND_PRIORITY_HIGH);

  TRACE_LEAVE2("ais_rc: %u", ais_rc);
  return rc;
}

/**
 * Create a new application stream
 *
 * @param open_sync_param[in] Parameters used to create the stream
 * @param o_stream[out]       The created stream
 *
 * @return AIS return code
 */
SaAisErrorT create_new_app_stream(lgsv_stream_open_req_t *open_sync_param,
                                  log_stream_t **o_stream) {
  SaAisErrorT ais_rc = SA_AIS_OK;
  log_stream_t *stream;
  SaBoolT twelveHourModeFlag;
  SaUint32T logMaxLogrecsize_conf = 0;
  SaConstStringT str_name;
  int err = 0;
  SaBoolT endloop = SA_FALSE, jstart = SA_TRUE;
  const char *dnPrefix = "safLgStr=";

  TRACE_ENTER();

  if (lgs_is_extended_name_valid(&open_sync_param->lstr_name) == false) {
    TRACE("SaNameT is invalid");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  if (open_sync_param->logFileFullAction != SA_LOG_FILE_FULL_ACTION_ROTATE) {
    TRACE("Unsupported logFileFullAction");
    ais_rc = SA_AIS_ERR_NOT_SUPPORTED;
    goto done;
  }

  /* Validate maxFilesRotated just in case of SA_LOG_FILE_FULL_ACTION_ROTATE
   * type */
  if ((open_sync_param->logFileFullAction == SA_LOG_FILE_FULL_ACTION_ROTATE) &&
      ((open_sync_param->maxFilesRotated < 1) ||
       (open_sync_param->maxFilesRotated > 127))) {
    TRACE("Invalid maxFilesRotated. Valid Range = [1-127]");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  if (open_sync_param->logFileFmt == nullptr) {
    TRACE("logFileFmt is NULL, use default one");
    const char *logFileFormat =
        static_cast<const char *>(lgs_cfg_get(LGS_IMM_LOG_STREAM_FILE_FORMAT));
    open_sync_param->logFileFmt = strdup(logFileFormat);
  }

  /* Check the format string */
  if (!lgs_is_valid_format_expression(open_sync_param->logFileFmt,
                                      STREAM_TYPE_APPLICATION_RT,
                                      &twelveHourModeFlag)) {
    TRACE("format expression failure");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  /* Verify if there is any special character in logFileName */
  if (lgs_has_special_char(open_sync_param->logFileName) == true) {
    TRACE("Invalid logFileName - %s", open_sync_param->logFileName);
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  /* Verify if logFileName length is valid */
  if (lgs_is_valid_filelength(open_sync_param->logFileName) == false) {
    TRACE("logFileName is invalid");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  /* Verify if logFilePathName length is valid */
  if (lgs_is_valid_pathlength(open_sync_param->logFilePathName,
                              open_sync_param->logFileName) == false) {
    TRACE("logFilePathName is invalid");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  /* Verify that path and file are unique */
  // Iterate all existing log streams in cluster.
  while ((stream = iterate_all_streams(endloop, jstart)) && !endloop) {
    jstart = SA_FALSE;
    if ((stream->fileName == open_sync_param->logFileName) &&
        (stream->pathName == open_sync_param->logFilePathName)) {
      TRACE("pathname already exist");
      ais_rc = SA_AIS_ERR_INVALID_PARAM;
      goto done;
    }
  }

  /* Verify that the name seems to be a DN */
  str_name = osaf_extended_name_borrow(&open_sync_param->lstr_name);
  if (strncmp(dnPrefix, str_name, strlen(dnPrefix)) != 0) {
    TRACE("'%s' is not a valid stream name => invalid param", str_name);
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  /* Verify that the fixedLogRecordSize is in valid range */
  logMaxLogrecsize_conf =
      *static_cast<const SaUint32T *>(lgs_cfg_get(LGS_IMM_LOG_MAX_LOGRECSIZE));
  if ((open_sync_param->maxLogRecordSize != 0) &&
      ((open_sync_param->maxLogRecordSize < SA_LOG_MIN_RECORD_SIZE) ||
       (open_sync_param->maxLogRecordSize > logMaxLogrecsize_conf))) {
    TRACE("maxLogRecordSize is invalid");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    goto done;
  }

  *o_stream = log_stream_new(str_name, STREAM_NEW);
  if (*o_stream == nullptr) {
    ais_rc = SA_AIS_ERR_NO_MEMORY;
    goto done;
  }

  err = lgs_populate_log_stream(
      open_sync_param->logFileName, open_sync_param->logFilePathName,
      open_sync_param->maxLogFileSize, open_sync_param->maxLogRecordSize,
      open_sync_param->logFileFullAction, open_sync_param->maxFilesRotated,
      open_sync_param->logFileFmt, STREAM_TYPE_APPLICATION_RT,
      twelveHourModeFlag, 0, *o_stream);  // output
  if (err == -1) {
    log_stream_delete(o_stream);
    ais_rc = SA_AIS_ERR_NO_MEMORY;
    goto done;
  }

  ais_rc = lgs_create_appstream_rt_object(*o_stream);
  if (ais_rc != SA_AIS_OK) log_stream_delete(o_stream);

  TRACE("%s: lgs_create_appstream_rt_object return, %s",
         __FUNCTION__, saf_error(ais_rc));

done:
  TRACE_LEAVE();
  return ais_rc;
}

/**
 * Compare existing stream attributes with open request
 * @param open_sync_param
 * @param applicationStream
 * @return
 */
static SaAisErrorT file_attribute_cmp(lgsv_stream_open_req_t *open_sync_param,
                                      log_stream_t *applicationStream) {
  SaAisErrorT rs = SA_AIS_OK;

  TRACE_ENTER2("Stream: %s", applicationStream->name.c_str());

  if (open_sync_param->maxLogFileSize != applicationStream->maxLogFileSize ||
      open_sync_param->maxLogRecordSize !=
          applicationStream->fixedLogRecordSize ||
      open_sync_param->maxFilesRotated != applicationStream->maxFilesRotated) {
    TRACE(
        "create params differ, new: mfs %llu, mlrs %u , mr %u"
        " existing: mfs %llu, mlrs %u , mr %u",
        open_sync_param->maxLogFileSize, open_sync_param->maxLogRecordSize,
        open_sync_param->maxFilesRotated, applicationStream->maxLogFileSize,
        applicationStream->fixedLogRecordSize,
        applicationStream->maxFilesRotated);
    rs = SA_AIS_ERR_EXIST;
  } else if (applicationStream->logFullAction !=
             open_sync_param->logFileFullAction) {
    TRACE("logFileFullAction create params differs, new: %d, old: %d",
          open_sync_param->logFileFullAction, applicationStream->logFullAction);
    rs = SA_AIS_ERR_EXIST;
  } else if (applicationStream->fileName != open_sync_param->logFileName) {
    TRACE("logFileName differs, new: %s existing: %s",
          open_sync_param->logFileName, applicationStream->fileName.c_str());
    rs = SA_AIS_ERR_EXIST;
  } else if (applicationStream->pathName != open_sync_param->logFilePathName) {
    TRACE("log file path differs, new: %s existing: %s",
          open_sync_param->logFilePathName,
          applicationStream->pathName.c_str());
    rs = SA_AIS_ERR_EXIST;
  } else if ((open_sync_param->logFileFmt != nullptr) &&
             strcmp(applicationStream->logFileFormat,
                    open_sync_param->logFileFmt) != 0) {
    TRACE("logFile format differs, new: %s existing: %s",
          open_sync_param->logFileFmt, applicationStream->logFileFormat);
    rs = SA_AIS_ERR_EXIST;
  }

  TRACE_LEAVE();
  return rs;
}

/****************************************************************************
 * Name          : proc_stream_open_msg
 *
 * Description   : This is the function which is called when lgs receives a
 *                 LGSV_LGA_LSTR_OPEN (log stream open) message.
 *
 * Arguments     : msg  - Message that was posted to the LGS Mail box.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE
 *
 * Notes         : None.
 *****************************************************************************/

static uint32_t proc_stream_open_msg(lgs_cb_t *cb, lgsv_lgs_evt_t *evt) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  SaAisErrorT ais_rv = SA_AIS_OK;
  uint32_t lstr_id = (uint32_t)-1;
  lgsv_msg_t msg;
  lgsv_stream_open_req_t *open_sync_param =
      &(evt->info.msg.info.api_info.param.lstr_open_sync);
  log_stream_t *logStream;
  std::string name;
  time_t file_closetime = 0;
  int i_rc = 0;

  /* Create null-terminated stream name */
  name = osaf_extended_name_borrow(&open_sync_param->lstr_name);

  TRACE_ENTER2("stream '%s', client_id %u", name.c_str(),
               open_sync_param->client_id);

  // Client should try again when role changes is in transition.
  if (cb->is_quiesced_set) {
    TRACE("Log service is in quiesced state");
    ais_rv = SA_AIS_ERR_TRY_AGAIN;
    goto snd_rsp;
  }

  logStream = log_stream_get_by_name(name);
  if (logStream != nullptr) {
    TRACE("existing stream - id %u", logStream->streamId);
    if (logStream->streamType == STREAM_TYPE_APPLICATION_RT) {
      /* Verify the creation attributes for an existing appl. stream */
      if (open_sync_param->lstr_open_flags & SA_LOG_STREAM_CREATE) {
        ais_rv = file_attribute_cmp(open_sync_param, logStream);
        if (ais_rv != SA_AIS_OK) goto snd_rsp;
      }
    } else {
      /* One of the well-known log streams */
      if (open_sync_param->lstr_open_flags & SA_LOG_STREAM_CREATE) {
        ais_rv = SA_AIS_ERR_INVALID_PARAM;
        rc = NCSCC_RC_FAILURE;
        goto snd_rsp;
      }
    }
  } else {
    /* Stream does not exist. Create a new stream */

    /*
     * Check if the stream is in the list of stream objects
     * not recovered. If in list, recover the stream and
     * add it to the client
     */
    if (lgs_cb->lgs_recovery_state == LGS_RECOVERY) {
      TRACE("%s LGS_RECOVERY", __FUNCTION__);
      i_rc = lgs_restore_one_app_stream(name, open_sync_param->client_id,
                                        &logStream);
      if (i_rc == -1) {
        // Not find the opening stream in recovery database.
        // Goto next for checking other inputs.
        goto next;
      }

      TRACE("%s Stream %s is recovered", __FUNCTION__, name.c_str());
      log_stream_print(logStream); /* TRACE */
      lstr_id = logStream->streamId;
      goto snd_rsp;
    }

  next:
    if ((open_sync_param->lstr_open_flags & SA_LOG_STREAM_CREATE) == 0) {
      /* Trying to open a non existing stream */
      TRACE("%s Attempt to open not existing stream", __FUNCTION__);
      ais_rv = SA_AIS_ERR_NOT_EXIST;
      goto snd_rsp;
    }

    if (check_max_stream()) {
      TRACE("The number of stream out of limitation");
      ais_rv = SA_AIS_ERR_NO_RESOURCES;
      goto snd_rsp;
    }

    /* Create the stream:
     *  - Check parameters
     *  - Create the stream in the stream "data base"
     *  - If active create IMM runtime object
     *
     * Note: Files are not created here
     */
    ais_rv = create_new_app_stream(open_sync_param, &logStream);
    if (ais_rv != SA_AIS_OK) {
      TRACE("%s create_new_app_stream Fail \"%s\"", __FUNCTION__,
            saf_error(ais_rv));
      if (ais_rv == SA_AIS_ERR_BAD_HANDLE) {
        // This means that the stream RT object could not be created because
        // of a bad OI handle. Change to TRY AGAIN in the reply to the agent
        ais_rv = SA_AIS_ERR_TRY_AGAIN;
      }
      goto snd_rsp;
    }
  }

  /* Create the log files:
   * - Only if opened for the first time
   * - Relative directory is created if not exist
   * - Config file is created
   * - If max number of log files with same stream name exists the oldest
   *   file is removed (rotation)
   *
   * Note: No error if the files could not be created. A new attempt will
   *       be done when trying to write to the stream.
   */
  log_stream_open_fileinit(logStream);

  log_stream_print(logStream); /* TRACE */

  /* Create an association between this client and the stream */
  rc = lgs_client_stream_add(open_sync_param->client_id, logStream->streamId);
  if (rc != 0) {
    log_stream_close(&logStream, &file_closetime);
    ais_rv = SA_AIS_ERR_TRY_AGAIN;
    goto snd_rsp;
  }

  TRACE_4("logStream->streamId = %u is created", logStream->streamId);
  lstr_id = logStream->streamId;

snd_rsp:
  memset(&msg, 0, sizeof(lgsv_msg_t));
  msg.type = LGSV_LGA_API_RESP_MSG;
  msg.info.api_resp_info.type = LGSV_STREAM_OPEN_RSP;
  msg.info.api_resp_info.rc = ais_rv;
  msg.info.api_resp_info.param.lstr_open_rsp.lstr_id = lstr_id;
  TRACE_4("lstr_open_rsp.lstr_id: %u, rv: %u", lstr_id, ais_rv);
  rc = lgs_mds_msg_send(cb, &msg, &evt->fr_dest, &evt->mds_ctxt,
                        MDS_SEND_PRIORITY_HIGH);

  // Checkpoint the opened stream
  if (ais_rv == SA_AIS_OK) {
    lgs_ckpt_stream_open(logStream, open_sync_param->client_id);
  }

  // These memories are allocated in MDS log open decode callback.
  free(open_sync_param->logFileFmt);
  free(open_sync_param->logFilePathName);
  free(open_sync_param->logFileName);
  osaf_extended_name_free(&open_sync_param->lstr_name);

  TRACE_LEAVE();
  return rc;
}

/**
 * Handle a stream close message
 * @param cb
 * @param evt
 *
 * @return uns32
 */
static uint32_t proc_stream_close_msg(lgs_cb_t *cb, lgsv_lgs_evt_t *evt) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  lgsv_stream_close_req_t *close_param =
      &(evt->info.msg.info.api_info.param.lstr_close);
  log_stream_t *stream;
  lgsv_msg_t msg;
  SaAisErrorT ais_rc = SA_AIS_OK;
  uint32_t streamId;
  time_t closetime = 0;
  lgsv_ckpt_msg_v2_t ckpt_v1;
  lgsv_ckpt_msg_v2_t ckpt_v2;
  void *ckpt_ptr;

  TRACE_ENTER2("client_id %u, stream ID %u", close_param->client_id,
               close_param->lstr_id);

  // Client should try again when role changes is in transition.
  if (cb->is_quiesced_set) {
    TRACE("Log service is in quiesced state");
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    goto snd_rsp;
  }

  if ((stream = log_stream_get_by_id(close_param->lstr_id)) == nullptr) {
    TRACE("Bad stream ID");
    ais_rc = SA_AIS_ERR_BAD_HANDLE;
    goto snd_rsp;
  }

  // This check is to avoid the client getting SA_AIS_BAD_OPERATION
  // as there is no IMM OI implementer set.
  if ((stream->streamType == STREAM_TYPE_APPLICATION_RT) &&
      (lgsGetOiHandle() == 0)) {
    TRACE("IMM service unavailable, close stream failed");
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    goto snd_rsp;
  }

  if (lgs_client_stream_rmv(close_param->client_id, close_param->lstr_id) !=
      0) {
    TRACE("Bad client or stream ID");
    ais_rc = SA_AIS_ERR_BAD_HANDLE;
    goto snd_rsp;
  }

  streamId = stream->streamId;
  log_stream_close(&stream, &closetime);

  /* Checkpointing */
  if (lgs_is_peer_v2()) {
    memset(&ckpt_v2, 0, sizeof(ckpt_v2));
    ckpt_v2.header.ckpt_rec_type = LGS_CKPT_CLOSE_STREAM;
    ckpt_v2.header.num_ckpt_records = 1;
    ckpt_v2.header.data_len = 1;
    ckpt_v2.ckpt_rec.stream_close.clientId = close_param->client_id;
    ckpt_v2.ckpt_rec.stream_close.streamId = streamId;
    ckpt_v2.ckpt_rec.stream_close.c_file_close_time_stamp = closetime;
    ckpt_ptr = &ckpt_v2;
  } else {
    memset(&ckpt_v1, 0, sizeof(ckpt_v1));
    ckpt_v1.header.ckpt_rec_type = LGS_CKPT_CLOSE_STREAM;
    ckpt_v1.header.num_ckpt_records = 1;
    ckpt_v1.header.data_len = 1;
    ckpt_v1.ckpt_rec.stream_close.clientId = close_param->client_id;
    ckpt_v1.ckpt_rec.stream_close.streamId = streamId;
    ckpt_ptr = &ckpt_v1;
  }

  (void)lgs_ckpt_send_async(cb, ckpt_ptr, NCS_MBCSV_ACT_RMV);

snd_rsp:
  msg.type = LGSV_LGA_API_RESP_MSG;
  msg.info.api_resp_info.type = LGSV_STREAM_CLOSE_RSP;
  msg.info.api_resp_info.rc = ais_rc;
  msg.info.api_resp_info.param.close_rsp.client_id = close_param->client_id;
  msg.info.api_resp_info.param.close_rsp.lstr_id = close_param->lstr_id;
  rc = lgs_mds_msg_send(cb, &msg, &evt->fr_dest, &evt->mds_ctxt,
                        MDS_SEND_PRIORITY_HIGH);
  TRACE_LEAVE2("ais_rc: %u", ais_rc);
  return rc;
}

/****************************************************************************
 * Name          : proc_write_log_async_msg
 *
 * Description   : This is the function which is called when lgs receives a
 *                 LGSV_LGA_WRITE_LOG_ASYNC message.
 *
 * Arguments     : msg  - Message that was posted to the Mail box.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE
 *
 * Notes         : None.
 *****************************************************************************/
static uint32_t proc_write_log_async_msg(lgs_cb_t *cb, lgsv_lgs_evt_t *evt) {
  lgsv_write_log_async_req_t *param =
      &(evt->info.msg.info.api_info.param).write_log_async;
  log_stream_t *stream = nullptr;
  SaStringT logOutputString = nullptr;
  SaUint32T buf_size;
  int n = 0;
  uint32_t max_logrecsize = 0;
  char node_name[_POSIX_HOST_NAME_MAX];

  memset(node_name, 0, _POSIX_HOST_NAME_MAX);
  strncpy(node_name, evt->node_name, _POSIX_HOST_NAME_MAX);

  TRACE_ENTER2("client_id %u, stream ID %u, node_name = %s", param->client_id,
               param->lstr_id, node_name);

  // Client should try again when role changes is in transition.
  if (cb->is_quiesced_set) {
    TRACE("Log service is in quiesced state");
    AckToWriteAsync(param, evt->fr_dest, SA_AIS_ERR_TRY_AGAIN);
    return NCSCC_RC_SUCCESS;
  }

  if (lgs_client_get_by_id(param->client_id) == nullptr) {
    TRACE("Bad client ID: %u", param->client_id);
    AckToWriteAsync(param, evt->fr_dest, SA_AIS_ERR_BAD_HANDLE);
    return NCSCC_RC_SUCCESS;
  }

  if ((stream = log_stream_get_by_id(param->lstr_id)) == nullptr) {
    TRACE("Bad stream ID: %u", param->lstr_id);
    AckToWriteAsync(param, evt->fr_dest, SA_AIS_ERR_BAD_HANDLE);
    return NCSCC_RC_SUCCESS;
  }

  /* Apply filtering only to system and application streams */
  if ((param->logRecord->logHdrType == SA_LOG_GENERIC_HEADER) &&
      ((stream->severityFilter &
        (1 << param->logRecord->logHeader.genericHdr.logSeverity)) == 0)) {
    stream->filtered++;
    AckToWriteAsync(param, evt->fr_dest, SA_AIS_OK);
    return NCSCC_RC_SUCCESS;
  }

  /*
  ** To avoid truncation we support fixedLogRecordSize==0. We then allocate an
  ** a buffer with an implementation defined size instead. We also do not pad in
  *this mode.
  */
  max_logrecsize =
      *static_cast<const uint32_t *>(lgs_cfg_get(LGS_IMM_LOG_MAX_LOGRECSIZE));
  buf_size = stream->fixedLogRecordSize == 0 ? max_logrecsize
                                             : stream->fixedLogRecordSize;
  logOutputString = static_cast<char *>(
      calloc(1, buf_size + 1)); /* Make room for a '\0' termination */
  if (logOutputString == nullptr) {
    LOG_ER("Could not allocate %d bytes", stream->fixedLogRecordSize + 1);
    AckToWriteAsync(param, evt->fr_dest, SA_AIS_ERR_NO_MEMORY);
    return NCSCC_RC_SUCCESS;
  }

  if ((n = lgs_format_log_record(
           param->logRecord, stream->logFileFormat, stream->maxLogFileSize,
           stream->fixedLogRecordSize, buf_size, logOutputString,
           ++stream->logRecordId, node_name)) == 0) {
    AckToWriteAsync(param, evt->fr_dest, SA_AIS_ERR_INVALID_PARAM);
    return NCSCC_RC_SUCCESS;
  }

  logOutputString[n] = '\0';
  auto info = std::make_shared<Cache::WriteAsyncInfo>(param,
                                                      evt->fr_dest, node_name);
  auto data = std::make_shared<Cache::Data>(info, logOutputString, n);
  Cache::instance()->Write(data);
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 * Name          : process_api_evt
 *
 * Description   : This is the function which is called when lgs receives an
 *                 event either because of an API Invocation or other internal
 *                 messages from LGA clients
 *
 * Arguments     : evt  - Message that was posted to the LGS Mail box.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE
 *
 * Notes         : None.
 *****************************************************************************/
static uint32_t process_api_evt(lgsv_lgs_evt_t *evt) {
  lgsv_api_msg_type_t api_type;

  if (evt->evt_type != LGSV_LGS_LGSV_MSG) goto done;

  if (evt->info.msg.type >= LGSV_MSG_MAX) {
    LOG_ER("Invalid event type %d", evt->info.msg.type);
    goto done;
  }

  api_type = evt->info.msg.info.api_info.type;

  if (api_type >= LGSV_API_MAX) {
    LOG_ER("Invalid msg type %d", api_type);
    goto done;
  }

  /* Discard too old messages. Don't discard writes as they are async,
   * no one is waiting on a response.
   * Using osaf time functions will guarantee that code works on 32 and 64 bit
   * systems.
   */
  if (api_type < LGSV_WRITE_LOG_ASYNC_REQ) {
    struct timespec current_ts, diff_ts;
    osaf_clock_gettime(CLOCK_MONOTONIC, &current_ts);

    /* Calculate time diff current - entered */
    if (osaf_timespec_compare(&current_ts, &evt->entered_at) < 0) {
      LOG_ER("%s - Entered message time > current time", __FUNCTION__);
      osafassert(0);
    }
    osaf_timespec_subtract(&current_ts, &evt->entered_at, &diff_ts);

    /* Convert to millisec and compare with sync send time used in
     * library
     */
    if (osaf_timespec_to_millis(&diff_ts) > (LGS_WAIT_TIME * 10)) {
      LOG_IN("discarded message from %" PRIx64 " type %u", evt->fr_dest,
             api_type);
      goto done;
    }
  }

  if (lgs_lga_api_msg_dispatcher[api_type](lgs_cb, evt) != NCSCC_RC_SUCCESS) {
    LOG_WA("lgs_lga_api_msg_dispatcher FAILED type: %d", evt->info.msg.type);
  }

done:
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 * Name          : lgs_process_mbx
 *
 * Description   : This is the function which process the IPC mail box of
 *                 LGS
 *
 * Arguments     : mbx  - This is the mail box pointer on which LGS is
 *                        going to block.
 *
 * Return Values : None.
 *
 * Notes         : None.
 *****************************************************************************/
void lgs_process_mbx(SYSF_MBX *mbx) {
  lgsv_lgs_evt_t *msg;

  msg = reinterpret_cast<lgsv_lgs_evt_t *>(m_NCS_IPC_NON_BLK_RECEIVE(mbx, msg));
  if (msg != nullptr) {
    if (lgs_cb->ha_state == SA_AMF_HA_ACTIVE) {
      if (msg->evt_type <= LGSV_LGS_EVT_LGA_DOWN) {
        lgs_lgsv_top_level_evt_dispatch_tbl[msg->evt_type](msg);
      } else if (msg->evt_type == LGSV_EVT_QUIESCED_ACK) {
        proc_mds_quiesced_ack_msg(msg);
      } else if (msg->evt_type == LGSV_EVT_NO_OP) {
        TRACE("Jolted the main thread so it picks up the new IMM FD");
      } else if (msg->evt_type == LGSV_EVT_RDA) {
        TRACE("ignoring RDA message for role %u", msg->info.rda_info.io_role);
      } else {
        LOG_ER("message type invalid");
      }
    } else {
      if (msg->evt_type == LGSV_LGS_EVT_LGA_DOWN) {
        lgs_lgsv_top_level_evt_dispatch_tbl[msg->evt_type](msg);
      }
      if (msg->evt_type == LGSV_EVT_RDA) {
        proc_rda_cb_msg(msg);
      }
    }

    lgs_evt_destroy(msg);
  }
}

//>
// Below code are added in the scope of improving the resilience of log server
//<

void AckToWriteAsync(WriteAsyncParam* req, MDS_DEST dest,
                     SaAisErrorT code) {
  if (req->ack_flags != SA_LOG_RECORD_WRITE_ACK) {
    lgs_free_write_log(req);
    return;
  }
  lgs_send_write_log_ack(req->client_id, req->invocation, code, dest);
  lgs_free_write_log(req);
}

bool is_active() { return lgs_cb->ha_state == SA_AMF_HA_ACTIVE; }
