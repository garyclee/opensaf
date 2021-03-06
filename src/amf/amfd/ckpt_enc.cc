/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright (C) 2017, Oracle and/or its affiliates. All rights reserved.
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
 * Author(s): Emerson Network Power
 *
 */

/*****************************************************************************

  DESCRIPTION: This module contain all the encode routines require for encoding
  AVD data structures during checkpointing.

******************************************************************************/

#include "base/logtrace.h"
#include "osaf/saflog/saflog.h"
#include "amf/amfd/amfd.h"
#include "amf/amfd/cluster.h"
#include "amf/common/amf_db_template.h"

extern "C" const AVSV_ENCODE_CKPT_DATA_FUNC_PTR
    avd_enc_ckpt_data_func_list[AVSV_CKPT_MSG_MAX];

/* Declaration of async update functions */
static uint32_t enc_cb_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_cluster_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_app_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_admin_si(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_siass(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_trans(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_oper_su(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_up_info(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_oper_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_failover_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_rcv_msg_id(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_node_snd_msg_id(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_adjust_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_su_assigned_num(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_su_spare_num(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_su_uninst_num(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_sg_fsm_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_si_curr_active(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_si_curr_stby(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_term_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_inst_msg_processed(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_recvr_node_fover_swover(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_switch(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_oper_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_pres_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_readiness_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_act_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_restart_count(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_su_preinstan(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_su_curr_active(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_su_curr_stby(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_switch(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_assignment_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_dep_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_si_alarm_sent(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_proxy_comp_name(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_curr_num_csi_actv(AVD_CL_CB *cb,
                                           NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_curr_num_csi_stby(AVD_CL_CB *cb,
                                           NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_oper_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_readiness_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_pres_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_restart_count(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t enc_comp_cs_type_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);
static uint32_t entire_data_update(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   bool c_sync);
static uint32_t enc_ng_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc);

/* Declaration of static cold sync encode functions */
static uint32_t enc_cs_cb_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj);
static uint32_t enc_cs_cluster_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                      uint32_t *num_of_obj);
static uint32_t enc_cs_node_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   uint32_t *num_of_obj);
static uint32_t enc_cs_app_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                  uint32_t *num_of_obj);
static uint32_t enc_cs_sg_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj);
static uint32_t enc_cs_su_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj);
static uint32_t enc_cs_si_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj);
static uint32_t enc_cs_sg_su_oper_list(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                       uint32_t *num_of_obj);
static uint32_t enc_cs_sg_admin_si(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   uint32_t *num_of_obj);
static uint32_t enc_cs_comp_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   uint32_t *num_of_obj);
static uint32_t enc_cs_siass(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                             uint32_t *num_of_obj);
static uint32_t enc_cs_si_trans(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                uint32_t *num_of_obj);
static uint32_t enc_cs_async_updt_cnt(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                      uint32_t *num_of_obj);
static uint32_t enc_cs_comp_cs_type_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                           uint32_t *num_of_obj);
static uint32_t enc_avd_to_avd_job_queue_status(AVD_CL_CB *cb,
                                                NCS_MBCSV_CB_ENC *enc);

static uint32_t enc_su_oper_list(AVD_CL_CB *cb, AVD_SG *sg,
                                 NCS_MBCSV_CB_ENC *enc);
/*
 * Function list for encoding the async data.
 * We will jump into this function using the reo_type received
 * in the encode argument.
 *
 * This array _must_ correspond to avsv_ckpt_msg_reo_type in ckpt_msg.h
 */
const AVSV_ENCODE_CKPT_DATA_FUNC_PTR avd_enc_ckpt_data_func_list[] = {
    enc_cb_config, enc_cluster_config, enc_node_config, enc_app_config,
    enc_sg_config, enc_su_config, enc_si_config, enc_oper_su, enc_sg_admin_si,
    enc_comp_config, enc_comp_cs_type_config, enc_siass,
    /* SI transfer update messages */
    enc_si_trans,

    /*
     * Messages to update independent fields.
     */

    /* Node Async Update messages */
    enc_node_admin_state, enc_node_oper_state, enc_node_up_info, enc_node_state,
    enc_node_rcv_msg_id, enc_node_snd_msg_id,

    /* SG Async Update messages */
    enc_sg_admin_state, enc_sg_su_assigned_num, enc_sg_su_spare_num,
    enc_sg_su_uninst_num, enc_sg_adjust_state, enc_sg_fsm_state,

    /* SU Async Update messages */
    enc_su_preinstan, enc_su_oper_state, enc_su_admin_state,
    enc_su_readiness_state, enc_su_pres_state, enc_su_si_curr_active,
    enc_su_si_curr_stby, enc_su_term_state, enc_su_switch, enc_su_act_state,

    /* SI Async Update messages */
    enc_si_admin_state, enc_si_assignment_state, enc_si_su_curr_active,
    enc_si_su_curr_stby, enc_si_switch, enc_si_alarm_sent,

    /* COMP Async Update messages */
    enc_comp_proxy_comp_name, enc_comp_curr_num_csi_actv,
    enc_comp_curr_num_csi_stby, enc_comp_oper_state, enc_comp_readiness_state,
    enc_comp_pres_state, enc_comp_restart_count, nullptr, /* AVSV_SYNC_COMMIT */
    enc_su_restart_count, enc_si_dep_state, enc_ng_admin_state,
    enc_avd_to_avd_job_queue_status,
    enc_node_failover_state, enc_su_inst_msg_processed, enc_recvr_node_fover_swover};

/*
 * Function list for encoding the cold sync response data
 * We will jump into this function using the reo_type received
 * in the cold sync response argument.
 */
const AVSV_ENCODE_COLD_SYNC_RSP_DATA_FUNC_PTR enc_cs_data_func_list[] = {
    enc_cs_cb_config,   enc_cs_cluster_config,      enc_cs_node_config,
    enc_cs_app_config,  enc_cs_sg_config,           enc_cs_su_config,
    enc_cs_si_config,   enc_cs_sg_su_oper_list,     enc_cs_sg_admin_si,
    enc_cs_comp_config, enc_cs_comp_cs_type_config, enc_cs_siass,
    enc_cs_si_trans,    enc_cs_async_updt_cnt};

void encode_cb(NCS_UBAID *ub, const AVD_CL_CB *cb,
               const uint16_t peer_version) {
  osaf_encode_uint32(ub, cb->init_state);
  osaf_encode_satimet(ub, cb->cluster_init_time);
  osaf_encode_uint32(ub, cb->nodes_exit_cnt);
}

/****************************************************************************\
 * Function: enc_cb_config
 *
 * Purpose:  Encode entire CB data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cb_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  /*
   * For updating CB, action is always to do update. We don't have add and
   * remove action on CB. So call EDU to encode CB data.
   */
  osafassert(enc->io_action == NCS_MBCSV_ACT_UPDATE);
  encode_cb(&enc->io_uba, cb, enc->i_peer_version);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

void encode_cluster(NCS_UBAID *ub, const AVD_CLUSTER *cluster,
                    const uint16_t peer_version) {
  osaf_encode_uint32(ub, cluster->saAmfClusterAdminState);
}

/****************************************************************************\
 * Function: enc_cluster_config
 *
 * Purpose:  Encode entire AVD_AVND data.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cluster_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_CLUSTER *cluster = reinterpret_cast<AVD_CLUSTER *>(enc->io_reo_hdl);

  switch (enc->io_action) {
    case NCS_MBCSV_ACT_UPDATE:
      encode_cluster(&enc->io_uba, cluster, enc->i_peer_version);
      break;
    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

void encode_node_config(NCS_UBAID *ub, const AVD_AVND *avnd,
                        uint16_t peer_version) {
  osaf_encode_uint32(ub, avnd->node_info.nodeId);
  osaf_encode_saclmnodeaddresst(ub, &avnd->node_info.nodeAddress);
  osaf_encode_sanamet_o2(ub, avnd->name.c_str());
  osaf_encode_bool(ub, avnd->node_info.member);
  osaf_encode_satimet(ub, avnd->node_info.bootTimestamp);
  osaf_encode_uint64(ub, avnd->node_info.initialViewNumber);
  osaf_encode_uint64(ub, avnd->adest);
  osaf_encode_uint32(ub, avnd->saAmfNodeAdminState);
  osaf_encode_uint32(ub, avnd->saAmfNodeOperState);
  osaf_encode_uint32(ub, avnd->node_state);
  osaf_encode_uint32(ub, AVSV_AVND_CARD_SYS_CON);
  osaf_encode_uint32(ub, avnd->rcv_msg_id);
  osaf_encode_uint32(ub, avnd->snd_msg_id);
  if (peer_version >= AVD_MBCSV_SUB_PART_VERSION_10) {
    osaf_encode_uint32(ub, avnd->failover_state);
  }
  if (peer_version >= AVD_MBCSV_SUB_PART_VERSION_12)
    osaf_encode_bool(ub, avnd->recvr_fail_sw);
}

/****************************************************************************\
 * Function: enc_node_config
 *
 * Purpose:  Encode entire AVD_AVND data.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_node_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);
  /*
   * Check for the action type (whether it is add, rmv or update) and act
   * accordingly. If it is update or add, encode entire data. If it is rmv
   * send key information only.
   */
  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_UPDATE:
      /* Send entire data */
      encode_node_config(&enc->io_uba, avnd, enc->i_peer_version);
      break;
    case NCS_MBCSV_ACT_RMV:
      /* Send only key information */
      osaf_encode_sanamet_o2(&enc->io_uba, avnd->name.c_str());
      break;
    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

void encode_app(NCS_UBAID *ub, const AVD_APP *app) {
  osaf_encode_sanamet_o2(ub, app->name.c_str());
  osaf_encode_uint32(ub, app->saAmfApplicationAdminState);
  osaf_encode_uint32(ub, app->saAmfApplicationCurrNumSGs);
}

/****************************************************************************\
 * Function: enc_app_config
 *
 * Purpose:  Encode entire AVD_AVND data.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_app_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  const AVD_APP *app = (AVD_APP *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  TRACE_ENTER2("io_action '%u'", enc->io_action);

  /*
   * Check for the action type (whether it is add, rmv or update) and act
   * accordingly. If it is update or add, encode entire data. If it is rmv
   * send key information only.
   */
  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_UPDATE:
      encode_app(&enc->io_uba, app);
      break;
    case NCS_MBCSV_ACT_RMV:
      osaf_encode_sanamet_o2(&enc->io_uba, app->name.c_str());
      break;
    default:
      osafassert(0);
  }

  TRACE_LEAVE2();
  return NCSCC_RC_SUCCESS;
}

static void encode_sg(NCS_UBAID *ub, const AVD_SG *sg) {
  osaf_encode_sanamet_o2(ub, sg->name.c_str());
  osaf_encode_uint32(ub, sg->saAmfSGAdminState);
  osaf_encode_uint32(ub, sg->saAmfSGNumCurrAssignedSUs);
  osaf_encode_uint32(ub, sg->saAmfSGNumCurrInstantiatedSpareSUs);
  osaf_encode_uint32(ub, sg->saAmfSGNumCurrNonInstantiatedSpareSUs);
  osaf_encode_uint32(ub, sg->adjust_state);
  osaf_encode_uint32(ub, sg->sg_fsm_state);
}

/****************************************************************************\
 *
 * Purpose:  Encode entire AVD_SG data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  const AVD_SG *sg = (AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  TRACE_ENTER2("io_action '%u'", enc->io_action);

  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_UPDATE:
      encode_sg(&enc->io_uba, sg);
      break;
    case NCS_MBCSV_ACT_RMV:
      osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
      break;
    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

static void encode_su(NCS_UBAID *ub, AVD_SU *su, uint16_t peer_version) {
  osaf_encode_sanamet_o2(ub, su->name.c_str());
  osaf_encode_bool(
      ub, (bool)su->saAmfSUPreInstantiable);  // TODO(hafe) change to bool
  osaf_encode_uint32(ub, su->saAmfSUOperState);
  osaf_encode_uint32(ub, su->saAmfSUAdminState);
  osaf_encode_uint32(ub, su->saAmfSuReadinessState);
  osaf_encode_uint32(ub, su->saAmfSUPresenceState);
  osaf_encode_sanamet_o2(ub, su->saAmfSUHostedByNode.c_str());
  osaf_encode_uint32(ub, su->saAmfSUNumCurrActiveSIs);
  osaf_encode_uint32(ub, su->saAmfSUNumCurrStandbySIs);
  osaf_encode_uint32(ub, su->saAmfSURestartCount);
  osaf_encode_bool(ub, su->term_state);
  osaf_encode_uint32(ub, su->su_switch);
  osaf_encode_uint32(ub, su->su_act_state);

  if (peer_version >= AVD_MBCSV_SUB_PART_VERSION_2)
    osaf_encode_bool(ub, su->su_is_external);

  if (peer_version >= AVD_MBCSV_SUB_PART_VERSION_11)
    osaf_encode_bool(ub, su->is_inst_msg_processed);
}

/****************************************************************************\
 *
 * Purpose:  Encode entire AVD_SU data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);

  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_UPDATE:
      encode_su(&enc->io_uba, (AVD_SU *)enc->io_reo_hdl, enc->i_peer_version);
      break;
    case NCS_MBCSV_ACT_RMV: {
      const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
      osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
      break;
    }
    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

static void encode_si(const AVD_CL_CB *cb, NCS_UBAID *ub, const AVD_SI *si,
                      uint16_t peer_version) {
#ifdef UPGRADE_FROM_4_2_1
  // special case for 4.2.1, si_dep_state should be check pointed using ver 4
  uint16_t ver_compare = AVD_MBCSV_SUB_PART_VERSION_4;
#else
  // default case, si_dep_stateavd_peer_ver should not be check pointed for
  // peers in ver 4 (or less)
  uint16_t ver_compare = AVD_MBCSV_SUB_PART_VERSION_5;
#endif
  TRACE_ENTER2("my_version: %u, to_version: %u", ver_compare, peer_version);

  osaf_encode_sanamet_o2(ub, si->name.c_str());
  osaf_encode_uint32(ub, si->saAmfSIAdminState);
  osaf_encode_uint32(ub, si->saAmfSIAssignmentState);
  osaf_encode_uint32(ub, si->saAmfSINumCurrActiveAssignments);
  osaf_encode_uint32(ub, si->saAmfSINumCurrStandbyAssignments);
  osaf_encode_uint32(ub, si->si_switch);
  osaf_encode_sanamet_o2(ub, si->saAmfSIProtectedbySG.c_str());
  osaf_encode_bool(ub, si->alarm_sent);

  if (peer_version >= ver_compare) {
    osaf_encode_uint32(ub, si->si_dep_state);
  }

  TRACE_LEAVE();
}

/****************************************************************************\
 * Function: enc_si_config
 *
 * Purpose:  Encode entire AVD_SI data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_si_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);

  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;

  /*
   * Check for the action type (whether it is add, rmv or update) and act
   * accordingly. If it is update or add, encode entire data. If it is rmv
   * send key information only.
   */
  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_UPDATE:
      /* Send entire data */
      encode_si(cb, &enc->io_uba, si, enc->i_peer_version);
      break;
    case NCS_MBCSV_ACT_RMV:
      /* Send only key information */
      osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
      break;
    default:
      osafassert(0);
  }

  TRACE_LEAVE2();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_sg_admin_si
 *
 * Purpose:  Encode entire AVD_SG_ADMIN_SI data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_admin_si(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  uint32_t status = NCSCC_RC_SUCCESS;
  EDU_ERR ederror = static_cast<EDU_ERR>(0);
  const AVD_SI *si =
      (AVD_SI *)((AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl)))->admin_si;

  TRACE_ENTER2("io_action '%u'", enc->io_action);

  /*
   * Check for the action type (whether it is add, rmv or update) and act
   * accordingly.
   */
  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_RMV:
      /*
       * Send SI key.
       */
      osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
      break;

    case NCS_MBCSV_ACT_UPDATE:
    default:
      osafassert(0);
  }

  if (status != NCSCC_RC_SUCCESS) {
    LOG_ER("%s: encode failed, ederror=%u", __FUNCTION__, ederror);
  }

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

void encode_si_trans(NCS_UBAID *ub, const AVD_SG *sg,
                     const uint16_t peer_version) {
  osaf_encode_sanamet_o2(ub, sg->name.c_str());
  osaf_encode_sanamet_o2(ub, sg->si_tobe_redistributed->name.c_str());
  osaf_encode_sanamet_o2(ub, sg->min_assigned_su->name.c_str());
  osaf_encode_sanamet_o2(ub, sg->max_assigned_su->name.c_str());
}

/*********************************************************************
 * @brief encodes si transfer parameters
 * @param[in] cb
 * @param[in] enc
 ********************************************************************/
static uint32_t enc_si_trans(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);

  const AVD_SG *sg = reinterpret_cast<AVD_SG *>(enc->io_reo_hdl);

  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
      encode_si_trans(&enc->io_uba, sg, enc->i_peer_version);
      break;

    case NCS_MBCSV_ACT_RMV:
      /* Send only key information */
      osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
      break;

    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

void encode_siass(NCS_UBAID *ub, const AVD_SU_SI_REL *susi,
                  const uint16_t peer_version) {
  osaf_encode_sanamet_o2(ub, susi->su->name.c_str());
  osaf_encode_sanamet_o2(ub, susi->si->name.c_str());
  osaf_encode_uint32(ub, susi->state);
  osaf_encode_uint32(ub, susi->fsm);
  if (peer_version >= AVD_MBCSV_SUB_PART_VERSION_3) {
    osaf_encode_bool(ub, static_cast<bool>(susi->csi_add_rem));
    osaf_encode_sanamet_o2(ub, susi->comp_name.c_str());
    osaf_encode_sanamet_o2(ub, susi->csi_name.c_str());
  }
}

/****************************************************************************\
 * Function: enc_siass
 *
 * Purpose:  Encode entire AVD_SU_SI_REL data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_siass(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);

  /*
   * Check for the action type (whether it is add, rmv or update) and act
   * accordingly. If it is update or add, encode entire data. If it is rmv
   * send key information only. In this case key is SU and SI key.
   */
  const AVD_SU_SI_REL *susi =
      static_cast<AVD_SU_SI_REL *>(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_UPDATE:
      encode_siass(&enc->io_uba, susi, enc->i_peer_version);
      break;

    case NCS_MBCSV_ACT_RMV:
      encode_siass(&enc->io_uba, susi, enc->i_peer_version);
      break;

    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: encode_comp
 *
 * Purpose:  Encode entire AVD_COMP data.
 *
 * Input: ub   - USRBUF work space for encode/decode.
 *        comp - AVD_COMP class to be encoded.
 *
 * Returns: void.
 *
 * NOTES:
 *
 *
\**************************************************************************/
void encode_comp(NCS_UBAID *ub, const AVD_COMP *comp) {
  osaf_encode_sanamet(ub, &comp->comp_info.name);
  osaf_encode_uint32(ub, comp->saAmfCompOperState);
  osaf_encode_uint32(ub, comp->saAmfCompReadinessState);
  osaf_encode_uint32(ub, comp->saAmfCompPresenceState);
  osaf_encode_uint32(ub, comp->saAmfCompRestartCount);
  osaf_encode_sanamet_o2(ub, comp->saAmfCompCurrProxyName.c_str());
}

/****************************************************************************\
 * Function: enc_comp_config
 *
 * Purpose:  Encode entire AVD_COMP data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_comp_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);

  AVD_COMP *comp = (AVD_COMP *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));
  /*
   * Check for the action type (whether it is add, rmv or update) and act
   * accordingly. If it is update or add, encode entire data. If it is rmv
   * send key information only.
   */
  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_UPDATE:
      encode_comp(&enc->io_uba, comp);
      break;

    case NCS_MBCSV_ACT_RMV:
      osaf_encode_sanamet(&enc->io_uba, &comp->comp_info.name);
      break;

    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode Operation SU name.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_oper_su(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);

  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  switch (enc->io_action) {
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_RMV:
      osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
      break;
    case NCS_MBCSV_ACT_UPDATE:
    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_node_up_info
 *
 * Purpose:  Encode avnd node up info.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_node_up_info(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_uint32(&enc->io_uba, avnd->node_info.nodeId);
    osaf_encode_saclmnodeaddresst(&enc->io_uba, &avnd->node_info.nodeAddress);
    osaf_encode_bool(&enc->io_uba, avnd->node_info.member);
    osaf_encode_satimet(&enc->io_uba, avnd->node_info.bootTimestamp);
    osaf_encode_uint64(&enc->io_uba, avnd->node_info.initialViewNumber);
    osaf_encode_uint64(&enc->io_uba, avnd->adest);
  } else
    osafassert(0);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_node_admin_state
 *
 * Purpose:  Encode avnd su admin state info.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_node_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet_o2(&enc->io_uba, avnd->name.c_str());
    osaf_encode_uint32(&enc->io_uba, avnd->saAmfNodeAdminState);
  } else
    osafassert(0);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_node_oper_state
 *
 * Purpose:  Encode avnd oper state info.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_node_oper_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet_o2(&enc->io_uba, avnd->name.c_str());
    osaf_encode_uint32(&enc->io_uba, avnd->saAmfNodeOperState);
  } else {
    osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_node_state
 *
 * Purpose:  Encode avnd node state info.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_node_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet_o2(&enc->io_uba, avnd->name.c_str());
    osaf_encode_uint32(&enc->io_uba, avnd->node_state);
  } else
    osafassert(0);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_node_rcv_msg_id
 *
 * Purpose:  Encode avnd receive message ID.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_node_rcv_msg_id(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_uint32(&enc->io_uba, avnd->node_info.nodeId);
    osaf_encode_uint32(&enc->io_uba, avnd->rcv_msg_id);
  } else
    osafassert(0);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_node_snd_msg_id
 *
 * Purpose:  Encode avnd Send message ID.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_node_snd_msg_id(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_uint32(&enc->io_uba, avnd->node_info.nodeId);
    osaf_encode_uint32(&enc->io_uba, avnd->snd_msg_id);
  } else
    osafassert(0);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SG admin state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SG *sg = (AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));
  osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
  osaf_encode_uint32(&enc->io_uba, sg->saAmfSGAdminState);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SG su assign number.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_su_assigned_num(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SG *sg = (AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));
  osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
  osaf_encode_uint32(&enc->io_uba, sg->saAmfSGNumCurrAssignedSUs);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SG su spare number.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_su_spare_num(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SG *sg = (AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));
  osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
  osaf_encode_uint32(&enc->io_uba, sg->saAmfSGNumCurrInstantiatedSpareSUs);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SG su uninst number.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_su_uninst_num(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SG *sg = (AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));
  osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
  osaf_encode_uint32(&enc->io_uba, sg->saAmfSGNumCurrNonInstantiatedSpareSUs);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SG adjust state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_adjust_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SG *sg = (AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));
  osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
  osaf_encode_uint32(&enc->io_uba, sg->adjust_state);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SG FSM state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_sg_fsm_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SG *sg = (AVD_SG *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));
  osaf_encode_sanamet_o2(&enc->io_uba, sg->name.c_str());
  osaf_encode_uint32(&enc->io_uba, sg->sg_fsm_state);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU preinstatible object.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_preinstan(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_bool(&enc->io_uba, su->saAmfSUPreInstantiable);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Operation state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_oper_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->saAmfSUOperState);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Admin state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->saAmfSUAdminState);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Rediness state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_readiness_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->saAmfSuReadinessState);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Presence state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_pres_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->saAmfSUPresenceState);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Current number of Active SI.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_si_curr_active(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->saAmfSUNumCurrActiveSIs);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Current number of Standby SI.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_si_curr_stby(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->saAmfSUNumCurrStandbySIs);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Admin state to terminate service.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_term_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_bool(&enc->io_uba, su->term_state);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU inst msg of service.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_inst_msg_processed(
                                     AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = reinterpret_cast<AVD_SU *>(enc->io_reo_hdl);
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_bool(&enc->io_uba, su->is_inst_msg_processed);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU toggle SI.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_switch(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->su_switch);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU action state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_act_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->su_act_state);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 *
 * Purpose:  Encode SU Restart count.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_restart_count(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SU *su = (AVD_SU *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
  osaf_encode_uint32(&enc->io_uba, su->saAmfSURestartCount);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_si_admin_state
 *
 * Purpose:  Encode SI admin state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_si_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
  osaf_encode_uint32(&enc->io_uba, si->saAmfSIAdminState);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_si_assignment_state
 *
 * Purpose:  Encode SI assignment state.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_si_assignment_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
  osaf_encode_uint32(&enc->io_uba, si->saAmfSIAssignmentState);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/******************************************************************
 * @brief    encodes si_dep_state during async update
 *
 * @param[in] cb
 *
 * @param[in] dec
 *
 *****************************************************************/
static uint32_t enc_si_dep_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
  osaf_encode_uint32(&enc->io_uba, si->si_dep_state);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_si_su_curr_active
 *
 * Purpose:  Encode number of active SU assignment for this SI.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_si_su_curr_active(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
  osaf_encode_uint32(&enc->io_uba, si->saAmfSINumCurrActiveAssignments);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_si_su_curr_stby
 *
 * Purpose:  Encode number of standby SU assignment for this SI.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_si_su_curr_stby(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
  osaf_encode_uint32(&enc->io_uba, si->saAmfSINumCurrStandbyAssignments);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_si_switch
 *
 * Purpose:  Encode SI switch.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_si_switch(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
  osaf_encode_uint32(&enc->io_uba, si->si_switch);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_si_alarm_sent
 *
 * Purpose:  Encode SI alarm sent
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_si_alarm_sent(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_SI *si = (AVD_SI *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, si->name.c_str());
  osaf_encode_bool(&enc->io_uba, si->alarm_sent);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}
/****************************************************************************\
 * Function: enc_comp_proxy_comp_name
 *
 * Purpose:  Encode COMP proxy compnent name.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_comp_proxy_comp_name(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  AVD_COMP *comp = (AVD_COMP *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  TRACE_ENTER();

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet(&enc->io_uba, &comp->comp_info.name);
    osaf_encode_sanamet_o2(&enc->io_uba, comp->saAmfCompCurrProxyName.c_str());
  } else {
    osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

// Function are not used
static uint32_t enc_comp_curr_num_csi_actv(AVD_CL_CB *cb,
                                           NCS_MBCSV_CB_ENC *enc) {
  LOG_NO("enc_comp_curr_num_csi_actv is deprecated");
  return NCSCC_RC_SUCCESS;
}

// Function are not used
static uint32_t enc_comp_curr_num_csi_stby(AVD_CL_CB *cb,
                                           NCS_MBCSV_CB_ENC *enc) {
  LOG_NO("enc_comp_curr_num_csi_stby is deprecated");
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_comp_oper_state
 *
 * Purpose:  Encode COMP Operation State.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_comp_oper_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  AVD_COMP *comp = (AVD_COMP *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  TRACE_ENTER();

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet(&enc->io_uba, &comp->comp_info.name);
    osaf_encode_uint32(&enc->io_uba, comp->saAmfCompOperState);
  } else {
    osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_comp_readiness_state
 *
 * Purpose:  Encode COMP Presence State.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_comp_readiness_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  AVD_COMP *comp = (AVD_COMP *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  TRACE_ENTER();

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet(&enc->io_uba, &comp->comp_info.name);
    osaf_encode_uint32(&enc->io_uba, comp->saAmfCompReadinessState);
  } else {
    osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_comp_pres_state
 *
 * Purpose:  Encode COMP Presence State.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_comp_pres_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  AVD_COMP *comp = (AVD_COMP *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  TRACE_ENTER();

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet(&enc->io_uba, &comp->comp_info.name);
    osaf_encode_uint32(&enc->io_uba, comp->saAmfCompPresenceState);

  } else {
    osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_comp_restart_count
 *
 * Purpose:  Encode COMP Restart count.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_comp_restart_count(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  AVD_COMP *comp = (AVD_COMP *)(NCS_INT64_TO_PTR_CAST(enc->io_reo_hdl));

  TRACE_ENTER();

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet(&enc->io_uba, &comp->comp_info.name);
    osaf_encode_uint32(&enc->io_uba, comp->saAmfCompRestartCount);
  } else {
    osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: avd_enc_cold_sync_rsp
 *
 * Purpose:  Encode cold sync response message.
 *
 * Input: cb  - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
uint32_t avd_enc_cold_sync_rsp(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  TRACE_LEAVE();
  return entire_data_update(cb, enc, true);
}

/****************************************************************************\
 * Function: entire_data_update
 *
 * Purpose:  Encode entire data to be sent during cold sync or warm sync.
 *
 * Input: cb  - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *        c_sync - true - Called while in cold sync.
 *                 false - Called while in warm sync.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t entire_data_update(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   bool c_sync) {
  uint32_t status = NCSCC_RC_SUCCESS;
  uint32_t num_of_obj = 0;
  uint8_t *encoded_cnt_loc;

  TRACE_ENTER2("io_reo_type '%u', c_sync '%d'", enc->io_reo_type, c_sync);

  /*
   * Since at decode we need to find out how many objects of particular data
   * type are being sent, encode that information at the begining of the
   * message.
   */
  encoded_cnt_loc = ncs_enc_reserve_space(&enc->io_uba, sizeof(uint32_t));
  if (!encoded_cnt_loc) {
    /* Handle error */
  }
  ncs_enc_claim_space(&enc->io_uba, sizeof(uint32_t));

  /*
   * If reo_handle and reo_type is nullptr then this the first time mbcsv is
   * calling the cold sync response for the standby. So start from first data
   * structure which is CB. Next time onwards depending on the value of reo_type
   * and reo_handle send the next data structures.
   */
  status = enc_cs_data_func_list[enc->io_reo_type](cb, enc, &num_of_obj);

  /* Now encode the number of objects actually in the UBA. */
  if (encoded_cnt_loc != nullptr) {
    ncs_encode_32bit(&encoded_cnt_loc, num_of_obj);
  }

  /*
   * Check if we have reached to last message required to be sent in cold sync
   * response, if yes then send cold sync complete. Else ask MBCSv to call you
   * back with the next reo_type.
   */
  if (AVSV_COLD_SYNC_RSP_ASYNC_UPDT_CNT == enc->io_reo_type) {
    if (c_sync) {
      enc->io_msg_type = NCS_MBCSV_MSG_COLD_SYNC_RESP_COMPLETE;
      saflog(LOG_NOTICE, amfSvcUsrName, "Cold sync complete of %x",
             cb->node_id_avd_other);
    } else
      enc->io_msg_type = NCS_MBCSV_MSG_DATA_RESP_COMPLETE;
    TRACE("Marking sync_state as in_sync");
    cb->stby_sync_state = AVD_STBY_IN_SYNC;
  } else
    enc->io_reo_type++;

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

/****************************************************************************\
 * Function: enc_cs_cb_config
 *
 * Purpose:  Encode entire CB data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_cb_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj) {
  TRACE_ENTER();

  /*
   * Send the CB data.
   */
  encode_cb(&enc->io_uba, cb, enc->i_peer_version);
  *num_of_obj = 1;

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_cluster_config
 *
 * Purpose:  Encode entire AVD_CLUSTER data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_cluster_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                      uint32_t *num_of_obj) {
  TRACE_ENTER();

  encode_cluster(&enc->io_uba, avd_cluster, enc->i_peer_version);
  (*num_of_obj)++;

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_node_config
 *
 * Purpose:  Encode entire AVD_AVND data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_node_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   uint32_t *num_of_obj) {
  TRACE_ENTER();

  /*
   * Walk through the entire list and send the entire list data.
   */
  for (const auto &value : *node_name_db) {
    AVD_AVND *avnd_node = value.second;
    encode_node_config(&enc->io_uba, avnd_node, enc->i_peer_version);
    (*num_of_obj)++;
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_app_config
 *
 * Purpose:  Encode entire AVD_APP data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_app_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                  uint32_t *num_of_obj) {
  TRACE_ENTER();

  /* Walk through all application instances and encode. */
  // If use std=c++11 in Makefile.common the following syntax can be used
  // instead: for (auto it = app_db->begin()
  for (const auto &value : *app_db) {
    AVD_APP *app = value.second;
    encode_app(&enc->io_uba, app);
    (*num_of_obj)++;
  }

  TRACE_LEAVE2();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_sg_config
 *
 * Purpose:  Encode entire AVD_SG data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_sg_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj) {
  uint32_t status = NCSCC_RC_SUCCESS;
  TRACE_ENTER();

  /*
   * Walk through the entire list and send the entire list data.
   */
  for (const auto &value : *sg_db) {
    AVD_SG *sg = value.second;
    encode_sg(&enc->io_uba, sg);
    (*num_of_obj)++;
  }

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

/****************************************************************************\
 * Function: enc_cs_su_config
 *
 * Purpose:  Encode entire AVD_SU data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_su_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj) {
  TRACE_ENTER();

  for (const auto &value : *su_db) {
    encode_su(&enc->io_uba, value.second, enc->i_peer_version);
    (*num_of_obj)++;
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_si_config
 *
 * Purpose:  Encode entire AVD_SI data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_si_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                 uint32_t *num_of_obj) {
  uint32_t status = NCSCC_RC_SUCCESS;
  TRACE_ENTER();

  /*
   * Walk through the entire list and send the entire list data.
   */
  for (const auto &value : *si_db) {
    const AVD_SI *si = value.second;
    encode_si(cb, &enc->io_uba, si, enc->i_peer_version);

    (*num_of_obj)++;
  }

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

/****************************************************************************\
 * Function: enc_cs_sg_su_oper_list
 *
 * Purpose:  Encode entire AVD_SG_SU_OPER_LIST data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_sg_su_oper_list(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                       uint32_t *num_of_obj) {
  uint32_t status = NCSCC_RC_SUCCESS;
  TRACE_ENTER();

  /*
   * Walk through the entire SG list and send the SU operation list
   * for that SG and then move to next SG.
   */
  for (const auto &value : *sg_db) {
    AVD_SG *sg = value.second;
    status = enc_su_oper_list(cb, sg, enc);

    if (status != NCSCC_RC_SUCCESS) {
      LOG_ER("%s: encode failed, %s", __FUNCTION__, sg->name.c_str());
      return NCSCC_RC_FAILURE;
    }

    (*num_of_obj)++;
  }

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

/****************************************************************************\
 * Function: enc_cs_sg_admin_si
 *
 * Purpose:  Encode entire AVD_SG_ADMIN_SI data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_sg_admin_si(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   uint32_t *num_of_obj) {
  uint32_t status = NCSCC_RC_SUCCESS;
  TRACE_ENTER();

  /*
   * Walk through the entire list and send the entire list data.
   */
  for (const auto &value : *sg_db) {
    AVD_SG *sg = value.second;
    if (nullptr == sg->admin_si) continue;

    osaf_encode_sanamet_o2(&enc->io_uba, sg->admin_si->name.c_str());

    (*num_of_obj)++;
  }

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

/********************************************************************
 * @brief encodes si transfer parameters during cold sync
 * @param[in] cb
 * @param[in] enc
 * @param[in] num_of_obj
 *******************************************************************/
static uint32_t enc_cs_si_trans(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                uint32_t *num_of_obj) {
  TRACE_ENTER();

  for (const auto &value : *sg_db) {
    AVD_SG *sg = value.second;
    if (sg->si_tobe_redistributed != nullptr) {
      encode_si_trans(&enc->io_uba, sg, enc->i_peer_version);
      (*num_of_obj)++;
    }
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_su_si_rel
 *
 * Purpose:  Encode entire AVD_SU_SI_REL data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
 \**************************************************************************/
static uint32_t enc_cs_siass(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                             uint32_t *num_of_obj) {
  const AVD_SU *su;
  const AVD_SU_SI_REL *rel;
  AVD_SU_SI_REL copy;
  TRACE_ENTER();

  /*
   * Walk through the entire list and send the entire list data.
   * We will walk the SU tree and all the SU_SI relationship for that SU
   * are sent.We will send the corresponding COMP_CSI relationship for that
   * SU_SI in the same update.
   */
  for (const auto &value : *su_db) {
    su = value.second;

    for (rel = su->list_of_susi; rel != nullptr; rel = rel->su_next) {
      copy = *rel;
      copy.csi_add_rem = SA_FALSE;
      encode_siass(&enc->io_uba, &copy, enc->i_peer_version);
      (*num_of_obj)++;
    }
  }
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_comp_config
 *
 * Purpose:  Encode entire AVD_COMP data.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_comp_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                   uint32_t *num_of_obj) {
  TRACE_ENTER();

  /*
   * Walk through the entire list and send the entire list data.
   */
  for (const auto &value : *comp_db) {
    AVD_COMP *comp = value.second;

    encode_comp(&enc->io_uba, comp);

    (*num_of_obj)++;
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_async_updt_cnt
 *
 * Purpose:  Send the latest async update count. This count will be used
 *           during warm sync for verifying the data at stnadby.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_async_updt_cnt(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                      uint32_t *num_of_obj) {
  EDU_ERR ederror = static_cast<EDU_ERR>(0);
  TRACE_ENTER();

  /*
   * Encode and send async update counts for all the data structures.
   */
  uint32_t status = m_NCS_EDU_VER_EXEC(
      &cb->edu_hdl, avsv_edp_ckpt_msg_async_updt_cnt, &enc->io_uba,
      EDP_OP_TYPE_ENC, &cb->async_updt_cnt, &ederror, enc->i_peer_version);

  if (status != NCSCC_RC_SUCCESS)
    LOG_ER("%s: encode failed, ederror=%u", __FUNCTION__, ederror);

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

/****************************************************************************\
 * Function: avd_enc_warm_sync_rsp
 *
 * Purpose:  Encode Warm sync response message.
 *
 * Input: cb  - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
uint32_t avd_enc_warm_sync_rsp(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  EDU_ERR ederror = static_cast<EDU_ERR>(0);
  TRACE_ENTER();

  /*
   * Encode and send latest async update counts. (In the same manner we sent
   * in the last message of the cold sync response.
   */
  uint32_t status = m_NCS_EDU_VER_EXEC(
      &cb->edu_hdl, avsv_edp_ckpt_msg_async_updt_cnt, &enc->io_uba,
      EDP_OP_TYPE_ENC, &cb->async_updt_cnt, &ederror, enc->i_peer_version);

  if (status != NCSCC_RC_SUCCESS)
    LOG_ER("%s: encode failed, ederror=%u", __FUNCTION__, ederror);

  TRACE_LEAVE2("status '%u'", status);
  return status;
}

/****************************************************************************\
 * Function: avd_enc_data_sync_rsp
 *
 * Purpose:  Encode Data sync response message.
 *
 * Input: cb  - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
uint32_t avd_enc_data_sync_rsp(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  return entire_data_update(cb, enc, false);
}

/****************************************************************************\
 * Function: enc_su_oper_list
 *
 * Purpose:  Encode entire AVD_SG_SU_OPER_LIST data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_su_oper_list(AVD_CL_CB *cb, AVD_SG *sg,
                                 NCS_MBCSV_CB_ENC *enc) {
  uint32_t num_of_opr_su = 0;
  uint8_t *encoded_cnt_loc;

  TRACE_ENTER();

  /* Reserve space for the number of operation SU to be encoded */
  encoded_cnt_loc = ncs_enc_reserve_space(&enc->io_uba, sizeof(uint32_t));
  osafassert(encoded_cnt_loc);
  ncs_enc_claim_space(&enc->io_uba, sizeof(uint32_t));

  /*
   * Now walk through the entire SU operation list and encode it.
   */
  for (const auto &su : sg->su_oper_list) {
    osaf_encode_sanamet_o2(&enc->io_uba, su->name.c_str());
    num_of_opr_su++;
  }

  /*
   * Encode the number of operation SU's we are sending in this message.
   */
  if (encoded_cnt_loc != nullptr) {
    ncs_encode_32bit(&encoded_cnt_loc, num_of_opr_su);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

void encode_comp_cs_type_config(NCS_UBAID *ub,
                                const AVD_COMPCS_TYPE *comp_cs_type,
                                const uint16_t peer_version) {
  osaf_encode_sanamet_o2(ub, comp_cs_type->name.c_str());
  osaf_encode_uint32(ub, comp_cs_type->saAmfCompNumCurrActiveCSIs);
  osaf_encode_uint32(ub, comp_cs_type->saAmfCompNumCurrStandbyCSIs);
}

/****************************************************************************\
 * Function: enc_comp_cs_type_config
 *
 * Purpose:  Encode entire AVD_COMP_CS_TYPE data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_comp_cs_type_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER2("io_action '%u'", enc->io_action);
  const AVD_COMPCS_TYPE *comp_cs_type =
      reinterpret_cast<AVD_COMPCS_TYPE *>(enc->io_reo_hdl);

  /*
   * Check for the action type (whether it is add, rmv or update) and act
   * accordingly. If it is update or add, encode entire data. If it is rmv
   * send key information only.
   */
  switch (enc->io_action) {
    case NCS_MBCSV_ACT_UPDATE:
      /* Send entire data */
      encode_comp_cs_type_config(&enc->io_uba, comp_cs_type,
                                 enc->i_peer_version);
      break;
    case NCS_MBCSV_ACT_ADD:
    case NCS_MBCSV_ACT_RMV:
      osafassert(0);
      break;
    default:
      osafassert(0);
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_cs_comp_cs_type_config
 *
 * Purpose:  Encode entire AVD_COMP_CS_TYPE data..
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_cs_comp_cs_type_config(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc,
                                           uint32_t *num_of_obj) {
  TRACE_ENTER();

  /*
   * Walk through the entire list and send the entire list data.
   */
  for (const auto &value : *compcstype_db) {
    AVD_COMPCS_TYPE *compcstype = value.second;
    encode_comp_cs_type_config(&enc->io_uba, compcstype, enc->i_peer_version);

    (*num_of_obj)++;
  }

  return NCSCC_RC_SUCCESS;
}

/**
 * @brief   encodes saAmfNGAdminState.
 *
 * @param   ptr to AVD_CL_CB
 * @param   ptr to encode structure NCS_MBCSV_CB_ENC.
 *
 * @return NCSCC_RC_SUCCESS
 */
static uint32_t enc_ng_admin_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const AVD_AMF_NG *ng = (AVD_AMF_NG *)enc->io_reo_hdl;
  osaf_encode_sanamet_o2(&enc->io_uba, ng->name.c_str());
  osaf_encode_uint32(&enc->io_uba, ng->saAmfNGAdminState);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

static uint32_t enc_avd_to_avd_job_queue_status(AVD_CL_CB *cb,
                                                NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();
  osafassert(NCS_MBCSV_ACT_UPDATE == enc->io_action);
  const uint32_t *size = reinterpret_cast<uint32_t *>(enc->io_reo_hdl);
  osaf_encode_uint32(&enc->io_uba, *size);
  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

static uint32_t enc_node_failover_state(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    auto failed_node = cb->failover_list.find(avnd->node_info.nodeId);
    if (failed_node != cb->failover_list.end()) {
      osaf_encode_sanamet_o2(&enc->io_uba, avnd->name.c_str());
      TRACE_ENTER2("New state '%u'", failed_node->second->GetState());
      osaf_encode_uint32(&enc->io_uba, failed_node->second->GetState());
    }
  } else {
    osafassert(0);
  }

  return NCSCC_RC_SUCCESS;
}

/****************************************************************************\
 * Function: enc_recvr_node_fover_swover
 *
 * Purpose:  Encode node failover/switchover recovery.
 *
 * Input: cb - CB pointer.
 *        enc - Encode arguments passed by MBCSV.
 *
 * Returns: NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * NOTES:
 *
 *
\**************************************************************************/
static uint32_t enc_recvr_node_fover_swover(AVD_CL_CB *cb, NCS_MBCSV_CB_ENC *enc) {
  TRACE_ENTER();

  const AVD_AVND *avnd = reinterpret_cast<AVD_AVND *>(enc->io_reo_hdl);

  /*
   * Action in this case is just to update. If action passed is add/rmv then log
   * error. Call EDU encode to encode this field.
   */
  if (NCS_MBCSV_ACT_UPDATE == enc->io_action) {
    osaf_encode_sanamet_o2(&enc->io_uba, avnd->name.c_str());
    osaf_encode_uint32(&enc->io_uba, avnd->recvr_fail_sw);
  } else
    osafassert(0);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}
