/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2022 The OpenSAF Foundation
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

#include "mds/mds_adest_op.h"
#include "mds/mds_log.h"

void mds_adest_op_down_timer_start(MDS_DEST adest, MDS_SVC_ID svc_id) {
	MDS_TMR_REQ_INFO *tmr_req_info = calloc(1, sizeof(MDS_TMR_REQ_INFO));
	if (tmr_req_info == NULL) {
		m_MDS_LOG_ERR("mds_mcm_svc_down out of memory\n");
		abort();
	}

	tmr_req_info->type = MDS_DOWN_TMR;
	tmr_req_info->info.down_event_tmr_info.adest = adest;
	tmr_req_info->info.down_event_tmr_info.svc_id = svc_id;

	tmr_t tmr_id = ncs_tmr_alloc(__FILE__, __LINE__);
	if (tmr_id == NULL) {
		m_MDS_LOG_ERR("mds_mcm_svc_down out of memory\n");
		abort();
	}

	tmr_req_info->info.down_event_tmr_info.tmr_id = tmr_id;

	uint32_t tmr_hdl = ncshm_create_hdl(NCS_HM_POOL_ID_COMMON,
					    NCS_SERVICE_ID_COMMON,
					    (NCSCONTEXT)(tmr_req_info));

	if (svc_id == 0) {
		MDS_ADEST_INFO *adest_info =
			(MDS_ADEST_INFO *) ncs_patricia_tree_get(
				&gl_mds_mcm_cb->adest_list, (uint8_t *)&adest);
		if (adest_info) {
			adest_info->tmr_req_info = tmr_req_info;
			adest_info->tmr_hdl = tmr_hdl;
		}
	}
	tmr_id = ncs_tmr_start(tmr_id, MDS_DOWN_TMR_VAL,
			       (TMR_CALLBACK)mds_tmr_callback,
			       (void *)(long)(tmr_hdl), __FILE__, __LINE__);
	assert(tmr_id != NULL);
}

void mds_adest_op_down_timer_stop(MDS_ADEST_INFO *adest_info) {
	assert(adest_info != NULL);
	if (adest_info->tmr_req_info) {
		MDS_TMR_REQ_INFO *tmr_req_info = adest_info->tmr_req_info;
		ncs_tmr_stop(tmr_req_info->info.down_event_tmr_info.tmr_id);
		ncs_tmr_free(tmr_req_info->info.down_event_tmr_info.tmr_id);
		m_MMGR_FREE_TMR_INFO(tmr_req_info);
		adest_info->tmr_req_info = NULL;
		ncshm_destroy_hdl(NCS_SERVICE_ID_COMMON,
				  (uint32_t)adest_info->tmr_hdl);
	}
}

void mds_adest_op_list_cleanup(void) {
	MDS_ADEST_INFO *adest_info = NULL;

	adest_info = (MDS_ADEST_INFO *)ncs_patricia_tree_getnext(
				&gl_mds_mcm_cb->adest_list, NULL);
	while (adest_info != NULL) {
		mds_adest_op_down_timer_stop(adest_info);
		ncs_patricia_tree_del(&gl_mds_mcm_cb->adest_list,
				      (NCS_PATRICIA_NODE *)adest_info);
		m_MMGR_FREE_ADEST_INFO(adest_info);
		adest_info = (MDS_ADEST_INFO *)ncs_patricia_tree_getnext(
					&gl_mds_mcm_cb->adest_list, NULL);
	}
}
