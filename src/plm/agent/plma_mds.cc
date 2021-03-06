/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
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
 * Author(s): Emerson
 *            High Availability Solutions Pvt. Ltd.
 */

/*************************************************************************
 * @file  : plma_mds.c
 * @brief : This file contains routines used by PLMA library for MDS Interface.
 * @author: Emerson Network Power
 *****************************************************************************/

#include <thread>
#include "base/osaf_poll.h"
#include "plm/common/plms_common.h"
#include "plma.h"

MDS_CLIENT_MSG_FORMAT_VER
plma_plms_msg_fmt_table[PLMA_WRT_PLMS_SUBPART_VER_RANGE] = {1};

uint32_t plma_mds_callback(struct ncsmds_callback_info *info);
static uint32_t plma_mds_rcv(MDS_CALLBACK_RECEIVE_INFO *rcv_info);
static uint32_t plma_mds_svc_evt(MDS_CALLBACK_SVC_EVENT_INFO *svc_evt);

/***********************************************************************
 * @brief : This routine gets MDS handle for PLMA.
 * @return: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 ***************************************************************************/
uint32_t plma_mds_get_handle() {
  NCSADA_INFO arg;
  uint32_t rc;
  PLMA_CB *plma_cb = plma_ctrlblk;

  TRACE_ENTER();

  memset(&arg, 0, sizeof(NCSADA_INFO));
  arg.req = NCSADA_GET_HDLS;
  rc = ncsada_api(&arg);

  if (rc != NCSCC_RC_SUCCESS) {
    return rc;
  }

  plma_cb->mds_hdl = arg.info.adest_get_hdls.o_mds_pwe1_hdl;
  plma_cb->mdest_id = arg.info.adest_get_hdls.o_adest;
  TRACE_5("PLM agent handle got : %d", plma_cb->mds_hdl);
  TRACE_5("PLM agent mdest ID got : %" PRIu64, plma_cb->mdest_id);

  TRACE_LEAVE2("%d", rc);

  return rc;
}

/***********************************************************************
 * @brief : This routine registers the PLMA with MDS.
 * @return: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 ***************************************************************************/
uint32_t plma_mds_register() {
  uint32_t rc = NCSCC_RC_SUCCESS;
  NCSMDS_INFO svc_info;
  MDS_SVC_ID svc_id[1] = {NCSMDS_SVC_ID_PLMS};
  PLMA_CB *plma_cb = plma_ctrlblk;
  TRACE_ENTER();

  /* STEP 1: the MDS handle for PLMA*/
  rc = plma_mds_get_handle();
  if (NCSCC_RC_SUCCESS != rc) {
    LOG_ER("PLMA - mds get handle failed");
    return rc;
  }

  /* STEP 2 : Install with MDS with service ID NCSMDS_SVC_ID_PLMA. */
  memset(&svc_info, 0, sizeof(NCSMDS_INFO));

  svc_info.i_mds_hdl = plma_cb->mds_hdl;
  svc_info.i_svc_id = NCSMDS_SVC_ID_PLMA;
  svc_info.i_op = MDS_INSTALL;

  svc_info.info.svc_install.i_yr_svc_hdl = 0;
  svc_info.info.svc_install.i_install_scope =
      NCSMDS_SCOPE_NONE;                                  /*node specific */
  svc_info.info.svc_install.i_svc_cb = plma_mds_callback; /* callback */
  svc_info.info.svc_install.i_mds_q_ownership = false;
  /***************************FIXME : MDS svc private sub part ver no?.**/
  svc_info.info.svc_install.i_mds_svc_pvt_ver = PLMA_MDS_PVT_SUBPART_VERSION;

  if (ncsmds_api(&svc_info) == NCSCC_RC_FAILURE) {
    LOG_ER("PLMA - MDS Install Failed");
    return NCSCC_RC_FAILURE;
  }

  /* STEP 3 : Subscribe to PLMS up/down events */
  memset(&svc_info, 0, sizeof(NCSMDS_INFO));
  svc_info.i_mds_hdl = plma_cb->mds_hdl;
  svc_info.i_svc_id = NCSMDS_SVC_ID_PLMA;
  svc_info.i_op = MDS_SUBSCRIBE;
  svc_info.info.svc_subscribe.i_num_svcs = 1;
  svc_info.info.svc_subscribe.i_scope = NCSMDS_SCOPE_NONE;
  svc_info.info.svc_subscribe.i_svc_ids = svc_id;
  if (ncsmds_api(&svc_info) == NCSCC_RC_FAILURE) {
    LOG_ER("PLMA - MDS Subscribe for PLMS up/down Failed");
    plma_mds_unregister();
    return NCSCC_RC_FAILURE;
  }
  TRACE_LEAVE2("%d", rc);
  return rc;
}

