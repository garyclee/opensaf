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
 * Author(s): Ribbon Communications, Inc.
 *
 */

#ifndef PLM_COMMON_PLM_COMMON_H_
#define PLM_COMMON_PLM_COMMON_H_

#include "base/ncs_edu_pub.h"
#include "mds/mds_papi.h"
#include "plms_evt.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLMS_MDS_SYNC_TIME 1500

#define PLMA_MDS_PVT_SUBPART_VERSION 1
/*PLMA - PLMS communication */
#define PLMA_WRT_PLMS_SUBPART_VER_MIN 1
#define PLMA_WRT_PLMS_SUBPART_VER_MAX 1

#define PLMA_WRT_PLMS_SUBPART_VER_RANGE \
  (PLMA_WRT_PLMS_SUBPART_VER_MAX - PLMA_WRT_PLMS_SUBPART_VER_MIN + 1)

uint32_t plm_mds_msg_sync_send(MDS_HDL mds_hdl, uint32_t from_svc,
                               uint32_t to_svc, MDS_DEST to_dest,
                               PLMS_EVT *i_evt, PLMS_EVT **o_evt,
                               SaTimeT timeout);
uint32_t plms_mds_normal_send(MDS_HDL mds_hdl, NCSMDS_SVC_ID from_svc,
                              NCSCONTEXT evt, MDS_DEST dest,
                              NCSMDS_SVC_ID to_svc);
SaUint32T plms_free_evt(PLMS_EVT *evt);
uint32_t plms_mds_enc(MDS_CALLBACK_ENC_INFO *info, EDU_HDL *edu_hdl);
uint32_t plms_mds_dec(MDS_CALLBACK_DEC_INFO *info, EDU_HDL *edu_hdl);
uint32_t plms_mds_enc_flat(struct ncsmds_callback_info *info, EDU_HDL *edu_hdl);
uint32_t plms_mds_dec_flat(struct ncsmds_callback_info *info, EDU_HDL *edu_hdl);

#ifdef __cplusplus
}
#endif

#endif
