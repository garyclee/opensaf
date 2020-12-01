/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
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
 * Author(s): Emerson Network Power
 *
 */

/*****************************************************************************
..............................................................................

..............................................................................

  DESCRIPTION:

  This file contains declarations fro AvD interface.

******************************************************************************
*/

#ifndef AMF_AMFND_AVND_DI_H_
#define AMF_AMFND_AVND_DI_H_

#include "amf/common/amf_si_assign.h"

struct avnd_cb_tag;

uint32_t avnd_di_oper_send(struct avnd_cb_tag *, const AVND_SU *, uint32_t);
uint32_t avnd_di_susi_resp_send(struct avnd_cb_tag *, AVND_SU *,
                                AVND_SU_SI_REC *);
uint32_t avnd_di_object_upd_send(struct avnd_cb_tag *, AVSV_PARAM_INFO *);
uint32_t avnd_di_pg_act_send(struct avnd_cb_tag *, const std::string &,
                             AVSV_PG_TRACK_ACT, bool);
uint32_t avnd_di_msg_send(struct avnd_cb_tag *, AVND_MSG *);
void avnd_di_msg_ack_process(struct avnd_cb_tag *, uint32_t);
void avnd_diq_rec_check_buffered_msg(struct avnd_cb_tag *);
AVND_DND_MSG_LIST *avnd_diq_rec_add(struct avnd_cb_tag *cb, AVND_MSG *msg);
void avnd_diq_rec_del(struct avnd_cb_tag *cb, AVND_DND_MSG_LIST *rec);
void avnd_diq_rec_send_buffered_msg(struct avnd_cb_tag *cb, bool only_ncs);
uint32_t avnd_diq_rec_send(struct avnd_cb_tag *cb, AVND_DND_MSG_LIST *rec);
uint32_t avnd_di_reg_su_rsp_snd(struct avnd_cb_tag *cb,
                                const std::string &su_name, uint32_t ret_code);
uint32_t avnd_di_node_down_msg_send(struct avnd_cb_tag *cb);
uint32_t avnd_di_ack_nack_msg_send(struct avnd_cb_tag *cb, uint32_t rcv_id,
                                   uint32_t view_num);
extern void avnd_di_uns32_upd_send(int class_id, int attr_id,
                                   const std::string &dn, uint32_t value);
extern uint32_t avnd_di_resend_pg_start_track(struct avnd_cb_tag *);
void avnd_sync_sisu(struct avnd_cb_tag *cb);
void avnd_sync_csicomp(struct avnd_cb_tag *cb);

#endif  // AMF_AMFND_AVND_DI_H_