/****************************************************************************
Name          : plma_mds_callback

Description   : This callback routine will be called by MDS on event arrival

Arguments     : info - pointer to the mds callback info

Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE

Notes         : None.
*****************************************************************************/
uint32_t plma_mds_callback(struct ncsmds_callback_info *info) {
  PLMA_CB *plma_cb = plma_ctrlblk;
  uint32_t rc = NCSCC_RC_SUCCESS;
  TRACE_ENTER();

  assert(info != NULL);

  switch (info->i_op) {
    case MDS_CALLBACK_COPY:
      rc = NCSCC_RC_FAILURE;
      TRACE_1("MDS_CALLBACK_COPY - do nothing");
      break;
    case MDS_CALLBACK_ENC:
      rc = plms_mds_enc(&info->info.enc, &plma_cb->edu_hdl);
      break;
    case MDS_CALLBACK_DEC:
      rc = plms_mds_dec(&info->info.dec, &plma_cb->edu_hdl);
      break;
    case MDS_CALLBACK_ENC_FLAT:
      if (0) {
        /*Set to zero for righorous testing of byteorder
         * enc/dec.*/
        rc = plms_mds_enc_flat(info, &plma_cb->edu_hdl);
      } else {
        rc = plms_mds_enc(&info->info.enc_flat, &plma_cb->edu_hdl);
      }
      break;
    case MDS_CALLBACK_DEC_FLAT:
      if (1) {
        /*Set to zero for righorous testing of byteorderr
         * enc/dec.*/
        rc = plms_mds_dec_flat(info, &plma_cb->edu_hdl);
      } else {
        rc = plms_mds_dec(&info->info.dec_flat, &plma_cb->edu_hdl);
      }
      break;
    case MDS_CALLBACK_RECEIVE:
      rc = plma_mds_rcv(&info->info.receive);
      break;
    case MDS_CALLBACK_SVC_EVENT:
      rc = plma_mds_svc_evt(&info->info.svc_evt);
      break;
    case MDS_CALLBACK_DIRECT_RECEIVE:
      rc = NCSCC_RC_FAILURE;
      TRACE_1("MDS_CALLBACK_DIRECT_RECEIVE - do nothing");
      break;
    default:
      LOG_ER("PLMA :Illegal type of MDS message");
      rc = NCSCC_RC_FAILURE;
      break;
  }
  TRACE_LEAVE2("%d", rc);
  return rc;
}

/***********************************************************************
 * @brief    : MDS will call this function on receiving PLMA messages.
 * @param[in]: rcv_info - MDS Receive information.
 * @return   : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 ***************************************************************************/
static uint32_t plma_mds_rcv(MDS_CALLBACK_RECEIVE_INFO *rcv_info) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  PLMA_ENTITY_GROUP_INFO *grp_info;
  PLMA_CB *plma_cb = plma_ctrlblk;
  PLMS_EVT *pEvt = (PLMS_EVT *)rcv_info->i_msg;
  TRACE_ENTER();
  if (pEvt->req_res == PLMS_REQ) {
    if (pEvt->req_evt.req_type == PLMS_AGENT_TRACK_EVT_T) {
      if (pEvt->req_evt.agent_track.evt_type == PLMS_AGENT_TRACK_CBK_EVT) {
        SaPlmEntityGroupHandleT grp_hdl = pEvt->req_evt.agent_track.grp_handle;

        grp_info = (PLMA_ENTITY_GROUP_INFO *)ncs_patricia_tree_get(
            &plma_cb->entity_group_info, (uint8_t *)&grp_hdl);

        if (!grp_info) {
          /** FIXME : free the evt structure */
          return NCSCC_RC_SUCCESS;
        }
        if (!grp_info->client_info) {
          /** FIXME : free the evt structure */
          return NCSCC_RC_SUCCESS;
        }
        pEvt->sinfo.ctxt = rcv_info->i_msg_ctxt;
        pEvt->sinfo.dest = rcv_info->i_fr_dest;
        pEvt->sinfo.to_svc = rcv_info->i_fr_svc_id;
        if (rcv_info->i_rsp_reqd) {
          pEvt->sinfo.stype = MDS_SENDTYPE_RSP;
        }

        /* Put it in PLMA's Event Queue */
        rc = m_NCS_IPC_SEND(&grp_info->client_info->callbk_mbx,
                            (NCSCONTEXT)pEvt, NCS_IPC_PRIORITY_NORMAL);

        if (NCSCC_RC_SUCCESS != rc) {
          LOG_ER("PLMA - IPC SEND FAILED");
        }
      }
    }
  }
  TRACE_LEAVE2("%d", rc);

  return rc;
}

