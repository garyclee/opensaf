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
..............................................................................

..............................................................................

  DESCRIPTION:

  This module is the include file for Availability Directors checkpointing.

******************************************************************************
*/

/*
 * Module Inclusion Control...
 */
#ifndef AMF_AMFD_CKPT_H_
#define AMF_AMFD_CKPT_H_

// current version
#define AVD_MBCSV_SUB_PART_VERSION 12

// supported versions
#define AVD_MBCSV_SUB_PART_VERSION_12 12
#define AVD_MBCSV_SUB_PART_VERSION_11 11
#define AVD_MBCSV_SUB_PART_VERSION_10 10
#define AVD_MBCSV_SUB_PART_VERSION_9 9
#define AVD_MBCSV_SUB_PART_VERSION_8 8
#define AVD_MBCSV_SUB_PART_VERSION_7 7
#define AVD_MBCSV_SUB_PART_VERSION_6 6
#define AVD_MBCSV_SUB_PART_VERSION_5 5
#define AVD_MBCSV_SUB_PART_VERSION_4 4
#define AVD_MBCSV_SUB_PART_VERSION_3 3
#define AVD_MBCSV_SUB_PART_VERSION_2 2
#define AVD_MBCSV_SUB_PART_VERSION_MIN 1

struct AVD_EVT;
struct cl_cb_tag;
class AVD_APP;
class AVD_COMP;
class AVD_SG;
class AVD_AVND;
/*
 * SU SI Relationship checkpoint encode/decode message structure..
 */
typedef struct avsv_su_si_rel_ckpt_msg {
  SaNameT su_name;
  SaNameT si_name;
  SaAmfHAStateT state;
  uint32_t fsm; /* The SU SI FSM state */
  SaBoolT csi_add_rem;
  SaNameT comp_name;
  SaNameT csi_name;
} AVSV_SU_SI_REL_CKPT_MSG;

/*
 * SI transfer fields checkpoint encode/decode message structure
 */
typedef struct avsv_si_trans_ckpt_msg {
  SaNameT sg_name;
  SaNameT si_name;
  SaNameT min_su_name;
  SaNameT max_su_name;
} AVSV_SI_TRANS_CKPT_MSG;

/*
 * Async Update message queue.
 */
typedef struct avsv_async_updt_msg_queue {
  struct avsv_async_updt_msg_queue *next;

  NCS_MBCSV_CB_DEC dec;
} AVSV_ASYNC_UPDT_MSG_QUEUE;

typedef struct avsv_async_updt_msg_queue_list {
  AVSV_ASYNC_UPDT_MSG_QUEUE *async_updt_queue;
  AVSV_ASYNC_UPDT_MSG_QUEUE *tail; /* Tail of the queue */
} AVSV_ASYNC_UPDT_MSG_QUEUE_LIST;

/*
 * Async update count. It will be used for warm sync verification.
 */
typedef struct avsv_async_updt_cnt {
  uint32_t cb_updt;
  uint32_t node_updt;
  uint32_t app_updt;
  uint32_t sg_updt;
  uint32_t su_updt;
  uint32_t si_updt;
  uint32_t sg_su_oprlist_updt;
  uint32_t sg_admin_si_updt;
  uint32_t siass_updt;
  uint32_t comp_updt;
  uint32_t csi_updt;
  uint32_t compcstype_updt;
  uint32_t si_trans_updt;
  uint32_t ng_updt;
  uint32_t failover_updt;
} AVSV_ASYNC_UPDT_CNT;

/*
 * Prototype for the AVSV checkpoint encode function pointer.
 */
typedef uint32_t (*AVSV_ENCODE_CKPT_DATA_FUNC_PTR)(struct cl_cb_tag *cb,
                                                   NCS_MBCSV_CB_ENC *enc);

/*
 * Prototype for the AVSV checkpoint Decode function pointer.
 */
typedef uint32_t (*AVSV_DECODE_CKPT_DATA_FUNC_PTR)(struct cl_cb_tag *cb,
                                                   NCS_MBCSV_CB_DEC *dec);

/*
 * Prototype for the AVSV checkpoint cold sync response encode function pointer.
 */
typedef uint32_t (*AVSV_ENCODE_COLD_SYNC_RSP_DATA_FUNC_PTR)(
    struct cl_cb_tag *cb, NCS_MBCSV_CB_ENC *enc, uint32_t *num_of_obj);

