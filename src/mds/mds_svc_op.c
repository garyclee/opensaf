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

#include "mds/mds_svc_op.h"
#include "mds/mds_adest_op.h"
#include "mds/mds_log.h"

static uint32_t mds_svc_op_adest_nway_add(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_adest_nway_delete(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_mxn_active_add(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_mxn_standby_add(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_mxn_active_update(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_mxn_standby_update(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_mxn_active_delete(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_mxn_standby_delete(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_nway_active_add(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_nway_standby_add(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_nway_active_update(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_nway_standby_update(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_nway_active_delete(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_vdest_nway_standby_delete(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_subtn_res_tbl_add(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_subtn_res_tbl_del(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_subtn_res_tbl_change_role(const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_subtn_res_tbl_add_active(const MCM_SVC_INFO *info,
			MDS_SUBSCRIPTION_RESULTS_INFO *subtn_res_info);
static uint32_t mds_svc_op_subtn_res_tbl_change_active(
			const MCM_SVC_INFO *info,
			MDS_SUBSCRIPTION_RESULTS_INFO *subtn_res_info);
static uint32_t mds_svc_op_subtn_res_tbl_remove_active(
			const MCM_SVC_INFO *info);
static uint32_t mds_svc_op_user_callback(const char *tag, NCSMDS_CHG change,
			const MCM_SVC_INFO *info);
static const char* mds_svc_op_change_get_name(NCSMDS_CHG change);
static void mds_svc_op_chg_dump(const MCM_SVC_INFO *info);
static MDS_VIEW mds_svc_op_get_view(const MCM_SVC_INFO *info);

const static char * INSTALL_TAG = "svc_install";
const static char * UNINSTALL_TAG = "svc_uninstall";
const static char * SUBSCRIBE_TAG = "svc_subscribe";
const static char * UNSUBSCRIBE_TAG = "svc_unsubscribe";
const static char * UP_TAG = "svc_up";
const static char * DOWN_TAG = "svc_down";

#define MDS_SVC_CHG_LOG_ERROR(tag, info, format, args...)                     \
	m_MDS_LOG_ERR("MCM:API: %s : " format " svc_id = %s(%d)",             \
		      tag,                                                    \
		      ##args,                                                 \
		      get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(            \
		                    (info)->local_svc_hdl)),                  \
		      m_MDS_GET_SVC_ID_FROM_SVC_HDL((info)->local_svc_hdl))

#define MDS_SVC_CHG_LOG_INFO(info, format, args...)                           \
	m_MDS_LOG_INFO("MCM:API: %s : " format " svc_id = %s(%d)",            \
			__FUNCTION__,                                         \
			##args,                                               \
			get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(          \
			              (info)->local_svc_hdl)),                \
			m_MDS_GET_SVC_ID_FROM_SVC_HDL((info)->local_svc_hdl))

#define MDS_SVC_CHG_LOG_FULL_INFO(change, info, format, args...)              \
	do {                                                                  \
		char to_adest_details[MDS_MAX_PROCESS_NAME_LEN];              \
		memset(to_adest_details, 0, MDS_MAX_PROCESS_NAME_LEN);        \
		get_subtn_adest_details(                                      \
			m_MDS_GET_PWE_HDL_FROM_SVC_HDL((info)->local_svc_hdl),\
			(info)->svc_id, (info)->adest, to_adest_details);     \
		m_MDS_LOG_INFO(                                               \
			"MCM:API: %s : "  format  " svc_id = %s(%d) on DEST"  \
			" id = %d got %s for svc_id = %s(%d) on Vdest id"     \
			" = %d on Adest = %s, rem_svc_pvt_ver=%d,"            \
			" rem_svc_archword=%d",                               \
			__FUNCTION__,                                         \
			##args,                                               \
			get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(          \
			              (info)->local_svc_hdl)),                \
			m_MDS_GET_SVC_ID_FROM_SVC_HDL((info)->local_svc_hdl), \
			m_MDS_GET_VDEST_ID_FROM_SVC_HDL(                      \
				(info)->local_svc_hdl),                       \
			mds_svc_op_change_get_name(change),                   \
			get_svc_names((info)->svc_id),                        \
			(info)->svc_id,                                       \
			(info)->vdest,                                        \
			to_adest_details,                                     \
			(info)->sub_part_ver,                                 \
			(info)->arch_word);                                   \
	} while(0)

#define MDS_SVC_LOG_ERROR(tag, info, format, args...)                         \
	m_MDS_LOG_ERR("MCM:API: %s : svc_id = %s(%d)"                         \
		      " on VDEST id = %d: " format,                           \
		      tag,                                                    \
		      get_svc_names((info)->i_svc_id),                        \
		      (info)->i_svc_id,                                       \
		      m_MDS_GET_VDEST_ID_FROM_PWE_HDL((info)->i_mds_hdl),     \
		      ##args)

#define MDS_SVC_LOG_INFO(tag, info, format, args...)                          \
	m_MDS_LOG_INFO("MCM:API: %s : svc_id = %s(%d)"                        \
		       " on VDEST id = %d : " format,                         \
		       tag,                                                   \
		       get_svc_names((info)->i_svc_id),                       \
		       (info)->i_svc_id,                                      \
		       m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl),      \
		       ##args);

uint32_t mds_svc_op_install(NCSMDS_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	V_DEST_RL local_vdest_role;
	NCS_VDEST_TYPE local_vdest_policy;
	MDS_VDEST_ID local_vdest_id = 0;
	local_vdest_id = m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl);

	m_MDS_ENTER();
	if ((info->i_svc_id > NCSMDS_MAX_SVCS) || (info->i_svc_id == 0)) {
		MDS_SVC_LOG_ERROR(INSTALL_TAG, info,
				  "Service wasn't in prescribed range");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	if (info->info.svc_install.i_install_scope < NCSMDS_SCOPE_INTRANODE ||
		info->info.svc_install.i_install_scope > NCSMDS_SCOPE_NONE) {
		MDS_SVC_LOG_ERROR(INSTALL_TAG, info,
			"Should use the proper scope of installation");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	if (info->info.svc_install.i_mds_q_ownership == false) {
		if (info->i_svc_id >= NCSMDS_SVC_ID_EXTERNAL_MIN) {
			MDS_SVC_LOG_ERROR(INSTALL_TAG, info,
				"Should use the MDS Q Ownership model");
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		}
	}

	status = mds_svc_tbl_query((MDS_PWE_HDL)info->i_mds_hdl,
				   info->i_svc_id);
	if (status == NCSCC_RC_SUCCESS) {
		/* Service already exist */
		MDS_SVC_LOG_ERROR(INSTALL_TAG, info, "Service already exist");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/*    STEP 1: Create a SVC-TABLE entry
		INPUT
		-   Pwe_Hdl
		-   SVC_ID
		-   SCOPE
		-   Q-ownership Flag
		-   Selection-obj (if any)
		-   Callback Ptr.
		Output
		-   Svc_Hdl
	*/
	/* Add new service entry to SVC Table */
	if (mds_svc_tbl_add(info) != NCSCC_RC_SUCCESS) {
		MDS_SVC_LOG_ERROR(INSTALL_TAG, info,
				  "Failed to add new service entry");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/*    STEP 2: if (Q-ownership=MDS)
			Create SYSF_MBX */
	/* Taken care by db function */

	/*    STEP 3: Call mdtm_svc_install(svc_hdl) */

	/* Get current role of VDEST */
	mds_vdest_tbl_get_role(
		m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(info->i_mds_hdl),
		&local_vdest_role);
	mds_vdest_tbl_get_policy(
		m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(info->i_mds_hdl),
		&local_vdest_policy);

	if (local_vdest_role == V_DEST_RL_QUIESCED) {
		/* If current role of VDEST is Quiesced install with Standby */
		local_vdest_role = V_DEST_RL_STANDBY;
	}
	/* Inform MDTM */
	status = mds_mdtm_svc_install(
			m_MDS_GET_PWE_ID_FROM_PWE_HDL(info->i_mds_hdl),
			info->i_svc_id, info->info.svc_install.i_install_scope,
			local_vdest_role,
			m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(info->i_mds_hdl),
			local_vdest_policy,
			info->info.svc_install.i_mds_svc_pvt_ver);
	if (status != NCSCC_RC_SUCCESS) {
		/* MDTM can't bind the service */

		/* Remove SVC info from MCM database */
		mds_svc_tbl_del((MDS_PWE_HDL)info->i_mds_hdl,
				info->i_svc_id,
				NULL); /* Last argument is NULL as no one
					  else will be able to add in
					  mailbox without getting lock */
		MDS_SVC_LOG_ERROR(INSTALL_TAG, info, "MDTM returned failure");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/* Perform Another bind as current VDEST role is Active */
	if (local_vdest_role == V_DEST_RL_ACTIVE &&
			local_vdest_id != m_VDEST_ID_FOR_ADEST_ENTRY) {
		status = mds_mdtm_svc_install(
			m_MDS_GET_PWE_ID_FROM_PWE_HDL(info->i_mds_hdl),
			info->i_svc_id, info->info.svc_install.i_install_scope,
			local_vdest_role,
			m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(info->i_mds_hdl),
			local_vdest_policy,
			info->info.svc_install.i_mds_svc_pvt_ver);
		if (status != NCSCC_RC_SUCCESS) {
			/* MDTM can't bind the service second time */

			/* Neither delete service nor return Failure as first
			   bind is already successful and second bind any way
			   we ignore on the other end (subscriber end) so
			   impact is minimal */
			MDS_SVC_LOG_ERROR(INSTALL_TAG, info,
				"Second install : MDTM returned failure");
		}
	}

	MDS_SVC_LOG_INFO(INSTALL_TAG, info, "Install successful with"
					    " svc_pvt_ver = %d",
			 info->info.svc_install.i_mds_svc_pvt_ver);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_uninstall(const NCSMDS_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_SVC_HDL svc_hdl;
	MDS_SUBSCRIPTION_INFO *temp_subtn_info;
	NCSMDS_INFO temp_ncsmds_info;
	MDS_SVC_INFO *svc_cb = NULL;
	MDS_VDEST_ID vdest_id;
	V_DEST_RL vdest_role;
	NCS_VDEST_TYPE vdest_policy;
	MDS_SVC_ID svc_id_max1[1]; /* Max 1 element */

	MDS_MCM_SYNC_SEND_QUEUE *q_hdr = NULL, *prev_mem = NULL;
	MDS_VDEST_ID local_vdest_id = 0;

	local_vdest_id = m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl);

	m_MDS_ENTER();
	/* STEP 1: From Input get svc_hdl */
	/* STEP 2: Search subscr_hdl from LOCAL-UNIQ-SUBSCRIPTION-REQUEST Table
	and make PEER_SVC_ID_LIST

	if (PEER_SVC_ID_LIST != NULL)
		{
		call mds_unsubscribe(vdest_hdl/pwe_hdl,
	svc_id(self),PEER_SVC_ID_LIST) (MDS API)
		}*/

	/* Check whether service exist */
	status = mds_svc_tbl_query((MDS_PWE_HDL)info->i_mds_hdl,
				   info->i_svc_id);
	if (status == NCSCC_RC_FAILURE) {
		/* Service doesn't exist */
		MDS_SVC_LOG_ERROR(UNINSTALL_TAG, info,
		                  "Service doesn't exist");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	temp_ncsmds_info.i_mds_hdl = info->i_mds_hdl;
	temp_ncsmds_info.i_svc_id = info->i_svc_id;
	temp_ncsmds_info.i_op = MDS_CANCEL;
	temp_ncsmds_info.info.svc_cancel.i_num_svcs = 1;
	temp_ncsmds_info.info.svc_cancel.i_svc_ids =
			svc_id_max1; /* Storage associated */

	svc_hdl = m_MDS_GET_SVC_HDL_FROM_PWE_HDL_AND_SVC_ID(
			(MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id);

	while (mds_svc_tbl_get_first_subscription(svc_hdl, &temp_subtn_info)
			!= NCSCC_RC_FAILURE) {
		temp_ncsmds_info.info.svc_cancel.i_svc_ids[0] =
			temp_subtn_info->sub_svc_id;
		mds_mcm_svc_unsubscribe(&temp_ncsmds_info);
		/* this will remove subscription entry so it will
		   get next result in first_subscription query */
	}

	/* STEP 3: mdtm_svc_uninstall(vdest-id, pwe_id, svc_id, svc_hdl) */

	if (mds_svc_tbl_get((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id,
			(NCSCONTEXT*)&svc_cb) != NCSCC_RC_SUCCESS) {
		/* Service doesn't exist */
		MDS_SVC_LOG_ERROR(UNINSTALL_TAG, info,
				  "Service doesn't exist");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	vdest_id = m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl);
	mds_vdest_tbl_get_role(vdest_id, &vdest_role);
	mds_vdest_tbl_get_policy(vdest_id, &vdest_policy);

	status = mds_mdtm_svc_uninstall(
			m_MDS_GET_PWE_ID_FROM_PWE_HDL(info->i_mds_hdl),
			info->i_svc_id, svc_cb->install_scope, vdest_role,
			vdest_id, vdest_policy, svc_cb->svc_sub_part_ver);
	if (status != NCSCC_RC_SUCCESS) {
		/* MDTM can't unsubscribe the service */
		MDS_SVC_LOG_ERROR(UNINSTALL_TAG, info,
				  "MDTM returned Failure");
	}
	/* Perform Another Unbind as current VDEST role is Active */
	if (vdest_role == V_DEST_RL_ACTIVE &&
			local_vdest_id != m_VDEST_ID_FOR_ADEST_ENTRY) {
		status = mds_mdtm_svc_uninstall(
				m_MDS_GET_PWE_ID_FROM_PWE_HDL(info->i_mds_hdl),
				info->i_svc_id, svc_cb->install_scope,
				vdest_role, vdest_id, vdest_policy,
				svc_cb->svc_sub_part_ver);
		if (status != NCSCC_RC_SUCCESS) {
			/* MDTM can't unsubscribe the service */
			MDS_SVC_LOG_ERROR(UNINSTALL_TAG, info,
				"Second Uninstall : MDTM returned Failure");
		}
	}

	/* STEP 4: if (Q-ownership==MDS)
		   Destroy SYSF_MBX */

	/* Destroying MBX taken care by DB */

	/* Raise the selection object for the sync send Q and free the
	   memory */
	q_hdr = svc_cb->sync_send_queue;
	while (q_hdr != NULL) {
		prev_mem = q_hdr;
		q_hdr = q_hdr->next_send;
		m_NCS_SEL_OBJ_IND(&prev_mem->sel_obj);
		m_MMGR_FREE_SYNC_SEND_QUEUE(prev_mem);
	}
	svc_cb->sync_send_queue = NULL;

	/* STEP 5: Delete a SVC-TABLE entry and do the node unsubscribe */

	if (svc_cb->i_node_subscr) {
		if (mds_mdtm_node_unsubscribe(svc_cb->node_subtn_ref_val)
				!= NCSCC_RC_SUCCESS) {
			MDS_SVC_LOG_ERROR(UNINSTALL_TAG, info,
					  "Node failed to unsubscribe");
		}
	}
	MDS_SVC_LOG_INFO(UNINSTALL_TAG, info, "Uninstall successful with"
					      " svc_pvt_ver = %d",
			 svc_cb->svc_sub_part_ver);

	mds_svc_tbl_del((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id,
			info->info.svc_uninstall.i_msg_free_cb);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_subscribe(const NCSMDS_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	NCSMDS_SCOPE_TYPE install_scope;
	MDS_SVC_HDL svc_hdl;
	MDS_VIEW view;
	uint32_t i = 0;
	NCSMDS_INFO unsubscribe_info;

	m_MDS_ENTER();
	/* STEP 1: Validation pass:
		- Check that "scope" is smaller or equal to install-scope
	  of SVC_ID
		- For each SVC in PEER_SVCID_LIST, check that there is no
	  subscription already existing (with same or different SCOPE or VIEW).
		  If any return failure. */

	status = mds_svc_tbl_query((MDS_PWE_HDL)info->i_mds_hdl,
				   info->i_svc_id);
	if (status == NCSCC_RC_FAILURE) {
		/* Service doesn't exist */
		MDS_SVC_LOG_ERROR(SUBSCRIBE_TAG, info,
				  "Service doesn't exist");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
	mds_svc_tbl_get_install_scope((MDS_PWE_HDL)info->i_mds_hdl,
				      info->i_svc_id, &install_scope);
	if (info->info.svc_subscribe.i_scope > install_scope ||
		info->info.svc_subscribe.i_scope < NCSMDS_SCOPE_INTRANODE) {
		/* Subscription scope is bigger than Install scope */
		MDS_SVC_LOG_ERROR(SUBSCRIBE_TAG, info,
		                  "Subscription Scope Mismatch");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
	mds_svc_tbl_get_svc_hdl((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id,
	                        &svc_hdl);
	if (info->i_op == MDS_SUBSCRIBE) {
		view = MDS_VIEW_NORMAL;
	} else { /* It is MDS_RED_SUBSCRIBE */
		view = MDS_VIEW_RED;
	}

	/* STEP 2: For each element of PEER_SVC_IDLIST:
	   if (entry doesn't exist in SUBSCR-TABLE)
		{
		STEP 2.a: Create SUBSCR-TABLE entry
			INPUT
			-   SUB_SVCID
			-   PWE_ID
			-   SCOPE
			-   VIEW=NORMAL_VIEW/RED_VIEW
			-   Svc_Hdl
			OUTPUT
			-   Subscr_req_hdl
		STEP 2.b: Start Subscription Timer ( done in DB )
		STEP 2.c: Call mdtm_svc_subscribe(sub_svc_id, scope, svc_hdl)
		}
		else
		{
		if(Subscription is Implicit)
			change the Flag to Explicit.
		}
	*/
	/* Validate whether subscription array gives is not null when count is
	 * non zero */
	if (info->info.svc_subscribe.i_num_svcs != 0 &&
			info->info.svc_subscribe.i_svc_ids == NULL) {
		MDS_SVC_LOG_ERROR(SUBSCRIBE_TAG, info,
				  "Gave non zero count and"
				  " NULL subscription array");
		return NCSCC_RC_FAILURE;
	}

	/* Subscription Validation */
	for (i = 0; i < info->info.svc_subscribe.i_num_svcs; i++) {
		if (info->info.svc_subscribe.i_svc_ids[i] > NCSMDS_MAX_SVCS ||
			info->info.svc_subscribe.i_svc_ids[i] == 0) {
			MDS_SVC_LOG_ERROR(
				SUBSCRIBE_TAG, info,
				"Subscription to svc_id = %s(%d) FAILED |"
				" not in prescribed range",
				get_svc_names(info->info.svc_subscribe
				              .i_svc_ids[i]),
				info->info.svc_subscribe.i_svc_ids[i]);
			return NCSCC_RC_FAILURE;
		}

		status = mds_subtn_tbl_query(svc_hdl,
					     info->info.svc_subscribe
					     .i_svc_ids[i]);

		if (status == NCSCC_RC_SUCCESS) {
			MDS_SVC_LOG_ERROR(
				SUBSCRIBE_TAG, info,
				"Subscription to svc_id = %s(%d) failed |"
				" already exist",
				get_svc_names(info->info.svc_subscribe
				              .i_svc_ids[i]),
				info->info.svc_subscribe.i_svc_ids[i]);
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		}
	}

	for (i = 0; i < info->info.svc_subscribe.i_num_svcs; i++) {
		status = mds_subtn_tbl_query(svc_hdl,
			info->info.svc_subscribe.i_svc_ids[i]);
		if (status == NCSCC_RC_NO_CREATION) {
			/* Implicit subscription already present
			   so change Implicit flag to explicit */
			mds_subtn_tbl_change_explicit(svc_hdl,
				info->info.svc_subscribe.i_svc_ids[i],
				(info->i_op) == MDS_SUBSCRIBE ? MDS_VIEW_NORMAL
				                              : MDS_VIEW_RED);
			MDS_SVC_LOG_INFO(
				SUBSCRIBE_TAG, info,
				"Subscription to svc_id = %s(%d) :"
				" already exist Implicitly: changed"
				" to explicit",
				get_svc_names(info->info.svc_subscribe
				              .i_svc_ids[i]),
				info->info.svc_subscribe.i_svc_ids[i]);
		} else {
			/**
			 * status = NCSCC_RC_FAILURE (It can't be SUCCESS,
			 * as it is verified in Validation)
			 **/

			/* Add new Subscription entry */
			status = mds_mcm_subtn_add(svc_hdl,
				info->info.svc_subscribe.i_svc_ids[i],
				info->info.svc_subscribe.i_scope, view,
				MDS_SUBTN_EXPLICIT);
			if (status != NCSCC_RC_SUCCESS) {
				/* There was some problem while subscribing so
				 * rollback all previous subscriptions */
				MDS_SVC_LOG_ERROR(SUBSCRIBE_TAG, info,
					"Subscription to svc_id = %s(%d)"
					" failed",
					get_svc_names(info->info.svc_subscribe
					              .i_svc_ids[i]),
					info->info.svc_subscribe.i_svc_ids[i]);
				MDS_SVC_LOG_INFO(SUBSCRIBE_TAG, info,
					"Rollbacking previous subscriptions");

				/* Unsubscribing the subscribed services */
				memset(&unsubscribe_info, 0,
				       sizeof(unsubscribe_info));
				unsubscribe_info.i_mds_hdl = info->i_mds_hdl;
				unsubscribe_info.i_svc_id = info->i_svc_id;
				unsubscribe_info.i_op = MDS_CANCEL;
				unsubscribe_info.info.svc_cancel.i_num_svcs =
					i;
				unsubscribe_info.info.svc_cancel.i_svc_ids =
					info->info.svc_subscribe.i_svc_ids;

				mds_mcm_svc_unsubscribe(&unsubscribe_info);
				m_MDS_LEAVE();
				return NCSCC_RC_FAILURE;
			}
			/**
			 * MDTM subscribe is done in above function
			 * Tipc subtn_ref_hdl returned stored in subtn_info in
			 * above function
			 **/
		}
		MDS_SVC_LOG_INFO(SUBSCRIBE_TAG, info,
				"Subscribe to svc_id = %s(%d) successful",
				get_svc_names(info->info.svc_subscribe
					      .i_svc_ids[i]),
				info->info.svc_subscribe.i_svc_ids[i]);
	}
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_unsubscribe(const NCSMDS_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_SVC_HDL svc_hdl;
	uint32_t i;
	MDS_SUBTN_REF_VAL subscr_req_hdl;

	m_MDS_ENTER();
	status = mds_svc_tbl_query((MDS_PWE_HDL)info->i_mds_hdl,
				   info->i_svc_id);
	if (status == NCSCC_RC_FAILURE) {
		/* Service doesn't exist */
		MDS_SVC_LOG_ERROR(UNSUBSCRIBE_TAG, info,
				  "Service doesn't exist");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
	/* STEP 1: Fetch "svc-hdl" from SVC-TABLE.
	   INPUT
	   - SUBTN_REF_VAL
	   OUTPUT
	   -   SVC_HDL    */
	mds_svc_tbl_get_svc_hdl((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id,
				&svc_hdl);

	/* STEP 2: Find all subscription entries from
	   LOCAL-SUBSCRIPTION-REQ-TABLE using "svc-hdl" INPUT -  SVC_HDL OUTPUT
	   - List of "subtn_hdl" */

	/* Subscription Validation */
	for (i = 0; i < info->info.svc_cancel.i_num_svcs; i++) {
		status = mds_subtn_tbl_query(svc_hdl,
				info->info.svc_cancel.i_svc_ids[i]);
		if (status == NCSCC_RC_FAILURE) {
			MDS_SVC_LOG_ERROR(UNSUBSCRIBE_TAG, info,
				"Unsubscription to svc_id = %s(%d) failed :"
				" not subscribed",
				get_svc_names(info->info.svc_cancel
					      .i_svc_ids[i]),
				info->info.svc_cancel.i_svc_ids[i]);
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		}
	}

	for (i = 0; i < info->info.svc_cancel.i_num_svcs; i++) {
		NCSMDS_SCOPE_TYPE scope = NCSMDS_SCOPE_INTRAPCON;

		status = mds_subtn_tbl_get_ref_hdl(svc_hdl,
				info->info.svc_cancel.i_svc_ids[i],
				&subscr_req_hdl, &scope);
		if (status == NCSCC_RC_FAILURE) {
			/* Not able to get subtn ref hdl */
		} else {
			/* STEP 3: Call mcm_mdtm_unsubscribe(subtn_hdl) */
			/* Inform MDTM about unsubscription */
			mds_mdtm_svc_unsubscribe(
				m_MDS_GET_PWE_ID_FROM_PWE_HDL(
					(MDS_PWE_HDL)(info->i_mds_hdl)),
				info->info.svc_cancel.i_svc_ids[i], scope,
				subscr_req_hdl);
		}

		/* Find and delete related adest from adest list */
		MDS_SUBSCRIPTION_RESULTS_INFO *s_info = NULL;
		mds_subtn_res_tbl_getnext_any(svc_hdl,
			info->info.svc_cancel.i_svc_ids[i],
			&s_info);
		if (s_info) {
			MDS_ADEST_INFO *adest_info = (MDS_ADEST_INFO *)
				ncs_patricia_tree_get(
					&gl_mds_mcm_cb->adest_list,
					(uint8_t *)&s_info->key.adest);
			if (adest_info) {
				mds_adest_op_down_timer_stop(adest_info);
				ncs_patricia_tree_del(
					&gl_mds_mcm_cb->adest_list,
					(NCS_PATRICIA_NODE *) adest_info);
					m_MMGR_FREE_ADEST_INFO(adest_info);
			}
		}

		/* Delete all MDTM entries */
		mds_subtn_res_tbl_del_all(svc_hdl, info->info.svc_cancel
					  .i_svc_ids[i]);
		mds_subtn_tbl_del(svc_hdl, info->info.svc_cancel.i_svc_ids[i]);
		MDS_SVC_LOG_INFO(UNSUBSCRIBE_TAG, info,
			"Unsubscribe to svc_id = %s(%d) successful",
			get_svc_names(info->info.svc_subscribe.i_svc_ids[i]),
			info->info.svc_subscribe.i_svc_ids[i]);
	}
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_up(const MCM_SVC_UP_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	uint32_t ret = NCSCC_RC_SUCCESS;
	NCSMDS_SCOPE_TYPE local_subtn_scope;
	MDS_VIEW local_subtn_view;
	NCSMDS_CALLBACK_INFO cbinfo;
	MDS_SUBSCRIPTION_RESULTS_INFO *log_subtn_result_info = NULL;
	m_MDS_ENTER();
	mds_svc_op_chg_dump(info);

	status = mds_svc_tbl_query(
			m_MDS_GET_PWE_HDL_FROM_SVC_HDL(info->local_svc_hdl),
			m_MDS_GET_SVC_ID_FROM_SVC_HDL(info->local_svc_hdl));
	if (status == NCSCC_RC_FAILURE) {
		MDS_SVC_CHG_LOG_ERROR(UP_TAG, info,
				      "Local service doesn't exist");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/* clear cbinfo contents */
	memset(&cbinfo, 0, sizeof(cbinfo));

	/*************** Validation for SCOPE **********************/

	/* check the installation scope of service is in subscription scope */
	mds_subtn_tbl_get_details(info->local_svc_hdl, info->svc_id,
				  &local_subtn_scope, &local_subtn_view);

	/* There is no check over here for above ,this may cause crash TO DO */
	status = mds_mcm_validate_scope(local_subtn_scope, info->scope,
					info->adest, info->svc_id,
					info->my_pcon);
	if (status == NCSCC_RC_FAILURE) {
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	}

	/*************** Validation for SCOPE **********************/

	if (info->adest != m_MDS_GET_ADEST) {
		MDS_ADEST_INFO *adest_info = (MDS_ADEST_INFO *)
			ncs_patricia_tree_get(&gl_mds_mcm_cb->adest_list,
					      (uint8_t *)&info->adest);
		if (!adest_info) {
			/* Add adest to adest list */
			adest_info = m_MMGR_ALLOC_ADEST_INFO;
			memset(adest_info, 0, sizeof(MDS_ADEST_INFO));
			adest_info->adest = info->adest;
			adest_info->node.key_info =
				(uint8_t *)&adest_info->adest;
			adest_info->svc_cnt = 1;
			adest_info->is_up = true;
			ncs_patricia_tree_add(&gl_mds_mcm_cb->adest_list,
					      (NCS_PATRICIA_NODE *)adest_info);
		} else {
			if (adest_info->svc_cnt == 0) {
				mds_adest_op_down_timer_stop(adest_info);
				adest_info->is_up = true;
			}
			adest_info->svc_cnt++;
		}
	}

	status = mds_get_subtn_res_tbl_by_adest(info->local_svc_hdl,
						info->svc_id,
						info->vdest,
						info->adest,
						&log_subtn_result_info);
	if (status == NCSCC_RC_FAILURE) {
		/* Subscription result table entry doesn't exist */
		if (info->vdest == m_VDEST_ID_FOR_ADEST_ENTRY) {
			/* Remote svc is on ADEST */
			ret = mds_svc_op_adest_nway_add(info);
		} else if (info->vdest_policy == NCS_VDEST_TYPE_MxN) {
			if (info->role == V_DEST_RL_ACTIVE) {
				ret = mds_svc_op_vdest_mxn_active_add(info);
			} else {
				ret = mds_svc_op_vdest_mxn_standby_add(info);
			}
		} else {
			/* vdest_policy == NCS_VDEST_TYPE_N_WAY_ROUND_ROBIN */
			if (info->role == V_DEST_RL_ACTIVE) {
				ret = mds_svc_op_vdest_nway_active_add(info);
			} else {
				ret = mds_svc_op_vdest_nway_standby_add(info);
			}
		} /* End : Remote service is on VDEST */
	} else {
		/* Entry exist in subscription result table */
		if (info->vdest == m_VDEST_ID_FOR_ADEST_ENTRY) {
			/* Discard as it is already UP */
			ret = NCSCC_RC_SUCCESS;
		} else if (info->vdest_policy == NCS_VDEST_TYPE_MxN) {
			if (info->role == V_DEST_RL_ACTIVE) {
				ret = mds_svc_op_vdest_mxn_active_update(info);
			} else {
				ret = mds_svc_op_vdest_mxn_standby_update(info);
			}
		} else { /* vdest_policy == NCS_VDEST_TYPE_N_WAY_ROUND_ROBIN */
			if (info->role == V_DEST_RL_ACTIVE) {
				ret = mds_svc_op_vdest_nway_active_update(
					info);
			} else {
				ret = mds_svc_op_vdest_nway_standby_update(
					info);
			}
		}
	}
	m_MDS_LEAVE();
	return ret;
}

uint32_t mds_svc_op_down(const MCM_SVC_DOWN_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	uint32_t ret = NCSCC_RC_SUCCESS;
	NCSMDS_SCOPE_TYPE local_subtn_scope;
	MDS_VIEW local_subtn_view;
	MDS_SUBSCRIPTION_RESULTS_INFO *log_subtn_result_info = NULL;
	m_MDS_ENTER();
	mds_svc_op_chg_dump(info);

	/* Potentially clean up the process info database */
	MDS_PROCESS_INFO *process_info = mds_process_info_get(info->adest,
					(NCSMDS_SVC_ID) (info->svc_id));
	if (process_info != NULL) {
		/* Process info exists, delay the cleanup with a timeout
		 * to avoid race */
		mds_adest_op_down_timer_start(info->adest, info->svc_id);
	}

	status = mds_svc_tbl_query(
			m_MDS_GET_PWE_HDL_FROM_SVC_HDL(info->local_svc_hdl),
			m_MDS_GET_SVC_ID_FROM_SVC_HDL(info->local_svc_hdl));
	if (status == NCSCC_RC_FAILURE) {
		/* Local Service doesn't exist */
		MDS_SVC_CHG_LOG_ERROR(DOWN_TAG, info,
				      "Local SVC doesn't exist");
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/*************** Validation for SCOPE **********************/

	/* check the installation scope of service is in subscription scope */
	mds_subtn_tbl_get_details(info->local_svc_hdl, info->svc_id,
				  &local_subtn_scope, &local_subtn_view);
	status = mds_mcm_validate_scope(local_subtn_scope, info->scope,
					info->adest, info->svc_id,
					info->my_pcon);
	if (status == NCSCC_RC_FAILURE) {
		return NCSCC_RC_SUCCESS;
	}
	/*************** Validation for SCOPE **********************/

	status = mds_get_subtn_res_tbl_by_adest(info->local_svc_hdl,
						info->svc_id,
						(MDS_VDEST_ID)info->vdest,
						info->adest,
						&log_subtn_result_info);
	if (status == NCSCC_RC_FAILURE) {
		/* Subscription result table entry doesn't exist */

		/* Discard : Getting down before getting up */
	} else {
		/* Entry exist in subscription result table */

		MDS_ADEST_INFO *adest_info = (MDS_ADEST_INFO *)
					ncs_patricia_tree_get(
						&gl_mds_mcm_cb->adest_list,
						(uint8_t *)&info->adest);
		if (adest_info && adest_info->svc_cnt > 0) {
			adest_info->svc_cnt--;
			if (adest_info->svc_cnt == 0) {
				MDS_SVC_CHG_LOG_INFO(info, "Down timer start");
				mds_adest_op_down_timer_start(info->adest, 0);
			}
		}

		if (info->vdest == m_VDEST_ID_FOR_ADEST_ENTRY) {
			ret = mds_svc_op_adest_nway_delete(info);
		} else {
			V_DEST_RL dest_role;
			MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info =
									NULL;

			mds_subtn_res_tbl_get_by_adest(info->local_svc_hdl,
						       info->svc_id,
						       info->vdest,
						       info->adest, &dest_role,
						       &subtn_result_info);
			if (dest_role != info->role) {
				/* Role Mismatch */

				/**
				 * Already role changed by Other role publish
				 * If svc is really going down it will get down
				 * for Other role also so Discard
				 **/
				MDS_SVC_CHG_LOG_FULL_INFO(NCSMDS_DOWN, info,
							  "ROLE MISMATCH");
				/**
				 * Return success as we have to discard this
				 * event
				 **/
				m_MDS_LEAVE();
				return NCSCC_RC_SUCCESS;
			}
			/* Remote svc is on VDEST */
			if (info->vdest_policy == NCS_VDEST_TYPE_MxN &&
					info->role == V_DEST_RL_ACTIVE) {
				ret = mds_svc_op_vdest_mxn_active_delete(info);
			} else if (info->vdest_policy == NCS_VDEST_TYPE_MxN &&
					info->role == V_DEST_RL_ACTIVE) {
				ret = mds_svc_op_vdest_mxn_standby_delete(
					info);
			} else if (info->role == V_DEST_RL_ACTIVE) {
				ret = mds_svc_op_vdest_nway_active_delete(
					info);
			} else {
				ret = mds_svc_op_vdest_nway_standby_delete(
					info);
			}
		}
	}
	m_MDS_LEAVE();
	return ret;
}

uint32_t mds_svc_op_adest_nway_add(const MCM_SVC_INFO *info) {
	mds_svc_op_subtn_res_tbl_add(info);
	/* Call user call back with UP */
	return mds_svc_op_user_callback(UP_TAG, NCSMDS_UP, info);
}

uint32_t mds_svc_op_adest_nway_delete(const MCM_SVC_INFO *info) {
	mds_svc_op_subtn_res_tbl_del(info);
	return mds_svc_op_user_callback(DOWN_TAG, NCSMDS_DOWN, info);
}

uint32_t mds_svc_op_vdest_mxn_active_add(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	status = mds_subtn_res_tbl_get(info->local_svc_hdl, info->svc_id,
				       (MDS_VDEST_ID) info->vdest,
				       &active_adest, &tmr_running,
				       &subtn_result_info, true);
	/* check if any other active present */
	if (status == NCSCC_RC_FAILURE) {
		/* No active present */

		/* Add entry to subscription result table */
		mds_svc_op_subtn_res_tbl_add(info);

		/* Call user call back with UP */
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_UP, info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}

		/* If subscripton is RED, call RED_UP also */
		if (local_subtn_view == MDS_VIEW_RED) {
			status = mds_svc_op_user_callback(UP_TAG,
					NCSMDS_RED_UP, info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
	} else {
		/* Active/Await-active already present */

		mds_svc_op_subtn_res_tbl_add(info);
		/* Here Current active if any will get replaced */

		/* If it was Awaiting active give NEW ACTIVE to user */
		if (tmr_running == true) {
			/* Call user callback UP */
			status = mds_svc_op_user_callback(UP_TAG,
					NCSMDS_NEW_ACTIVE, info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}

		/* If subscription is RED, call RED_UP also */
		if (local_subtn_view == MDS_VIEW_RED) {
			status = mds_svc_op_user_callback(UP_TAG,
					NCSMDS_RED_UP, info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_mxn_standby_add(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	mds_svc_op_subtn_res_tbl_add(info);

	/* check the type of subscription normal/redundant */
	if (local_subtn_view == MDS_VIEW_NORMAL) {
		/* Don't send UP for Standby */
	} else {
		/* local_subtn_view = MDS_VIEW_RED */

		/* Call user callback with UP */
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_RED_UP, info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}

	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_mxn_active_update(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_SUBSCRIPTION_RESULTS_INFO *active_subtn_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);
	V_DEST_RL dest_role;

	status = mds_subtn_res_tbl_get(info->local_svc_hdl, info->svc_id,
				       (MDS_VDEST_ID)info->vdest,
				       &active_adest, &tmr_running,
				       &active_subtn_result_info, true);
	if (status == NCSCC_RC_FAILURE) {
		/* No other active entry exist */

		/* Get pointer to this adest */
		mds_subtn_res_tbl_get_by_adest(info->local_svc_hdl,
					       info->svc_id,
					       info->vdest, info->adest,
					       &dest_role,
					       &subtn_result_info);
		/* Add Active subtn result table entry */
		if (dest_role == V_DEST_RL_STANDBY) {
			/* Change role to Active */
			mds_svc_op_subtn_res_tbl_change_role(info);
			/* If subscription is RED, call CHG_ROLE */
			if (local_subtn_view == MDS_VIEW_RED) {
				status = mds_svc_op_user_callback(UP_TAG,
						NCSMDS_CHG_ROLE, info);
				if (status != NCSCC_RC_SUCCESS) {
					return NCSCC_RC_FAILURE;
				}
			}
		}

		mds_svc_op_subtn_res_tbl_add_active(info, subtn_result_info);
		/* Call user callback UP */
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_UP, info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	} else {
		/* Active or Await Active entry exist */

		if (active_adest == info->adest) {
			/* Current Active entry is entry for which we got up */

			/* Discard as it is duplicate */
		} else {
			/* Some other Active or Await entry exist */

			/* Some other entry is active and this one standby */
			/* so make this one active */

			/* Get pointer to this adest */
			mds_subtn_res_tbl_get_by_adest(info->local_svc_hdl,
						       info->svc_id,
						       info->vdest,
						       info->adest,
						       &dest_role,
						       &subtn_result_info);
			if (dest_role == V_DEST_RL_STANDBY) {
				/* Change role to Active */
				mds_svc_op_subtn_res_tbl_change_role(info);

				/* If subscription is RED, call CHG_ROLE */
				if (local_subtn_view == MDS_VIEW_RED) {
					status = mds_svc_op_user_callback(
							UP_TAG,
							NCSMDS_CHG_ROLE,
							info);
					if (status != NCSCC_RC_SUCCESS) {
						return NCSCC_RC_FAILURE;
					}
				}
			}

			if (tmr_running == true) {
				/* Make this as active */
				mds_svc_op_subtn_res_tbl_change_active(info,
						subtn_result_info);
				/* Call user callback UP */
				status = mds_svc_op_user_callback(UP_TAG,
						NCSMDS_NEW_ACTIVE, info);
				if (status != NCSCC_RC_SUCCESS) {
					return NCSCC_RC_FAILURE;
				}
			} else {
				/* Conflict active entry, keep old active
				   entry. */
			}
		}
	}

	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_mxn_standby_update(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_SUBSCRIPTION_RESULTS_INFO *next_active_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);
	V_DEST_RL dest_role;

	/**
	 * Check whether then existing entry is active and we got UP
	 * for standby for same entry
	 **/
	mds_subtn_res_tbl_get(info->local_svc_hdl, info->svc_id,
			      (MDS_VDEST_ID)info->vdest,
			      &active_adest, &tmr_running,
			      &subtn_result_info, true);
	if (active_adest == info->adest) {
		/* This entry is going down */
		MCM_SVC_INFO callback_info = *info;
		callback_info.adest = 0;
		callback_info.arch_word = MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;

		mds_svc_op_subtn_res_tbl_remove_active(info);
		mds_svc_op_subtn_res_tbl_change_role(info);

		/* Call user callback NO ACTIVE */
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_NO_ACTIVE,
						  &callback_info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}

		/* If subscription is RED, call CHG_ROLE also */
		if (local_subtn_view == MDS_VIEW_RED) {
			status = mds_svc_op_user_callback(UP_TAG,
					NCSMDS_CHG_ROLE, info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}

		status = mds_subtn_res_tbl_query_next_active(
				info->local_svc_hdl, info->svc_id, info->vdest,
				subtn_result_info, &next_active_result_info);
		if (status == NCSCC_RC_SUCCESS) {
			/* Other active present. Change active entry */
			mds_svc_op_subtn_res_tbl_change_active(info,
				next_active_result_info);
			status = mds_svc_op_user_callback(UP_TAG,
				NCSMDS_NEW_ACTIVE, info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
	} else {
		/* Some other entry is active */

		/**
		 * It can be either duplicate in this case we will discard
		 * Or some other entry is forcefully made active due to remote
		 * VDEST role change in this case we will just change role
		 **/
		mds_subtn_res_tbl_get_by_adest(info->local_svc_hdl,
					       info->svc_id,
					       info->vdest, info->adest,
					       &dest_role,
					       &subtn_result_info);
		if (dest_role == V_DEST_RL_ACTIVE) {
			/* There is change in role of this service */
			mds_svc_op_subtn_res_tbl_change_role(info);

			/* If subscription is RED, call CHG_ROLE also */
			if (local_subtn_view == MDS_VIEW_RED) {
				status = mds_svc_op_user_callback(UP_TAG,
						NCSMDS_CHG_ROLE, info);
				if (status != NCSCC_RC_SUCCESS) {
					return NCSCC_RC_FAILURE;
				}
			}
		}
	}

	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_mxn_active_delete(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_SUBSCRIPTION_RESULTS_INFO *next_active_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	/* Get info about subtn entry before deleting */
	mds_subtn_res_tbl_get(info->local_svc_hdl, info->svc_id, info->vdest,
			      &active_adest, &tmr_running, &subtn_result_info,
			      true);
	/* First delete the entry */
	mds_svc_op_subtn_res_tbl_del(info);

	MDS_SUBSCRIPTION_RESULTS_INFO *s_info = NULL;
	bool adest_exists = false;
	MCM_SVC_INFO callback_info = *info;
	callback_info.adest = 0;

	/* If no adest remains for this svc send MDS_DOWN */
	status = mds_subtn_res_tbl_getnext_any(info->local_svc_hdl,
					       info->svc_id, &s_info);
	while (status != NCSCC_RC_FAILURE) {
		if (s_info->key.vdest_id != m_VDEST_ID_FOR_ADEST_ENTRY) {
			adest_exists = true;
			break;
		}
		status = mds_subtn_res_tbl_getnext_any(info->local_svc_hdl,
						       info->svc_id, &s_info);
	}

	if (active_adest != info->adest && adest_exists == false) {
		callback_info.arch_word = info->arch_word;
		status = mds_svc_op_user_callback(DOWN_TAG, NCSMDS_DOWN,
						  &callback_info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}

	if (active_adest == info->adest) {
		callback_info.arch_word = MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;
		status = mds_subtn_res_tbl_query_next_active(
				info->local_svc_hdl, info->svc_id, info->vdest,
				NULL, &next_active_result_info);
		if (status == NCSCC_RC_FAILURE) {
			/* No other active present */
			mds_svc_op_subtn_res_tbl_remove_active(info);
			/* Call user call back with NO ACTIVE */
			status = mds_svc_op_user_callback(DOWN_TAG,
							  NCSMDS_NO_ACTIVE,
							  &callback_info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		} else {
			status = mds_svc_op_user_callback(DOWN_TAG,
							  NCSMDS_NO_ACTIVE,
							  &callback_info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
			/* Change Active entry */
			mds_svc_op_subtn_res_tbl_change_active(info,
				next_active_result_info);

			MCM_SVC_INFO act_info = *info;
			act_info.adest = next_active_result_info->key.adest;
			status = mds_svc_op_user_callback(DOWN_TAG,
							  NCSMDS_NEW_ACTIVE,
							  &act_info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
		if (adest_exists == false) {
			/**
			 * No other adest exists for this svc_id,
			 * call user callback DOWN
			 **/
			callback_info.arch_word = info->arch_word;
			status = mds_svc_op_user_callback(DOWN_TAG,
							  NCSMDS_DOWN,
							  &callback_info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
	}
	/* If subscription is RED, call RED_DOWN also */
	if (local_subtn_view == MDS_VIEW_RED) {
		callback_info.adest = info->adest;
		callback_info.arch_word = MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;
		status = mds_svc_op_user_callback(DOWN_TAG, NCSMDS_RED_DOWN,
						  info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_mxn_standby_delete(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);
	MCM_SVC_DOWN_INFO callback_info = *info;

	/* Delete backup entry */
	mds_svc_op_subtn_res_tbl_del(info);
	if (local_subtn_view == MDS_VIEW_NORMAL) {
		/* Don't send DOWN for Standby */
	} else {
		/* local_subtn_view = MDS_VIEW_RED */

		/* Call user callback with DOWN */
		callback_info.arch_word = MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;
		status = mds_svc_op_user_callback(DOWN_TAG, NCSMDS_RED_DOWN,
						  &callback_info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	{
		MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
		bool adest_exists = false;

		/* if no adest remains for this svc, send MDS_DOWN */
		status = mds_subtn_res_tbl_getnext_any(info->local_svc_hdl,
				info->svc_id, &subtn_result_info);
		while (status != NCSCC_RC_FAILURE) {
			if (subtn_result_info->key.vdest_id
					!= m_VDEST_ID_FOR_ADEST_ENTRY) {
				adest_exists = true;
				break;
			}

			status = mds_subtn_res_tbl_getnext_any(
					info->local_svc_hdl, info->svc_id,
					&subtn_result_info);
		}

		if (adest_exists == false) {
			/**
			 * No other adest exists for this svc_id,
			 * Call user callback DOWN
			 **/
			callback_info.adest = 0;
			callback_info.arch_word =
				MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;
			status = mds_svc_op_user_callback(DOWN_TAG,
					NCSMDS_DOWN, &callback_info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
	}

	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_nway_active_add(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	/**
	 * To get whether current entry exist. It will just store
	 * tmr_running and status
	 **/
	status = mds_subtn_res_tbl_get(info->local_svc_hdl, info->svc_id,
			(MDS_VDEST_ID) info->vdest, &active_adest,
			&tmr_running, &subtn_result_info, true);

	/* Add entry first */
	mds_svc_op_subtn_res_tbl_add(info);

	/* check if any other active present */
	if (status == NCSCC_RC_FAILURE) {
		/* No active present or Await Active Entry present */

		/* Call user callback UP */
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_UP, info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	} else if (tmr_running == true) {
		/* Await active entry is present */

		/* Call user callback UP */
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_NEW_ACTIVE,
						  info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	/* If subscripton is RED, call RED_UP also */
	if (local_subtn_view == MDS_VIEW_RED) {
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_RED_UP, info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_nway_standby_add(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);
	mds_svc_op_subtn_res_tbl_add(info);

	/* check the type of subscription normal/redundant */
	if (local_subtn_view == MDS_VIEW_NORMAL) {
		/* Dont send UP for Standby */
	} else {
		/* local_subtn_view == MDS_VIEW_RED */

		/* Call user callback with UP for Standby */
		status = mds_svc_op_user_callback(UP_TAG, NCSMDS_RED_UP, info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_nway_active_update(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	V_DEST_RL dest_role;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_SUBSCRIPTION_RESULTS_INFO *active_subtn_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	mds_subtn_res_tbl_get_by_adest(info->local_svc_hdl, info->svc_id,
				       (MDS_VDEST_ID)info->vdest, info->adest,
				       &dest_role, &subtn_result_info);
	if (dest_role == V_DEST_RL_ACTIVE) {
		/* Discard as it is duplicate */
	} else {
		/* dest_role == V_DEST_RL_STANDBY */

		/* Make it ACTIVE */
		mds_svc_op_subtn_res_tbl_change_role(info);

		/* If subscription is RED, call CHG_ROLE also */
		if (local_subtn_view == MDS_VIEW_RED) {
			status = mds_svc_op_user_callback(UP_TAG,
					NCSMDS_CHG_ROLE, info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}

		/* check if any Active entry exists */
		status = mds_subtn_res_tbl_get(info->local_svc_hdl,
				info->svc_id, info->vdest, &active_adest,
				&tmr_running, &active_subtn_result_info, true);

		if (status == NCSCC_RC_SUCCESS) {
			/* Active or Await active entry exists */

			if (tmr_running == true) {
				/* If Await active exist then point to this one
				   as active */
				mds_svc_op_subtn_res_tbl_change_active(info,
					subtn_result_info);
				/* Call user callback UP */
				status = mds_svc_op_user_callback(UP_TAG,
						NCSMDS_NEW_ACTIVE, info);
				if (status != NCSCC_RC_SUCCESS) {
					return NCSCC_RC_FAILURE;
				}
			} else {
				/**
				 * Some other active entry exist so don't
				 * disturb it. This entry can't be active
				 * as earlier it was Standby
				 **/
			}
		} else {
			/* Add Active entry */
			mds_svc_op_subtn_res_tbl_add_active(info,
				subtn_result_info);

			/* Call user callback UP */
			status = mds_svc_op_user_callback(UP_TAG, NCSMDS_UP,
							  info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_nway_standby_update(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	V_DEST_RL dest_role;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_SUBSCRIPTION_RESULTS_INFO *active_subtn_result_info = NULL;
	MDS_SUBSCRIPTION_RESULTS_INFO *next_active_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	mds_subtn_res_tbl_get_by_adest(info->local_svc_hdl, info->svc_id,
				       (MDS_VDEST_ID)info->vdest, info->adest,
				       &dest_role, &subtn_result_info);
	if (dest_role == V_DEST_RL_STANDBY) {
		/* Discard as it is duplicate */
	} else {
		/**
		 * dest_role == V_DEST_RL_ACTIVE
		 * This active entry is going to standby
		 **/

		/**
		 * Check if Active entry was referring to
		 * the same entry going standby
		 **/
		status = mds_subtn_res_tbl_get(info->local_svc_hdl,
				info->svc_id, info->vdest, &active_adest,
				&tmr_running, &active_subtn_result_info,
				true);
		if (status == NCSCC_RC_SUCCESS) {
			/* Active or Await active entry exists */
			if (tmr_running == true) {
				/* Timer is running so active entry points to
				   NULL */

				/* Do nothing */
			} else if (active_subtn_result_info ==
					subtn_result_info) {
				/* Active entry exist and is currently
				   active */

				/* Point to next active if present */
				mds_subtn_res_tbl_query_next_active(
					info->local_svc_hdl, info->svc_id,
					info->vdest, subtn_result_info,
					&next_active_result_info);
				if (subtn_result_info ==
						next_active_result_info) {
					MCM_SVC_INFO callback_info = *info;
					callback_info.adest = 0;
					callback_info.arch_word =
					    MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;
					/* No other active present */
					mds_svc_op_subtn_res_tbl_remove_active(
						info);
					/* No active timer will be started in
					   above function */

					/* Call user callback NO Active */
					status = mds_svc_op_user_callback(
							UP_TAG,
							NCSMDS_NO_ACTIVE,
							&callback_info);
					if (status != NCSCC_RC_SUCCESS) {
						return NCSCC_RC_FAILURE;
					}
				} else {
					/* Change to Active to point to next
					   active */
					mds_svc_op_subtn_res_tbl_change_active(
						info, next_active_result_info);
				}
			} else {
				/* Active entry exist and isn't currently
				   active */

				/* We don't care, and don't disturb other
				   actives */
			}
		} else {
			/* just change role, which is done in step below */
		}

		/* Make this as STANDBY */
		mds_svc_op_subtn_res_tbl_change_role(info);

		/* If subscription is RED, call CHG_ROLE also */
		if (local_subtn_view == MDS_VIEW_RED) {
			status = mds_svc_op_user_callback(UP_TAG,
					NCSMDS_CHG_ROLE, info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_nway_active_delete(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_DEST active_adest = 0;
	bool tmr_running;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	MDS_SUBSCRIPTION_RESULTS_INFO *next_active_result_info = NULL;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	/* Get info about subtn entry before deleting */
	mds_subtn_res_tbl_get(info->local_svc_hdl, info->svc_id, info->vdest,
			      &active_adest, &tmr_running, &subtn_result_info,
			      true);
	/* First delete the entry */
	mds_svc_op_subtn_res_tbl_del(info);

	if (active_adest == info->adest) {
		status = mds_subtn_res_tbl_query_next_active(
					info->local_svc_hdl, info->svc_id,
					info->vdest, NULL,
					&next_active_result_info);
		if (status == NCSCC_RC_FAILURE) {
			MCM_SVC_INFO callback_info = *info;
			callback_info.adest = 0;
			callback_info.arch_word =
				MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;
			/* No other active present */
			mds_svc_op_subtn_res_tbl_remove_active(info);
			/* No active timer will be started in above function */

			/* Call user callback NO Active */
			status = mds_svc_op_user_callback(DOWN_TAG,
					NCSMDS_NO_ACTIVE, &callback_info);
			if (status != NCSCC_RC_SUCCESS) {
				return NCSCC_RC_FAILURE;
			}
		} else {
			/* Change to Active to point to next active */
			mds_svc_op_subtn_res_tbl_change_active(info,
				next_active_result_info);
		}
	}

	/* If subscripton is RED, call RED_DOWN also */
	if (local_subtn_view == MDS_VIEW_RED) {
		status = mds_svc_op_user_callback(DOWN_TAG, NCSMDS_RED_DOWN,
						  info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_vdest_nway_standby_delete(const MCM_SVC_INFO *info) {
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;
	bool adest_exists = false;
	MDS_VIEW local_subtn_view = mds_svc_op_get_view(info);

	/* Delete backup entry */
	mds_svc_op_subtn_res_tbl_del(info);
	if (local_subtn_view == MDS_VIEW_NORMAL) {
		/* Don't send DOWN for Standby */
	} else {
		/* local_subtn_view = MDS_VIEW_RED */

		/* Call user callback with DOWN */
		status = mds_svc_op_user_callback(DOWN_TAG, NCSMDS_RED_DOWN,
						  info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	/* if no adest remains for this svc,
		* send MDS_DOWN */
	status = mds_subtn_res_tbl_getnext_any(info->local_svc_hdl,
					       info->svc_id,
					       &subtn_result_info);
	while (status != NCSCC_RC_FAILURE) {
		if (subtn_result_info->key.vdest_id
				!= m_VDEST_ID_FOR_ADEST_ENTRY) {
			adest_exists = true;
			break;
		}
		status = mds_subtn_res_tbl_getnext_any(info->local_svc_hdl,
				info->svc_id, &subtn_result_info);
	}

	if (adest_exists == false) {
		/**
		 * No other adest exists for this svc_id,
		 * Call user callback DOWN
		 **/
		MCM_SVC_INFO callback_info = *info;
		callback_info.adest = 0;
		callback_info.arch_word = MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED;

		status = mds_svc_op_user_callback(DOWN_TAG, NCSMDS_DOWN,
				&callback_info);
		if (status != NCSCC_RC_SUCCESS) {
			return NCSCC_RC_FAILURE;
		}
	}
	return NCSCC_RC_SUCCESS;
}

uint32_t mds_svc_op_subtn_res_tbl_add(const MCM_SVC_INFO *info) {
	return mds_subtn_res_tbl_add(info->local_svc_hdl, info->svc_id,
		(MDS_VDEST_ID)info->vdest, info->adest, info->role,
		info->scope, info->vdest_policy, info->sub_part_ver,
		info->arch_word);
}

uint32_t mds_svc_op_subtn_res_tbl_del(const MCM_SVC_INFO *info) {
	return mds_subtn_res_tbl_del(info->local_svc_hdl, info->svc_id,
		info->vdest, info->adest, info->vdest_policy,
		info->sub_part_ver, info->arch_word);
}

uint32_t mds_svc_op_subtn_res_tbl_change_role(const MCM_SVC_INFO *info) {
	return mds_subtn_res_tbl_change_role(info->local_svc_hdl, info->svc_id,
		info->vdest, info->adest, info->role);
}

uint32_t mds_svc_op_subtn_res_tbl_add_active(const MCM_SVC_INFO *info,
			MDS_SUBSCRIPTION_RESULTS_INFO *subtn_res_info) {
	return mds_subtn_res_tbl_add_active(info->local_svc_hdl, info->svc_id,
		info->vdest, info->vdest_policy, subtn_res_info,
		info->sub_part_ver, info->arch_word);
}

uint32_t mds_svc_op_subtn_res_tbl_change_active(const MCM_SVC_INFO *info,
			MDS_SUBSCRIPTION_RESULTS_INFO *subtn_res_info) {
	return mds_subtn_res_tbl_change_active(info->local_svc_hdl,
		info->svc_id, (MDS_VDEST_ID) info->vdest, subtn_res_info,
		info->sub_part_ver, info->arch_word);
}

uint32_t mds_svc_op_subtn_res_tbl_remove_active(const MCM_SVC_INFO *info) {
	return mds_subtn_res_tbl_remove_active(info->local_svc_hdl,
		info->svc_id, (MDS_VDEST_ID)info->vdest);
}

uint32_t mds_svc_op_user_callback(const char* tag, NCSMDS_CHG change,
				  const MCM_SVC_INFO *info) {
	uint32_t status = mds_mcm_user_event_callback(info->local_svc_hdl,
				info->pwe_id, info->svc_id, info->role,
				info->vdest, info->adest, change,
				info->sub_part_ver, info->arch_word);
	if (status != NCSCC_RC_SUCCESS) {
		MDS_SVC_CHG_LOG_ERROR(tag, info, "%s Callback Failure",
				      mds_svc_op_change_get_name(change));
		return NCSCC_RC_FAILURE;
	}
	MDS_SVC_CHG_LOG_FULL_INFO(change, info, "");
	return NCSCC_RC_SUCCESS;
}

const char* mds_svc_op_change_get_name(NCSMDS_CHG change) {
	switch (change) {
		case NCSMDS_UP:
			return "UP";
		case NCSMDS_RED_UP:
			return "RED_UP";
		case NCSMDS_DOWN:
			return "DOWN";
		case NCSMDS_RED_DOWN:
			return "RED_DOWN";
		case NCSMDS_CHG_ROLE:
			return "CHG_ROLE";
		case NCSMDS_NEW_ACTIVE:
			return "NEW_ACTIVE";
			break;
		case NCSMDS_NO_ACTIVE:
			return "NO_ACTIVE";
		default:
			return "UNKNOWN";
	}
}

void mds_svc_op_chg_dump(const MCM_SVC_UP_INFO *info) {
	m_MDS_LOG_DBG(
		"MCM:API: LOCAL SVC INFO  : svc_id = %s(%d) | PWE id = %d"
		" | VDEST id = %d |",
		get_svc_names(
			m_MDS_GET_SVC_ID_FROM_SVC_HDL(info->local_svc_hdl)),
			m_MDS_GET_SVC_ID_FROM_SVC_HDL(info->local_svc_hdl),
			m_MDS_GET_PWE_ID_FROM_SVC_HDL(info->local_svc_hdl),
			m_MDS_GET_VDEST_ID_FROM_SVC_HDL(info->local_svc_hdl));
	m_MDS_LOG_DBG(
		"MCM:API: REMOTE SVC INFO : svc_id = %s(%d) | PWE id = %d"
		" | VDEST id = %d | POLICY = %d | SCOPE = %d | ROLE = %d"
		" | MY_PCON = %d |",
		get_svc_names(info->svc_id), info->svc_id, info->pwe_id,
		info->vdest, info->vdest_policy, info->scope, info->role,
		info->my_pcon);
}

MDS_VIEW mds_svc_op_get_view(const MCM_SVC_INFO *info) {
	NCSMDS_SCOPE_TYPE local_subtn_scope;
	MDS_VIEW local_subtn_view;
	mds_subtn_tbl_get_details(info->local_svc_hdl, info->svc_id,
				  &local_subtn_scope, &local_subtn_view);
	return local_subtn_view;
}