static void reinit_with_plms(void) {
  // reinit the clients with plmd
  PLMA_CLIENT_INFO *client_node = 0;
  SaPlmHandleT *temp_ptr = 0, temp_hdl = 0;
  uint32_t rc = NCSCC_RC_SUCCESS;
  PLMA_CB *plma_cb = plma_ctrlblk;

  TRACE_ENTER();

  m_NCS_LOCK(&plma_cb->cb_lock, NCS_LOCK_WRITE);

  if (plma_cb->plms_sync_awaited)
    m_NCS_SEL_OBJ_IND(&plma_cb->sel_obj);

  while ((client_node = (PLMA_CLIENT_INFO *)ncs_patricia_tree_getnext(
         &plma_cb->client_info, (uint8_t *)temp_ptr))) {
    PLMS_EVT plm_init_evt, *plm_init_resp = 0;

    temp_hdl = client_node->plm_handle;
    temp_ptr = &temp_hdl;

    // Fill the init event structure
    memset(&plm_init_evt, 0, sizeof(PLMS_EVT));
    plm_init_evt.req_res = PLMS_REQ;
    plm_init_evt.req_evt.req_type = PLMS_AGENT_LIB_REQ_EVT_T;
    plm_init_evt.req_evt.agent_lib_req.lib_req_type = PLMS_AGENT_REINIT_EVT;
    plm_init_evt.req_evt.agent_lib_req.plm_handle = client_node->plm_handle;

    TRACE("sending reinit to plmd with handle: %llu", client_node->plm_handle);

    // Send a sync msg to PLMS to obtain plmHandle for this client
    rc = plm_mds_msg_sync_send(plma_cb->mds_hdl, NCSMDS_SVC_ID_PLMA,
                               NCSMDS_SVC_ID_PLMS, plma_cb->plms_mdest_id,
                               &plm_init_evt, &plm_init_resp,
                               PLMS_MDS_SYNC_TIME);

    if (rc != NCSCC_RC_SUCCESS || !plm_init_resp)
      LOG_ER("unable to reinit handle %llu with plmd", client_node->plm_handle);

    if (plm_init_resp)
      plms_free_evt(plm_init_resp);
  }

  // readd entity groups
  PLMA_ENTITY_GROUP_INFO *grp_info_node = 0;
  temp_ptr = 0;
  while ((grp_info_node = (PLMA_ENTITY_GROUP_INFO *)
         ncs_patricia_tree_getnext(&plma_cb->entity_group_info,
                                   (uint8_t *)temp_ptr))) {
    temp_hdl = grp_info_node->entity_group_handle;
    temp_ptr = &temp_hdl;

    TRACE("reiniting group handle: %llu", grp_info_node->entity_group_handle);
    PLMS_EVT plm_in_evt, *plm_out_resp = 0;
    memset(&plm_in_evt, 0, sizeof(PLMS_EVT));
    plm_in_evt.req_res = PLMS_REQ;
    plm_in_evt.req_evt.req_type = PLMS_AGENT_GRP_OP_EVT_T;
    plm_in_evt.req_evt.agent_grp_op.grp_evt_type = PLMS_AGENT_GRP_REINIT_EVT;
    plm_in_evt.req_evt.agent_grp_op.plm_handle =
      grp_info_node->client_info->plm_handle;
    plm_in_evt.req_evt.agent_grp_op.grp_handle =
      grp_info_node->entity_group_handle;

    rc = plm_mds_msg_sync_send(plma_cb->mds_hdl, NCSMDS_SVC_ID_PLMA,
                               NCSMDS_SVC_ID_PLMS, plma_cb->plms_mdest_id,
                               &plm_in_evt, &plm_out_resp, PLMS_MDS_SYNC_TIME);

    if (rc != NCSCC_RC_SUCCESS || !plm_out_resp) {
      LOG_ER("unable to reinit entity group %llu with plmd",
             grp_info_node->entity_group_handle);
    }

    if (plm_out_resp)
      plms_free_evt(plm_out_resp);

    for (auto it : grp_info_node->entityNamesList) {
      PLMS_EVT plm_in_evt, *plm_out_resp = 0;

      memset(&plm_in_evt, 0, sizeof(PLMS_EVT));
      plm_in_evt.req_res = PLMS_REQ;
      plm_in_evt.req_evt.req_type = PLMS_AGENT_GRP_OP_EVT_T;
      plm_in_evt.req_evt.agent_grp_op.grp_evt_type = PLMS_AGENT_GRP_ADD_EVT;
      plm_in_evt.req_evt.agent_grp_op.plm_handle =
        grp_info_node->client_info->plm_handle;
      plm_in_evt.req_evt.agent_grp_op.grp_handle =
        grp_info_node->entity_group_handle;
      plm_in_evt.req_evt.agent_grp_op.entity_names_number =
        it.first.size();

      SaNameT *names(new SaNameT[it.first.size()]);
      for (size_t i(0); i < it.first.size(); i++) {
        names[i].length = it.first[i].length;
        memcpy(names[i].value, it.first[i].value, names[i].length);
      }

      plm_in_evt.req_evt.agent_grp_op.entity_names = names;

      plm_in_evt.req_evt.agent_grp_op.grp_add_option = it.second;

      // Send a mds sync msg to PLMS to obtain group handle for this.
      rc = plm_mds_msg_sync_send(plma_cb->mds_hdl, NCSMDS_SVC_ID_PLMA,
                                 NCSMDS_SVC_ID_PLMS, plma_cb->plms_mdest_id,
                                 &plm_in_evt, &plm_out_resp,
                                 PLMS_MDS_SYNC_TIME);

      if (rc != NCSCC_RC_SUCCESS || !plm_out_resp) {
        LOG_ER("unable to reinit entity names %llu with plmd",
               grp_info_node->entity_group_handle);
      }

      if (plm_out_resp)
        plms_free_evt(plm_out_resp);
    }
  }

  // reinit tracking
  temp_ptr = 0;
  while ((grp_info_node = (PLMA_ENTITY_GROUP_INFO *)
         ncs_patricia_tree_getnext(&plma_cb->entity_group_info,
                                   (uint8_t *)temp_ptr))) {
    temp_hdl = grp_info_node->entity_group_handle;
    temp_ptr = &temp_hdl;

    if (grp_info_node->is_trk_enabled) {
      PLMS_EVT plm_in_evt;

      memset(&plm_in_evt, 0, sizeof(PLMS_EVT));

      plm_in_evt.req_res = PLMS_REQ;
      plm_in_evt.req_evt.req_type = PLMS_AGENT_TRACK_EVT_T;
      plm_in_evt.req_evt.agent_track.agent_handle =
        grp_info_node->client_info->plm_handle;
      plm_in_evt.req_evt.agent_track.grp_handle =
        grp_info_node->entity_group_handle;
      plm_in_evt.req_evt.agent_track.evt_type = PLMS_AGENT_TRACK_START_EVT;
      plm_in_evt.req_evt.agent_track.track_start.track_flags =
        grp_info_node->trackFlags;
      plm_in_evt.req_evt.agent_track.track_start.track_cookie =
        grp_info_node->trackCookie;

      rc = plms_mds_normal_send(plma_cb->mds_hdl, NCSMDS_SVC_ID_PLMA,
                                &plm_in_evt, plma_cb->plms_mdest_id,
                                NCSMDS_SVC_ID_PLMS);

      if (NCSCC_RC_SUCCESS != rc)
        LOG_ER("unable to reinit tracking with plmd");
    }
  }

  plma_cb->plms_svc_up = true;

  m_NCS_UNLOCK(&plma_cb->cb_lock, NCS_LOCK_WRITE);

  TRACE_LEAVE();
}