/*
 * Prototype for the AVSV checkpoint cold sync response encode function pointer.
 */
typedef uint32_t (*AVSV_DECODE_COLD_SYNC_RSP_DATA_FUNC_PTR)(
    struct cl_cb_tag *cb, NCS_MBCSV_CB_DEC *enc, uint32_t num_of_obj);

/* Function Definations of avd_chkop.c */
uint32_t avd_active_role_initialization(struct cl_cb_tag *cb,
                                        SaAmfHAStateT role);
uint32_t avd_standby_role_initialization(struct cl_cb_tag *cb);
void avd_role_change_evh(struct cl_cb_tag *cb, struct AVD_EVT *evt);
uint32_t avsv_mbcsv_register(struct cl_cb_tag *cb);
uint32_t avsv_mbcsv_deregister(struct cl_cb_tag *cb);
uint32_t avsv_set_ckpt_role(struct cl_cb_tag *cb, uint32_t role);
uint32_t avsv_mbcsv_dispatch(struct cl_cb_tag *cb, uint32_t flag);
uint32_t avsv_send_ckpt_data(struct cl_cb_tag *cb, uint32_t action,
                             MBCSV_REO_HDL reo_hdl, uint32_t reo_type,
                             uint32_t send_type);
uint32_t avsv_mbcsv_obj_set(struct cl_cb_tag *cb, uint32_t obj, uint32_t val);
uint32_t avsv_send_data_req(struct cl_cb_tag *cb);
uint32_t avsv_dequeue_async_update_msgs(struct cl_cb_tag *cb, bool pr_or_fr);

/* Function Definations of avd_ckpt_enc.c */
uint32_t avd_enc_cold_sync_rsp(struct cl_cb_tag *cb, NCS_MBCSV_CB_ENC *enc);
uint32_t avd_enc_warm_sync_rsp(struct cl_cb_tag *cb, NCS_MBCSV_CB_ENC *enc);
uint32_t avd_enc_data_sync_rsp(struct cl_cb_tag *cb, NCS_MBCSV_CB_ENC *enc);

/* Function Definations of avd_ckpt_dec.c */
uint32_t avd_dec_cold_sync_rsp(struct cl_cb_tag *cb, NCS_MBCSV_CB_DEC *dec);
uint32_t avd_dec_warm_sync_rsp(struct cl_cb_tag *cb, NCS_MBCSV_CB_DEC *dec);
uint32_t avd_dec_data_sync_rsp(struct cl_cb_tag *cb, NCS_MBCSV_CB_DEC *dec);
uint32_t avd_dec_data_req(struct cl_cb_tag *cb, NCS_MBCSV_CB_DEC *dec);
uint32_t avd_avnd_send_role_change(struct cl_cb_tag *cb, NODE_ID,
                                   uint32_t role);
void encode_app(NCS_UBAID *ub, const AVD_APP *app);
void decode_app(NCS_UBAID *ub, AVD_APP *app);
void encode_comp(NCS_UBAID *ub, const AVD_COMP *comp);
void decode_comp(NCS_UBAID *ub, AVD_COMP *comp);
void encode_cb(NCS_UBAID *ub, const struct cl_cb_tag *cb,
               const uint16_t peer_version);
void decode_cb(NCS_UBAID *ub, struct cl_cb_tag *cb,
               const uint16_t peer_version);

void encode_siass(NCS_UBAID *ub, const struct avd_su_si_rel_tag *susi,
                  const uint16_t peer_version);
void decode_siass(NCS_UBAID *ub, struct avsv_su_si_rel_ckpt_msg *susi,
                  const uint16_t peer_version);
void encode_si_trans(NCS_UBAID *ub, const AVD_SG *sg,
                     const uint16_t peer_version);
void decode_si_trans(NCS_UBAID *ub, AVSV_SI_TRANS_CKPT_MSG *msg,
                     const uint16_t peer_version);

void encode_node_config(NCS_UBAID *ub, const AVD_AVND *avnd,
                        const uint16_t peer_version);
void decode_node_config(NCS_UBAID *ub, AVD_AVND *avnd,
                        const uint16_t peer_version);

#endif  // AMF_AMFD_CKPT_H_