/***********************************************************************
 * @brief    : PLMA is informed when MDS events occur that he has subscribed to.
 * @param[in]: svc_evt - MDS Svc evt info.
 * @return   : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 ***************************************************************************/
static uint32_t plma_mds_svc_evt(MDS_CALLBACK_SVC_EVENT_INFO *svc_evt) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  PLMA_CB *plma_cb = plma_ctrlblk;

  TRACE_ENTER();

  if (svc_evt->i_svc_id != NCSMDS_SVC_ID_PLMS) {
    return NCSCC_RC_SUCCESS;
  }
  switch (svc_evt->i_change) {
    case NCSMDS_RED_UP:
      break;
    case NCSMDS_NEW_ACTIVE:
      TRACE_5("Received NCSMDS_NEW_ACTIVE for PLMS");
    case NCSMDS_UP:
      TRACE_5("Received MDSUP EVT for PLMS");
      m_NCS_LOCK(&plma_cb->cb_lock, NCS_LOCK_WRITE);
      plma_cb->plms_mdest_id = svc_evt->i_dest;

      if (ncs_patricia_tree_size(&plma_cb->client_info)) {
        std::thread x(&reinit_with_plms);
        x.detach();
      } else {
        plma_cb->plms_svc_up = true;
        if (plma_cb->plms_sync_awaited == true) {
          m_NCS_SEL_OBJ_IND(&plma_cb->sel_obj);
        }
      }
      m_NCS_UNLOCK(&plma_cb->cb_lock, NCS_LOCK_WRITE);
      break;
    case NCSMDS_NO_ACTIVE:
      TRACE_5("Received NCSMDS_NO_ACTIVE for PLMS");
    case NCSMDS_DOWN:
      TRACE_5("Received MDSDOWN EVT for PLMS");
      plma_cb->plms_mdest_id = 0;
      plma_cb->plms_svc_up = false;
      break;
    default:
      TRACE_5("Received unknown event");
      break;
  }

  TRACE_LEAVE2("%d", rc);

  return rc;
}

/***********************************************************************
 * @brief : This function un-registers the PLMA Service with MDS.
 * @return: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 ***************************************************************************/
void plma_mds_unregister() {
  NCSMDS_INFO arg;
  uint32_t rc;
  PLMA_CB *plma_cb = plma_ctrlblk;

  TRACE_ENTER();

  /**
   * Un-install your service into MDS.
   * No need to cancel the services that are subscribed
   */
  memset(&arg, 0, sizeof(NCSMDS_INFO));

  arg.i_mds_hdl = plma_cb->mds_hdl;
  arg.i_svc_id = NCSMDS_SVC_ID_PLMA;
  arg.i_op = MDS_UNINSTALL;

  if ((rc = ncsmds_api(&arg)) != NCSCC_RC_SUCCESS) {
    LOG_ER("PLMA - MDS Unregister Failed rc:%u", rc);
    goto done;
  }
done:
  TRACE_LEAVE();
  return;
}

/***********************************************************************
 * @brief : This function is for PLMA to sync with PLMS when it gets
 *	   MDS callback.
 * @return: Returns nothing.
 ***************************************************************************/
void plma_sync_with_plms() {
  PLMA_CB *cb = plma_ctrlblk;

  TRACE_ENTER();

  m_NCS_LOCK(&cb->cb_lock, NCS_LOCK_WRITE);

  if (cb->plms_svc_up) {
    TRACE("Plms is up");
    m_NCS_UNLOCK(&cb->cb_lock, NCS_LOCK_WRITE);
    return;
  }

  cb->plms_sync_awaited = true;
  m_NCS_SEL_OBJ_CREATE(&cb->sel_obj);
  m_NCS_UNLOCK(&cb->cb_lock, NCS_LOCK_WRITE);

  /** Await indication from MDS saying PLMS is up */
  osaf_poll_one_fd(m_GET_FD_FROM_SEL_OBJ(cb->sel_obj), 30000);

  /* Destroy the sync - object */
  m_NCS_LOCK(&cb->cb_lock, NCS_LOCK_WRITE);

  cb->plms_sync_awaited = false;
  m_NCS_SEL_OBJ_DESTROY(&cb->sel_obj);

  m_NCS_UNLOCK(&cb->cb_lock, NCS_LOCK_WRITE);

  TRACE_LEAVE();
  return;
}
