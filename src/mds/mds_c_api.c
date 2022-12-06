/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
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

  DESCRIPTION:  MCM APIs

******************************************************************************
*/

#include "mds_core.h"
#include "mds_log.h"
#include "mds_svc_op.h"
#include "mds_adest_op.h"


/*********************************************************

  Function NAME: mds_validate_pwe_hdl

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_validate_pwe_hdl(MDS_PWE_HDL pwe_hdl)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	m_MDS_ENTER();

	status =
	    mds_pwe_tbl_query(m_MDS_GET_VDEST_HDL_FROM_VDEST_ID(
				  m_MDS_GET_VDEST_ID_FROM_PWE_HDL(pwe_hdl)),
			      m_MDS_GET_PWE_ID_FROM_PWE_HDL(pwe_hdl));
	m_MDS_LEAVE();
	return status;
}

/* ******************************************** */
/* ******************************************** */
/* ******************************************** */

/*              ncsmds_adm_api                  */

/* ******************************************** */
/* ******************************************** */
/* ******************************************** */

/*********************************************************

  Function NAME: mds_mcm_vdest_create

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_vdest_create(NCSMDS_ADMOP_INFO *info)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_SUBTN_REF_VAL subtn_ref_ptr;
	MDS_VDEST_ID vdest_id;
	m_MDS_ENTER();
	/* STEP 1: Create VDEST-Table Entry  */

	vdest_id = (MDS_VDEST_ID)info->info.vdest_create.i_vdest;

	if ((vdest_id > NCSMDS_MAX_VDEST) || (vdest_id == 0)) {
		m_MDS_LOG_ERR(
		    "MCM:API: Vdest_create : FAILED : VDEST id = %d not in prescribed range ",
		    vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/* Check whether VDEST already exist */
	status = mds_vdest_tbl_query(vdest_id);
	if (status == NCSCC_RC_SUCCESS) {
		/* VDEST already exist */
		m_MDS_LOG_ERR(
		    "MCM:API: vdest_create : VDEST id = %d Already exist",
		    vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	} else {
		/* Add vdest in to table */

		mds_vdest_tbl_add(
		    vdest_id, info->info.vdest_create.i_policy,
		    (MDS_VDEST_HDL *)&info->info.vdest_create.o_mds_vdest_hdl);
	}

	/* STEP 2: IF Policy is MxN *
		     call mdtm_vdest_subscribe (uint32_t i_vdest_id) */

	if (info->info.vdest_create.i_policy == NCS_VDEST_TYPE_MxN) {
		status = mds_mdtm_vdest_subscribe(vdest_id, &subtn_ref_ptr);
		if (status != NCSCC_RC_SUCCESS) {
			/* VDEST Subscription Failed */

			/* Remove VDEST info from MCM Database */
			mds_vdest_tbl_del(vdest_id);

			m_MDS_LOG_ERR(
			    "MCM:API: vdest_create : VDEST id = %d can't subscribe : MDTM Returned Failure ",
			    vdest_id);
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		}

		mds_vdest_tbl_update_ref_val(vdest_id, subtn_ref_ptr);
	}

	/* Fill output handle parameter */
	info->info.vdest_create.o_mds_vdest_hdl =
	    (MDS_HDL)m_MDS_GET_VDEST_HDL_FROM_VDEST_ID(vdest_id);

	m_MDS_LOG_INFO(
	    "MCM:API: vdest_create : VDEST id = %d Created Successfully",
	    vdest_id);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_vdest_destroy

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_vdest_destroy(NCSMDS_ADMOP_INFO *info)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_VDEST_ID local_vdest_id;
	MDS_PWE_HDL pwe_hdl;
	V_DEST_RL local_vdest_role;
	NCS_VDEST_TYPE local_vdest_policy;
	MDS_SUBTN_REF_VAL subtn_ref_val;
	NCSMDS_ADMOP_INFO pwe_destroy_info;
	m_MDS_ENTER();

	/* Check whether VDEST already exist */

	/* Get Vdest_id */
	local_vdest_id = m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(
	    info->info.vdest_destroy.i_vdest_hdl);

	status = mds_vdest_tbl_query(local_vdest_id);
	if (status == NCSCC_RC_FAILURE) {
		/* VDEST Doesn't exist */
		m_MDS_LOG_ERR(
		    "MCM:API: vdest_destroy : VDEST id = %d Doesn't exist",
		    local_vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/* Get Role, policy and subtn ref value for that vdest */
	mds_vdest_tbl_get_role(local_vdest_id, &local_vdest_role);
	mds_vdest_tbl_get_policy(local_vdest_id, &local_vdest_policy);
	mds_vdest_tbl_get_subtn_ref_val(local_vdest_id, &subtn_ref_val);

	/* STEP 1: For all PWEs on this DEST
		    Call mds_mcm_pwe_destroy (pwe_hdl) */

	pwe_destroy_info.i_op = MDS_ADMOP_PWE_DESTROY;

	while (mds_vdest_tbl_get_first(local_vdest_id, &pwe_hdl) !=
	       NCSCC_RC_FAILURE) {
		pwe_destroy_info.info.pwe_destroy.i_mds_pwe_hdl =
		    (MDS_HDL)pwe_hdl;
		mds_mcm_pwe_destroy(&pwe_destroy_info);
	}

	/* STEP 2: IF VDEST is Active
		    Call mdtm_vdest_uninstall (vdest_id)
		    To inform other VDEST about its going to Standby state. */

	/* STEP 3: Call mdtm_vdest_unsubscribe(vdest_id)
		    To Subscribe to active VDEST instance */

	if (local_vdest_policy == NCS_VDEST_TYPE_MxN) {
		/* Unsubscribe for this vdest */
		status =
		    mds_mdtm_vdest_unsubscribe(local_vdest_id, subtn_ref_val);
		if (status != NCSCC_RC_SUCCESS) {
			/* VDEST Unsubscription Failed */
			m_MDS_LOG_ERR(
			    "MCM:API:  VDEST id = %d can't Unsubscribe MDTM Returned Failure",
			    local_vdest_id);
		}

		/* If current role is active uninstall it */
		if (local_vdest_role == V_DEST_RL_ACTIVE) {
			status = mds_mdtm_vdest_uninstall(local_vdest_id);
			if (status != NCSCC_RC_SUCCESS) {
				/* VDEST Uninstalled Failed */
				m_MDS_LOG_ERR(
				    "MCM:API:  VDEST id = %d can't UnInstall  MDTM Returned Failure",
				    local_vdest_id);
			}
		}
	}

	/* STEP 4: Delete DEST from VDEST-table entry */

	status = mds_vdest_tbl_del(m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(
	    info->info.vdest_destroy.i_vdest_hdl));

	m_MDS_LOG_INFO(
	    "MCM:API: vdest_destroy : VDEST id = %d Destroyed Successfully",
	    local_vdest_id);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 *
 * Function Name: mds_mcm_vdest_query
 *
 * Purpose:       This function queries information about a local vdest.
 *
 * Return Value:  NCSCC_RC_SUCCESS
 *                NCSCC_RC_FAILURE
 *
 ****************************************************************************/

uint32_t mds_mcm_vdest_query(NCSMDS_ADMOP_INFO *info)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_VDEST_ID local_vdest_id;
	m_MDS_ENTER();

	local_vdest_id = (MDS_VDEST_ID)info->info.vdest_query.i_local_vdest;

	/* Check whether VDEST already exist */
	status = mds_vdest_tbl_query(local_vdest_id);
	if (status == NCSCC_RC_SUCCESS) {
		/* VDEST exists */

		/* Dummy statements as vdest_id is same as vdest_hdl */

		info->info.vdest_query.o_local_vdest_hdl =
		    (MDS_HDL)m_MDS_GET_VDEST_HDL_FROM_VDEST_ID(local_vdest_id);

		info->info.vdest_query.o_local_vdest_anc =
		    (V_DEST_QA)m_MDS_GET_ADEST;

		m_MDS_LOG_INFO(
		    "MCM:API: vdest_query for VDEST id = %d Successful",
		    local_vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	} else {
		/* VDEST Doesn't exist */
		m_MDS_LOG_INFO("MCM:API: vdest_query for VDEST id = %d FAILED",
			       local_vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
}

/*********************************************************

  Function NAME: mds_mcm_vdest_chg_role

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_vdest_chg_role(NCSMDS_ADMOP_INFO *info)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	V_DEST_RL current_role;
	MDS_VDEST_ID vdest_id;
	NCS_VDEST_TYPE local_vdest_policy;
	MDS_VDEST_ID local_vdest_id;
	MDS_SVC_INFO *svc_info = NULL;
	m_MDS_ENTER();

	/*
	From Dest_Hdl get the Current Role of VDEST

	if (Current Role = Active && New Role = Quiesced)
	    {
		STEP 1: Call mdtm_vdest_uninstall(vdest_id)
		(To unbind to intimate other VDEST with same id about its Role
	change.)

		STEP 2: Update New Role = Quiesced in Local VDEST table.

		STEP 3: For each service on this VDEST (obtained by a join on
	VDEST TABLE, PWE TABLE,SVC-TABLE) Call mdtm_svc_install(svc_hdl, NEW
	ROLE=Quiesced) Call mdtm_svc_uninstall(svc_hdl, Current ROLE)

		STEP 4: Start Quiesced timer
	    }

	if  (Current Role = Quiesced && New Role = Standby)
	    {
		STEP 1: Update New Role = Standby in Local VDEST table.
	    }
	if (New Role = Active)
	    {
		STEP 1: Call mdtm_vdest_install(vdest_id)
			 (To unbind to intimate other VDEST with same id about
	its Role change)
	    }
	if (new Role = Active / Standby)
	    {
		STEP 1: For each service on this VDEST (obtained by a join on
	VDEST TABLE, PWE TABLE,SVC-TABLE) Call mdtm_svc_uninstall(svc_hdl,
	Current ROLE) Call mdtm_svc_install(svc_hdl, NEW ROLE=Active/Standby)
	    }
	*/
	local_vdest_id = (MDS_VDEST_ID)info->info.vdest_query.i_local_vdest;

	status = mds_vdest_tbl_query(local_vdest_id);
	if (status == NCSCC_RC_FAILURE) {
		/* Vdest Doesn't exist */
		m_MDS_LOG_ERR(
		    "MCM:API: vdest_chg_role : VDEST id = %d Doesn't exist",
		    local_vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/* Get current Role */
	mds_vdest_tbl_get_role(local_vdest_id, &current_role);

	/* Get Policy of Vdest */
	mds_vdest_tbl_get_policy(local_vdest_id, &local_vdest_policy);

	if (current_role == V_DEST_RL_ACTIVE &&
	    info->info.vdest_config.i_new_role == V_DEST_RL_QUIESCED) {

		/* Bad: Directly accessing service tree */
		svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
		    &gl_mds_mcm_cb->svc_list, NULL);
		while (svc_info != NULL) {
			vdest_id =
			    m_MDS_GET_VDEST_ID_FROM_SVC_HDL(svc_info->svc_hdl);
			if (local_vdest_id == vdest_id) {
				mds_mdtm_svc_install(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, V_DEST_RL_STANDBY,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_uninstall(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, V_DEST_RL_ACTIVE,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_uninstall(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, V_DEST_RL_ACTIVE,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
			}
			svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
			    &gl_mds_mcm_cb->svc_list,
			    (uint8_t *)&svc_info->svc_hdl);
		}

		mds_vdest_tbl_update_role(local_vdest_id, V_DEST_RL_QUIESCED,
					  true);
		/* timer started in tbl_update */

		if (local_vdest_policy == NCS_VDEST_TYPE_MxN) {
			mds_mdtm_vdest_uninstall(local_vdest_id);
		}
		m_MDS_LOG_INFO(
		    "MCM:API: vdest_chg_role: VDEST id = %d : Role Changed : Active -> Quiesced",
		    local_vdest_id);
	} else if (current_role == V_DEST_RL_QUIESCED &&
		   info->info.vdest_config.i_new_role == V_DEST_RL_ACTIVE) {
		/* Bad: Directly accessing service tree */
		svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
		    &gl_mds_mcm_cb->svc_list, NULL);
		while (svc_info != NULL) {
			vdest_id =
			    m_MDS_GET_VDEST_ID_FROM_SVC_HDL(svc_info->svc_hdl);
			if (local_vdest_id == vdest_id) {
				mds_mdtm_svc_install(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, V_DEST_RL_ACTIVE,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_uninstall(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, V_DEST_RL_STANDBY,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_install(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, V_DEST_RL_ACTIVE,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
			}
			svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
			    &gl_mds_mcm_cb->svc_list,
			    (uint8_t *)&svc_info->svc_hdl);
		}

		mds_vdest_tbl_update_role(local_vdest_id, V_DEST_RL_ACTIVE,
					  true);
		/* timer started in tbl_update */

		if (local_vdest_policy == NCS_VDEST_TYPE_MxN) {
			mds_mdtm_vdest_install(local_vdest_id);
		}
		m_MDS_LOG_INFO(
		    "MCM:API: vdest_chg_role: VDEST id = %d : Role Changed : Quiesced -> Active",
		    local_vdest_id);

	} else if (current_role == V_DEST_RL_QUIESCED &&
		   info->info.vdest_config.i_new_role == V_DEST_RL_STANDBY) {
		/* Just update the role */
		mds_vdest_tbl_update_role(local_vdest_id, V_DEST_RL_STANDBY,
					  true);
		/* timer stopped in tbl_update */

		m_MDS_LOG_INFO(
		    "MCM:API:vdest_chg_role: VDEST id = %d : Role Changed : Quiesced -> Standby",
		    local_vdest_id);

	} else if (current_role == V_DEST_RL_STANDBY &&
		   info->info.vdest_config.i_new_role == V_DEST_RL_QUIESCED) {
		m_MDS_LOG_ERR(
		    "MCM:API:vdest_chg_role: VDEST id = %d : Role Changed : Standby -> Quiesced : NOT POSSIBLE",
		    local_vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	} else if (current_role == V_DEST_RL_STANDBY &&
		   info->info.vdest_config.i_new_role == V_DEST_RL_ACTIVE) {

		/* Bad: Directly accessing service tree */
		svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
		    &gl_mds_mcm_cb->svc_list, NULL);
		while (svc_info != NULL) {
			vdest_id =
			    m_MDS_GET_VDEST_ID_FROM_SVC_HDL(svc_info->svc_hdl);
			if (local_vdest_id == vdest_id) {
				mds_mdtm_svc_install(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope,
				    info->info.vdest_config.i_new_role,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_uninstall(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, current_role,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_install(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope,
				    info->info.vdest_config.i_new_role,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
			}
			svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
			    &gl_mds_mcm_cb->svc_list,
			    (uint8_t *)&svc_info->svc_hdl);
		}
		/* Update the role */
		mds_vdest_tbl_update_role(
		    local_vdest_id, info->info.vdest_config.i_new_role, true);

		if (local_vdest_policy == NCS_VDEST_TYPE_MxN) {
			/* Install vdest instance */
			mds_mdtm_vdest_install(local_vdest_id);
		}

		m_MDS_LOG_INFO(
		    "MCM:API:vdest_chg_role: VDEST id = %d : Role Changed : Standby -> Active",
		    local_vdest_id);

	} else if (current_role == V_DEST_RL_ACTIVE &&
		   info->info.vdest_config.i_new_role == V_DEST_RL_STANDBY) {

		/* Bad: Directly accessing service tree */
		svc_info = NULL;
		svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
		    &gl_mds_mcm_cb->svc_list, NULL);
		while (svc_info != NULL) {
			vdest_id =
			    m_MDS_GET_VDEST_ID_FROM_SVC_HDL(svc_info->svc_hdl);
			if (local_vdest_id == vdest_id) {
				mds_mdtm_svc_install(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope,
				    info->info.vdest_config.i_new_role,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_uninstall(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, current_role,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
				mds_mdtm_svc_uninstall(
				    m_MDS_GET_PWE_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    m_MDS_GET_SVC_ID_FROM_SVC_HDL(
					svc_info->svc_hdl),
				    svc_info->install_scope, current_role,
				    local_vdest_id, local_vdest_policy,
				    svc_info->svc_sub_part_ver);
			}
			svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
			    &gl_mds_mcm_cb->svc_list,
			    (uint8_t *)&svc_info->svc_hdl);
		}

		/* Update the role */
		mds_vdest_tbl_update_role(
		    local_vdest_id, info->info.vdest_config.i_new_role, true);

		if (local_vdest_policy == NCS_VDEST_TYPE_MxN) {
			/* Install vdest instance */
			mds_mdtm_vdest_uninstall(local_vdest_id);
		}

		m_MDS_LOG_INFO(
		    "MCM:API:vdest_chg_role: VDEST id = %d : Role Changed : Active -> Standby",
		    local_vdest_id);

	} else if (current_role == info->info.vdest_config.i_new_role) {
		m_MDS_LOG_INFO(
		    "MCM:API:vdest_chg_role: Role Changed FAILED for VDEST id = %d : OLD_ROLE = NEW_ROLE = %s",
		    local_vdest_id,
		    (current_role == V_DEST_RL_STANDBY)
			? "standby"
			: ((current_role == V_DEST_RL_ACTIVE) ? "active"
							      : "quieced"));
	}
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_pwe_create

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:


*********************************************************/
uint32_t mds_mcm_pwe_create(NCSMDS_ADMOP_INFO *info)
{

	m_MDS_ENTER();
	if ((info->info.pwe_create.i_pwe_id > NCSMDS_MAX_PWES) ||
	    (info->info.pwe_create.i_pwe_id == 0)) {
		m_MDS_LOG_ERR(
		    "MCM:API: pwe_create : FAILED : PWE id = %d not in prescribed range ",
		    info->info.pwe_create.i_pwe_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/* check whether hdl is of adest or vdest */
	if (info->info.pwe_create.i_mds_dest_hdl == (MDS_HDL)(m_ADEST_HDL)) {
		/* It is ADEST hdl */

		if (mds_pwe_tbl_query((MDS_VDEST_HDL)m_VDEST_ID_FOR_ADEST_ENTRY,
				      info->info.pwe_create.i_pwe_id) ==
		    NCSCC_RC_SUCCESS) {
			/* PWE already present */

			m_MDS_LOG_ERR(
			    "MCM:API: pwe_create : FAILED : PWE id = %d Already exist on Adest",
			    info->info.pwe_create.i_pwe_id);
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		} else {
			mds_pwe_tbl_add(
			    (MDS_VDEST_HDL)m_VDEST_ID_FOR_ADEST_ENTRY,
			    info->info.pwe_create.i_pwe_id,
			    (MDS_PWE_HDL *)&info->info.pwe_create
				.o_mds_pwe_hdl);

			/* info->info.pwe_create.o_mds_pwe_hdl = (NCSCONTEXT)
			   m_MDS_GET_PWE_HDL_FROM_PWE_ID_AND_VDEST_ID(info->info.pwe_create.i_pwe_id,
			   m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(info.pwe_create.i_mds_dest_hdl));
			 */

			m_MDS_LOG_INFO(
			    "MCM:API: PWE id = %d Created Successfully on Adest",
			    info->info.pwe_create.i_pwe_id);
			m_MDS_LEAVE();
			return NCSCC_RC_SUCCESS;
		}

	} else if ((info->info.pwe_create.i_mds_dest_hdl & 0xffff0000) != 0) {
		/* It is PWE hdl(pwe+vdest) */
		/* ERROR ERROR ERROR */
		m_MDS_LOG_ERR(
		    "MCM:API: pwe_create : FAILED : DEST HDL = %" PRId32
		    " passed is already PWE HDL",
		    info->info.pwe_create.i_mds_dest_hdl);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	} else {
		/* It is VDEST hdl */

		if (mds_pwe_tbl_query(
			(MDS_VDEST_HDL)info->info.pwe_create.i_mds_dest_hdl,
			info->info.pwe_create.i_pwe_id) == NCSCC_RC_SUCCESS) {
			/* PWE already present */
			m_MDS_LOG_ERR(
			    "MCM:API: pwe_create : FAILED : PWE id = %d Already exist on VDEST id = %d",
			    info->info.pwe_create.i_pwe_id,
			    m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(
				info->info.pwe_create.i_mds_dest_hdl));
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		} else {
			mds_pwe_tbl_add(
			    (MDS_VDEST_HDL)info->info.pwe_create.i_mds_dest_hdl,
			    info->info.pwe_create.i_pwe_id,
			    (MDS_PWE_HDL *)&info->info.pwe_create
				.o_mds_pwe_hdl);

			/* info->info.pwe_create.o_mds_pwe_hdl =(NCSCONTEXT)
			   m_MDS_GET_PWE_HDL_FROM_PWE_ID_AND_VDEST_ID(info->info.pwe_create.i_pwe_id,
			   m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(info.pwe_create.i_mds_dest_hdl));
			 */

			m_MDS_LOG_INFO(
			    "MCM:API: PWE id = %d Created Successfully on VDEST id = %d",
			    info->info.pwe_create.i_pwe_id,
			    m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(
				info->info.pwe_create.i_mds_dest_hdl));
			m_MDS_LEAVE();
			return NCSCC_RC_SUCCESS;
		}
	}
}

/*********************************************************

  Function NAME: mds_mcm_pwe_destroy

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_pwe_destroy(NCSMDS_ADMOP_INFO *info)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_PWE_HDL temp_pwe_hdl;
	MDS_SVC_ID temp_svc_id;
	NCSMDS_INFO temp_ncsmds_info;
	MDS_VDEST_ID vdest_id;
	PW_ENV_ID pwe_id;
	MDS_SVC_INFO *svc_info = NULL;

	m_MDS_ENTER();
	pwe_id =
	    m_MDS_GET_PWE_ID_FROM_PWE_HDL(info->info.pwe_destroy.i_mds_pwe_hdl);
	vdest_id = m_MDS_GET_VDEST_ID_FROM_PWE_HDL(
	    info->info.pwe_destroy.i_mds_pwe_hdl);

	status = mds_pwe_tbl_query(m_MDS_GET_VDEST_HDL_FROM_VDEST_ID(vdest_id),
				   pwe_id);
	if (status == NCSCC_RC_FAILURE) {
		/* Pwe doesn't exist */
		m_MDS_LOG_ERR(
		    "MCM:API: pwe_destroy : PWE id = %d Doesn't exist on VDEST id = %d",
		    pwe_id, vdest_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	temp_ncsmds_info.i_op = MDS_UNINSTALL;
	temp_ncsmds_info.info.svc_uninstall.i_msg_free_cb = NULL;

	/* STEP 1: For ever svc on PWE,
		    - Delete subscription entries if any from
		      LOCAL-UNIQ-SUBSCRIPTION-REQUEST-Table and
	   BLOCK-SEND-Request Table. - Call mdtm_svc_uninstall(svc_hdl, Current
	   Role) */

	svc_info = (MDS_SVC_INFO *)ncs_patricia_tree_getnext(
	    &gl_mds_mcm_cb->svc_list, NULL);
	while (svc_info != NULL) {
		MDS_SVC_INFO *next_svc_info =
			(MDS_SVC_INFO *)ncs_patricia_tree_getnext(
						&gl_mds_mcm_cb->svc_list,
						(uint8_t *)&svc_info->svc_hdl);
		temp_pwe_hdl =
		    m_MDS_GET_PWE_HDL_FROM_SVC_HDL(svc_info->svc_hdl);
		if (temp_pwe_hdl ==
		    (MDS_PWE_HDL)info->info.pwe_destroy.i_mds_pwe_hdl) {
			temp_svc_id =
			    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_info->svc_hdl);
			temp_ncsmds_info.i_mds_hdl =
			    info->info.pwe_destroy.i_mds_pwe_hdl;
			temp_ncsmds_info.i_svc_id = temp_svc_id;

			mds_mcm_svc_uninstall(&temp_ncsmds_info);
		}
		svc_info = next_svc_info;
	}

	/* STEP 2: Delete entry from PWE Table */
	status =
	    mds_pwe_tbl_del((MDS_PWE_HDL)info->info.pwe_destroy.i_mds_pwe_hdl);

	m_MDS_LOG_INFO(
	    "MCM:API: PWE id = %d on VDEST id = %d Destoryed Successfully",
	    pwe_id, vdest_id);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/****************************************************************************
 *
 * Function Name: mds_mcm_adm_pwe_query
 *
 * Purpose:       This function queries information about a pwe on a local
 *                vdest.
 *
 * Return Value:  NCSCC_RC_SUCCESS
 *                NCSCC_RC_FAILURE
 *
 ****************************************************************************/

uint32_t mds_mcm_adm_pwe_query(NCSMDS_ADMOP_INFO *info)
{
	uint32_t status = NCSCC_RC_SUCCESS;

	m_MDS_ENTER();
	status = mds_pwe_tbl_query(m_MDS_GET_VDEST_HDL_FROM_PWE_HDL(
				       info->info.pwe_query.i_local_dest_hdl),
				   info->info.pwe_query.i_pwe_id);
	if (status == NCSCC_RC_FAILURE) {
		/* Pwe Already exists */
		m_MDS_LOG_ERR(
		    "MCM:API: pwe_query : FAILURE : PWE id = %d Doesn't Exists",
		    info->info.pwe_query.i_pwe_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
	/* check whether hdl is of adest or vdest */
	if ((uint32_t)info->info.pwe_query.i_local_dest_hdl == m_ADEST_HDL) {
		/* It is ADEST hdl */

		info->info.pwe_query.o_mds_pwe_hdl =
		    (MDS_HDL)m_MDS_GET_PWE_HDL_FROM_VDEST_HDL_AND_PWE_ID(
			(MDS_VDEST_HDL)m_VDEST_ID_FOR_ADEST_ENTRY,
			info->info.pwe_query.i_pwe_id);

		m_MDS_LOG_INFO(
		    "MCM:API: pwe_query : SUCCESS for PWE id = %d on Adest",
		    info->info.pwe_query.i_pwe_id);
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	} else if ((((uint32_t)info->info.pwe_query.i_local_dest_hdl) &
		    0xffff0000) != 0) {
		/* It is PWE hdl(pwe+vdest) */
		/* ERROR ERROR ERROR */
		m_MDS_LOG_ERR(
		    "MCM:API: pwe_query : PWE Query Failed for PWE id = %d as DEST hdl = %" PRId32
		    " passed is PWE_HDL",
		    info->info.pwe_query.i_pwe_id,
		    info->info.pwe_query.i_local_dest_hdl);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	} else {
		/* It is VDEST hdl */

		info->info.pwe_query.o_mds_pwe_hdl =
		    (MDS_HDL)m_MDS_GET_PWE_HDL_FROM_VDEST_HDL_AND_PWE_ID(
			info->info.pwe_query.i_local_dest_hdl,
			info->info.pwe_query.i_pwe_id);

		m_MDS_LOG_INFO(
		    "MCM:API: pwe_query : SUCCESS for PWE id = %d on VDEST id = %d",
		    info->info.pwe_query.i_pwe_id,
		    m_MDS_GET_VDEST_ID_FROM_VDEST_HDL(
			info->info.pwe_query.i_local_dest_hdl));
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	}
}

/* ******************************************** */
/* ******************************************** */
/* ******************************************** */

/*              ncsmds_api                      */

/* ******************************************** */
/* ******************************************** */
/* ******************************************** */

/*********************************************************

  Function NAME: mds_mcm_svc_install

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/

uint32_t mds_mcm_svc_install(NCSMDS_INFO *info)
{
	return mds_svc_op_install(info);
}

/*********************************************************

  Function NAME: mds_mcm_svc_uninstall

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/

uint32_t mds_mcm_svc_uninstall(NCSMDS_INFO *info)
{
	return mds_svc_op_uninstall(info);
}

/*********************************************************

  Function NAME: mds_mcm_svc_subscribe

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/

uint32_t mds_mcm_svc_subscribe(NCSMDS_INFO *info)
{
	return mds_svc_op_subscribe(info);
}

/*********************************************************

  Function NAME: mds_mcm_svc_unsubscribe

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/

uint32_t mds_mcm_svc_unsubscribe(NCSMDS_INFO *info)
{
	return mds_svc_op_unsubscribe(info);
}

/*********************************************************

  Function NAME: mds_mcm_dest_query

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_dest_query(NCSMDS_INFO *info)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_SVC_HDL local_svc_hdl;
	MDS_SUBSCRIPTION_RESULTS_INFO *subtn_result_info = NULL;

	m_MDS_ENTER();
	info->info.query_dest.o_node_id =
	    m_MDS_GET_NODE_ID_FROM_ADEST(info->info.query_dest.i_dest);
	if (info->info.query_dest.o_node_id == 0) { /* Destination is VDEST */

		/* Get Service hdl */
		mds_svc_tbl_get_svc_hdl((MDS_PWE_HDL)info->i_mds_hdl,
					info->i_svc_id, &local_svc_hdl);

		if (info->info.query_dest.i_query_for_role ==
		    true) { /* Return Role given Anchor */
			status = mds_subtn_res_tbl_get_by_adest(
			    local_svc_hdl, info->info.query_dest.i_svc_id,
			    (MDS_VDEST_ID)info->info.query_dest.i_dest,
			    info->info.query_dest.info.query_for_role.i_anc,
			    &info->info.query_dest.info.query_for_role
				 .o_vdest_rl,
			    &subtn_result_info);
			if (status ==
			    NCSCC_RC_FAILURE) { /* No such subscription result
						   present */
				m_MDS_LEAVE();
				return NCSCC_RC_FAILURE;
			}
		} else { /* Return Anchor given Role */
			status = mds_subtn_res_tbl_getnext_any(
			    local_svc_hdl, info->info.query_dest.i_svc_id,
			    &subtn_result_info);
			while (status != NCSCC_RC_FAILURE) {
				if (subtn_result_info->key.vdest_id !=
				    m_VDEST_ID_FOR_ADEST_ENTRY) { /* Subscription
								     result
								     table entry
								     is VDEST
								     entry */

					if (subtn_result_info->key.vdest_id ==
						(MDS_VDEST_ID)info->info
						    .query_dest.i_dest &&
					    subtn_result_info->info.vdest_inst
						    .role ==
						info->info.query_dest.info
						    .query_for_anc.i_vdest_rl) {
						info->info.query_dest.info
						    .query_for_anc.o_anc =
						    (V_DEST_QA)subtn_result_info
							->key.adest;
						break;
					}
				}
				status = mds_subtn_res_tbl_getnext_any(
				    local_svc_hdl,
				    info->info.query_dest.i_svc_id,
				    &subtn_result_info);
			}
			if (status ==
			    NCSCC_RC_FAILURE) { /* while terminated as it didnt
						   find any result */
				m_MDS_LEAVE();
				return NCSCC_RC_FAILURE;
			}
		}
		/* Store common output parameters */
		if (subtn_result_info->key.adest == m_MDS_GET_ADEST)
			info->info.query_dest.o_local = true;
		else
			info->info.query_dest.o_local = false;
		/* Filling node id as previously it was checked for 0 for
		 * vdest/adest */
		info->info.query_dest.o_node_id =
		    m_MDS_GET_NODE_ID_FROM_ADEST(subtn_result_info->key.adest);
		info->info.query_dest.o_adest = subtn_result_info->key.adest;

		m_MDS_LOG_INFO(
		    "MCM:API: dest_query : Successful for Adest = %s",
		    subtn_result_info->sub_adest_details);
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	} else { /* Destination is ADEST */
		m_MDS_LOG_ERR("MCM:API: dest_query : FAILED : Adest  = %" PRIu64
			      " passed is Adest",
			      info->info.query_dest.i_dest);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
}

/*********************************************************

  Function NAME: mds_mcm_pwe_query

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_pwe_query(NCSMDS_INFO *info)
{
	MDS_VDEST_ID vdest_id;

	m_MDS_ENTER();
	info->info.query_pwe.o_pwe_id =
	    (PW_ENV_ID)m_MDS_GET_PWE_ID_FROM_PWE_HDL(
		(MDS_PWE_HDL)info->i_mds_hdl);

	if (((uint32_t)info->i_mds_hdl & m_ADEST_HDL) == m_ADEST_HDL) {
		/* It is ADEST hdl */
		info->info.query_pwe.o_absolute = 1;
		info->info.query_pwe.info.abs_info.o_adest = m_MDS_GET_ADEST;
	} else {
		/* It is VDEST hdl */
		info->info.query_pwe.o_absolute = 0;
		vdest_id = m_MDS_GET_VDEST_ID_FROM_PWE_HDL(
		    (MDS_PWE_HDL)info->i_mds_hdl);

		info->info.query_pwe.info.virt_info.o_vdest =
		    (MDS_DEST)vdest_id;
		info->info.query_pwe.info.virt_info.o_anc =
		    (V_DEST_QA)m_MDS_GET_ADEST;

		mds_vdest_tbl_get_role(
		    vdest_id, &info->info.query_pwe.info.virt_info.o_role);
	}
	m_MDS_LOG_INFO("MCM:API: query_pwe : Successful for PWE hdl = %" PRId32,
		       info->i_mds_hdl);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_node_subscribe

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_node_subscribe(NCSMDS_INFO *info)
{
	MDS_SVC_HDL svc_hdl;
	MDS_SVC_INFO *local_svc_info = NULL;

	m_MDS_ENTER();
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_query((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id)) {
		/* Service doesn't exist */
		m_MDS_LOG_ERR(
		    "MCM:API: node_subscribe : svc_id = %s(%d) on VDEST id = %d FAILED : svc_id Doesn't Exist",
		    get_svc_names(info->i_svc_id), info->i_svc_id,
		    m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl));
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
	mds_svc_tbl_get_svc_hdl((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id,
				&svc_hdl);

	/* Get Service info cb */
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_get(m_MDS_GET_PWE_HDL_FROM_SVC_HDL(svc_hdl),
			    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl),
			    (NCSCONTEXT)&local_svc_info)) {
		/* Service Doesn't exist */
		m_MDS_LOG_ERR(
		    "MCM: svc_id = %s(%d) doesnt exists, returning from mds_mcm_node_subscribe\n",
		    get_svc_names(info->i_svc_id), info->i_svc_id);
		return NCSCC_RC_FAILURE;
	}

	if (local_svc_info->i_node_subscr) {
		m_MDS_LOG_ERR(
		    "MCM_API: node_subscribe: svc_id = %s(%d) ,VDEST id = %d FAILED : subscription Exist",
		    get_svc_names(info->i_svc_id), info->i_svc_id,
		    m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl));
		return NCSCC_RC_FAILURE;
	} else {
		if (mds_mdtm_node_subscribe(
			svc_hdl, &local_svc_info->node_subtn_ref_val) !=
		    NCSCC_RC_SUCCESS) {
			m_MDS_LOG_ERR(
			    "MCM_API: mds_mdtm_node_subscribe: svc_id = %s(%d) Fail\n",
			    get_svc_names(info->i_svc_id), info->i_svc_id);
			return NCSCC_RC_FAILURE;
		}
		local_svc_info->i_node_subscr = 1;
	}
	m_MDS_LOG_DBG("MCM:API: mds_mcm_node_subscribe : S\n");
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_node_unsubscribe

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_node_unsubscribe(NCSMDS_INFO *info)
{
	MDS_SVC_HDL svc_hdl;
	MDS_SVC_INFO *local_svc_info = NULL;

	m_MDS_ENTER();
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_query((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id)) {
		/* Service doesn't exist */
		m_MDS_LOG_ERR(
		    "MCM:API: node_subscribe : svc_id = %s(%d) on VDEST id = %d FAILED : SVC Doesn't Exist",
		    get_svc_names(info->i_svc_id), info->i_svc_id,
		    m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl));
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}
	mds_svc_tbl_get_svc_hdl((MDS_PWE_HDL)info->i_mds_hdl, info->i_svc_id,
				&svc_hdl);

	/* Get Service info cb */
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_get(m_MDS_GET_PWE_HDL_FROM_SVC_HDL(svc_hdl),
			    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl),
			    (NCSCONTEXT)&local_svc_info)) {
		/* Service Doesn't exist */
		m_MDS_LOG_ERR(
		    "MCM: svc_id = %s(%d) doesnt exists, returning from mds_mcm_node_subscribe\n",
		    get_svc_names(info->i_svc_id), info->i_svc_id);
		return NCSCC_RC_FAILURE;
	}

	if (0 == local_svc_info->i_node_subscr) {
		m_MDS_LOG_ERR(
		    "MCM_API: node_subscribe: svc_id = %s(%d) ,VDEST id = %d FAILED : node subscription doesnt Exist",
		    get_svc_names(info->i_svc_id), info->i_svc_id,
		    m_MDS_GET_VDEST_ID_FROM_PWE_HDL(info->i_mds_hdl));
		return NCSCC_RC_FAILURE;
	} else {
		if (mds_mdtm_node_unsubscribe(
			local_svc_info->node_subtn_ref_val) !=
		    NCSCC_RC_SUCCESS) {
			m_MDS_LOG_ERR(
			    "MCM_API: mds_mdtm_node_unsubscribe: svc_id = %s(%d) Fail\n",
			    get_svc_names(info->i_svc_id), info->i_svc_id);
			return NCSCC_RC_FAILURE;
		}
		local_svc_info->i_node_subscr = 0;
		local_svc_info->node_subtn_ref_val = 0;
	}
	m_MDS_LOG_DBG("MCM:API: mds_mcm_node_unsubscribe : S\n");
	return NCSCC_RC_SUCCESS;
}

/* ******************************************** */
/* ******************************************** */
/* ******************************************** */

/*              MDTM to MCM if                  */

/* ******************************************** */
/* ******************************************** */
/* ******************************************** */

/*********************************************************

  Function NAME: mds_mcm_svc_up

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/

uint32_t mds_mcm_svc_up(PW_ENV_ID pwe_id, MDS_SVC_ID svc_id, V_DEST_RL role,
			NCSMDS_SCOPE_TYPE scope, MDS_VDEST_ID vdest_id,
			NCS_VDEST_TYPE vdest_policy, MDS_DEST adest,
			bool my_pcon, MDS_SVC_HDL local_svc_hdl,
			MDS_SUBTN_REF_VAL subtn_ref_val,
			MDS_SVC_PVT_SUB_PART_VER svc_sub_part_ver,
			MDS_SVC_ARCHWORD_TYPE archword_type)
{
	MCM_SVC_UP_INFO up_info;

	memset(&up_info, 0, sizeof(MCM_SVC_UP_INFO));
	up_info.adest = adest;
	up_info.arch_word = archword_type;
	up_info.local_svc_hdl = local_svc_hdl;
	up_info.my_pcon = my_pcon;
	up_info.pwe_id = pwe_id;
	up_info.role = role;
	up_info.scope = scope;
	up_info.sub_part_ver = svc_sub_part_ver;
	up_info.subtn_ref_val = subtn_ref_val;
	up_info.svc_id = svc_id;
	up_info.vdest = vdest_id;
	up_info.vdest_policy = vdest_policy;

	return mds_svc_op_up(&up_info);
}

/*********************************************************

  Function NAME: mds_mcm_svc_down

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_svc_down(PW_ENV_ID pwe_id, MDS_SVC_ID svc_id, V_DEST_RL role,
			  NCSMDS_SCOPE_TYPE scope, MDS_VDEST_ID vdest_id,
			  NCS_VDEST_TYPE vdest_policy, MDS_DEST adest,
			  bool my_pcon, MDS_SVC_HDL local_svc_hdl,
			  MDS_SUBTN_REF_VAL subtn_ref_val,
			  MDS_SVC_PVT_SUB_PART_VER svc_sub_part_ver,
			  MDS_SVC_ARCHWORD_TYPE archword_type)
{
	MCM_SVC_DOWN_INFO down_info;

	memset(&down_info, 0, sizeof(MCM_SVC_DOWN_INFO));
	down_info.adest = adest;
	down_info.arch_word = archword_type;
	down_info.local_svc_hdl = local_svc_hdl;
	down_info.my_pcon = my_pcon;
	down_info.pwe_id = pwe_id;
	down_info.role = role;
	down_info.scope = scope;
	down_info.sub_part_ver = svc_sub_part_ver;
	down_info.subtn_ref_val = subtn_ref_val;
	down_info.svc_id = svc_id;
	down_info.vdest = vdest_id;
	down_info.vdest_policy = vdest_policy;

	return mds_svc_op_down(&down_info);
}

/*********************************************************

  Function NAME: mds_mcm_node_up

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_node_up(MDS_SVC_HDL local_svc_hdl, NODE_ID node_id,
			 char *node_ip, uint16_t addr_family, char *node_name)
{
	MDS_MCM_MSG_ELEM *event_msg = NULL;
	MDS_SVC_INFO *local_svc_info = NULL;
	NCSMDS_CALLBACK_INFO *cbinfo = NULL;
	uint32_t status = NCSCC_RC_SUCCESS;

	/* Get Service info cb */
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_get(m_MDS_GET_PWE_HDL_FROM_SVC_HDL(local_svc_hdl),
			    m_MDS_GET_SVC_ID_FROM_SVC_HDL(local_svc_hdl),
			    (NCSCONTEXT)&local_svc_info)) {
		/* Service Doesn't exist */
		m_MDS_LOG_ERR(
		    " SVC doesnt exists, returning from mds_mcm_node_up\n");
		return NCSCC_RC_FAILURE;
	}

	if (0 == local_svc_info->i_node_subscr) {
		/* Node Subscription Doesn't exist */
		m_MDS_LOG_ERR(
		    " Node subscription doesnt exists, returning from mds_mcm_node_up\n");
		return NCSCC_RC_FAILURE;
	}

	event_msg = m_MMGR_ALLOC_MSGELEM;
	if (event_msg == NULL) {
		m_MDS_LOG_ERR("mds_mcm_node_up out of memory\n");
		return NCSCC_RC_FAILURE;
	}
	memset(event_msg, 0, sizeof(MDS_MCM_MSG_ELEM));
	event_msg->type = MDS_EVENT_TYPE;
	event_msg->pri = MDS_SEND_PRIORITY_MEDIUM;

	/* Temp ptr to cbinfo in event_msg */
	cbinfo = &event_msg->info.event.cbinfo; /* NOTE: Aliased pointer */

	cbinfo->i_op = MDS_CALLBACK_NODE_EVENT;
	cbinfo->i_yr_svc_hdl = local_svc_info->yr_svc_hdl;
	cbinfo->i_yr_svc_id = local_svc_info->svc_id;

	cbinfo->info.node_evt.node_chg = NCSMDS_NODE_UP;

	cbinfo->info.node_evt.node_id = node_id;
	cbinfo->info.node_evt.addr_family = addr_family;
	if (node_ip) {
		cbinfo->info.node_evt.ip_addr_len = strlen(node_ip);
		cbinfo->info.node_evt.length =
		    cbinfo->info.node_evt.ip_addr_len;
		memcpy(cbinfo->info.node_evt.ip_addr, node_ip,
		       cbinfo->info.node_evt.ip_addr_len);
	}

	m_MDS_LOG_INFO(
	    "MDTM: node up node_ip:%s, length:%d node_id:%u addr_family:%d msg_type:%d",
	    cbinfo->info.node_evt.ip_addr, cbinfo->info.node_evt.ip_addr_len,
	    cbinfo->info.node_evt.node_id, cbinfo->info.node_evt.addr_family,
	    cbinfo->info.node_evt.node_chg);
	if (node_name) {
		cbinfo->info.node_evt.i_node_name_len = strlen(node_name) + 1;
		strncpy(cbinfo->info.node_evt.i_node_name, node_name,
			_POSIX_HOST_NAME_MAX - 1);
	}

	/* Post to mail box If Q Ownership is enabled Else Call user callback */
	if (local_svc_info->q_ownership == true) {

		if ((m_NCS_IPC_SEND(&local_svc_info->q_mbx, event_msg,
				    NCS_IPC_PRIORITY_NORMAL)) !=
		    NCSCC_RC_SUCCESS) {
			/* Message Queuing failed */
			m_MMGR_FREE_MSGELEM(event_msg);
			m_MDS_LOG_ERR(
			    "SVC Mailbox IPC_SEND : NODE UP EVENT : FAILED\n");
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		} else {
			m_MDS_LOG_INFO(
			    "SVC mailbox IPC_SEND : NODE UP EVENT : Success\n");
			m_MDS_LEAVE();
			return NCSCC_RC_SUCCESS;
		}
	} else {
		/* Call user callback */
		status = local_svc_info->cback_ptr(cbinfo);
		m_MMGR_FREE_MSGELEM(event_msg);
	}
	m_MDS_LEAVE();
	return status;
}

/*********************************************************

  Function NAME: mds_mcm_node_down

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_node_down(MDS_SVC_HDL local_svc_hdl, NODE_ID node_id,
			   uint16_t addr_family)

{
	MDS_MCM_MSG_ELEM *event_msg = NULL;
	MDS_SVC_INFO *local_svc_info = NULL;
	NCSMDS_CALLBACK_INFO *cbinfo = NULL;
	uint32_t status = NCSCC_RC_SUCCESS;

	/* Get Service info cb */
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_get(m_MDS_GET_PWE_HDL_FROM_SVC_HDL(local_svc_hdl),
			    m_MDS_GET_SVC_ID_FROM_SVC_HDL(local_svc_hdl),
			    (NCSCONTEXT)&local_svc_info)) {
		/* Service Doesn't exist */
		m_MDS_LOG_ERR(
		    " SVC doesnt exists, returning from mds_mcm_node_down\n");
		return NCSCC_RC_FAILURE;
	}

	if (0 == local_svc_info->i_node_subscr) {
		/* Node Subscription Doesn't exist */
		m_MDS_LOG_ERR(
		    " Node subscription doesnt exists, returning from mds_mcm_node_down\n");
		return NCSCC_RC_FAILURE;
	}

	event_msg = m_MMGR_ALLOC_MSGELEM;
	if (event_msg == NULL) {
		m_MDS_LOG_ERR("mds_mcm_node_up out of memory\n");
		return NCSCC_RC_FAILURE;
	}
	memset(event_msg, 0, sizeof(MDS_MCM_MSG_ELEM));
	event_msg->type = MDS_EVENT_TYPE;
	event_msg->pri = MDS_SEND_PRIORITY_MEDIUM;

	/* Temp ptr to cbinfo in event_msg */
	cbinfo = &event_msg->info.event.cbinfo; /* NOTE: Aliased pointer */

	cbinfo->i_op = MDS_CALLBACK_NODE_EVENT;
	cbinfo->i_yr_svc_hdl = local_svc_info->yr_svc_hdl;
	cbinfo->i_yr_svc_id = local_svc_info->svc_id;

	cbinfo->info.node_evt.node_chg = NCSMDS_NODE_DOWN;

	cbinfo->info.node_evt.node_id = node_id;
	cbinfo->info.node_evt.addr_family = addr_family;

	m_MDS_LOG_INFO(
	    "MDTM: node down  node_id:%u  addr_family:%d  msg_type:%d",
	    cbinfo->info.node_evt.node_id, cbinfo->info.node_evt.addr_family,
	    cbinfo->info.node_evt.node_chg);
	/* Post to mail box If Q Ownership is enabled Else Call user callback */
	if (local_svc_info->q_ownership == true) {

		if ((m_NCS_IPC_SEND(&local_svc_info->q_mbx, event_msg,
				    NCS_IPC_PRIORITY_NORMAL)) !=
		    NCSCC_RC_SUCCESS) {
			/* Message Queuing failed */
			m_MMGR_FREE_MSGELEM(event_msg);
			m_MDS_LOG_ERR(
			    "SVC Mailbox IPC_SEND : NODE DOWN EVENT : FAILED\n");
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		} else {
			m_MDS_LOG_INFO(
			    "SVC mailbox IPC_SEND : NODE DOWN EVENT : Success\n");
			m_MDS_LEAVE();
			return NCSCC_RC_SUCCESS;
		}
	} else {
		/* Call user callback */
		status = local_svc_info->cback_ptr(cbinfo);
		m_MMGR_FREE_MSGELEM(event_msg);
	}
	m_MDS_LEAVE();
	return status;
}

/*********************************************************

  Function NAME: mds_mcm_msg_loss

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
void mds_mcm_msg_loss(MDS_SVC_HDL local_svc_hdl, MDS_DEST rem_adest,
		      MDS_SVC_ID rem_svc_id, MDS_VDEST_ID rem_vdest_id)

{
	MDS_MCM_MSG_ELEM *event_msg = NULL;
	MDS_SVC_INFO *local_svc_info = NULL;
	NCSMDS_CALLBACK_INFO *cbinfo = NULL;

	/* Get Service info cb */
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_get(m_MDS_GET_PWE_HDL_FROM_SVC_HDL(local_svc_hdl),
			    m_MDS_GET_SVC_ID_FROM_SVC_HDL(local_svc_hdl),
			    (NCSCONTEXT)&local_svc_info)) {
		/* Service Doesn't exist */
		m_MDS_LOG_ERR(
		    " SVC doesnt exists, returning from mds_mcm_msg_loss\n");
		return;
	}

	/* Check whether the msg loss is enabled or not */
	if (true != local_svc_info->i_msg_loss_indication) {
		m_MDS_LOG_NOTIFY("MSG loss is not enabled mds_mcm_msg_loss\n");
		return;
	}

	event_msg = m_MMGR_ALLOC_MSGELEM;
	if (event_msg == NULL) {
		m_MDS_LOG_ERR("mds_mcm_msg_loss out of memory\n");
		return;
	}
	memset(event_msg, 0, sizeof(MDS_MCM_MSG_ELEM));
	event_msg->type = MDS_EVENT_TYPE;
	event_msg->pri = MDS_SEND_PRIORITY_MEDIUM;

	/* Temp ptr to cbinfo in event_msg */
	cbinfo = &event_msg->info.event.cbinfo; /* NOTE: Aliased pointer */

	cbinfo->i_op = MDS_CALLBACK_MSG_LOSS;
	cbinfo->i_yr_svc_hdl = local_svc_info->yr_svc_hdl;
	cbinfo->i_yr_svc_id = local_svc_info->svc_id;

	cbinfo->info.msg_loss_evt.i_dest = rem_adest;
	cbinfo->info.msg_loss_evt.i_pwe_id =
	    m_MDS_GET_PWE_ID_FROM_SVC_HDL(local_svc_hdl);
	cbinfo->info.msg_loss_evt.i_svc_id = rem_svc_id;
	cbinfo->info.msg_loss_evt.i_vdest_id = rem_vdest_id;

	/* Post to mail box If Q Ownership is enabled Else Call user callback */
	if (local_svc_info->q_ownership == true) {

		if ((m_NCS_IPC_SEND(&local_svc_info->q_mbx, event_msg,
				    NCS_IPC_PRIORITY_NORMAL)) !=
		    NCSCC_RC_SUCCESS) {
			/* Message Queuing failed */
			m_MMGR_FREE_MSGELEM(event_msg);
			m_MDS_LOG_ERR(
			    "SVC Mailbox IPC_SEND : MSG LOSS EVENT : FAILED\n");
			m_MDS_LEAVE();
			return;
		} else {
			m_MDS_LOG_INFO(
			    "SVC mailbox IPC_SEND : MSG LOSS EVENT : Success\n");
			m_MDS_LEAVE();
			return;
		}
	} else {
		/* Call user callback */
		local_svc_info->cback_ptr(cbinfo);
		m_MMGR_FREE_MSGELEM(event_msg);
	}
	m_MDS_LEAVE();
	return;
}

/*********************************************************

  Function NAME: mds_mcm_vdest_up

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_vdest_up(MDS_VDEST_ID vdest_id, MDS_DEST adest)
{
	V_DEST_RL current_role;
	NCSMDS_ADMOP_INFO chg_role_info;

	m_MDS_ENTER();
	if (adest == gl_mds_mcm_cb->adest) { /* Got up from this vdest itself */
		/* Discard */
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	}
	/* STEP 1: Get the Current role and Dest_hdl of VDEST
		    with id VDEST_ID from LOCAL-VDEST table. */
	mds_vdest_tbl_get_role(vdest_id, &current_role);
	/* STEP 2: if (Current Role == Active)
		call dest_chg_role(dest_hdl, New Role = QUIESCED) */

	if (current_role == V_DEST_RL_ACTIVE) {
		/* change role of local vdest to quiesced */
		chg_role_info.i_op = MDS_ADMOP_VDEST_CONFIG;
		chg_role_info.info.vdest_config.i_vdest = vdest_id;
		chg_role_info.info.vdest_config.i_new_role = V_DEST_RL_QUIESCED;
		mds_mcm_vdest_chg_role(&chg_role_info);
	} else {
		/* Do nothing. Each service subscribed will take care
		   of services coming up on that vdest */
	}

	m_MDS_LOG_INFO(
	    "MCM:API: vdest_up : Got UP from vdest_id id = %d at Adest <0x%08x, %u>",
	    vdest_id, m_MDS_GET_NODE_ID_FROM_ADEST(adest),
	    m_MDS_GET_PROCESS_ID_FROM_ADEST(adest));

	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_vdest_down

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_vdest_down(MDS_VDEST_ID vdest_id, MDS_DEST adest)
{
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_user_callback

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_user_event_callback(MDS_SVC_HDL local_svc_hdl,
				     PW_ENV_ID pwe_id, MDS_SVC_ID svc_id,
				     V_DEST_RL role, MDS_VDEST_ID vdest_id,
				     MDS_DEST adest, NCSMDS_CHG event_type,
				     MDS_SVC_PVT_SUB_PART_VER svc_sub_part_ver,
				     MDS_SVC_ARCHWORD_TYPE archword_type)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	MDS_SVC_INFO *local_svc_info = NULL;
	MDS_SUBSCRIPTION_INFO *local_subtn_info = NULL;
	MDS_MCM_MSG_ELEM *event_msg = NULL;
	NCSMDS_CALLBACK_INFO *cbinfo = NULL;
	MDS_AWAIT_DISC_QUEUE *curr_queue_element = NULL;

	m_MDS_ENTER();
	/* Get Service info cb */
	if (NCSCC_RC_SUCCESS !=
	    mds_svc_tbl_get(m_MDS_GET_PWE_HDL_FROM_SVC_HDL(local_svc_hdl),
			    m_MDS_GET_SVC_ID_FROM_SVC_HDL(local_svc_hdl),
			    (NCSCONTEXT)&local_svc_info)) {
		/* Service Doesn't exist */
		m_MDS_LOG_ERR(
		    "MDS_SND_RCV: SVC doesnt exists, returning from mds_mcm_user_event_callback=%d\n",
		    svc_id);
		return NCSCC_RC_FAILURE;
	}
	/* if previous MDS version subscriptions  increment or decrement count
	   on NCSMDS_UP/DOWN event , this is Mcast or Bcast differentiators if
	   conut is ZERO mcast else count is grater than ZERO bcast
	   (multi-unicast) */
	if ((archword_type & 0x7) < 1) {
		if (event_type == NCSMDS_UP) {
			local_svc_info->subtn_info->prev_ver_sub_count++;
			m_MDS_LOG_DBG(
			    "MDTM: NCSMDS_UP for svc_id = %s(%d), archword :%d ,prev_ver_sub_count %u",
			    get_svc_names(svc_id), svc_id, archword_type,
			    local_svc_info->subtn_info->prev_ver_sub_count);
		} else if (event_type == NCSMDS_DOWN) {
			if (local_svc_info->subtn_info->prev_ver_sub_count > 0)
				local_svc_info->subtn_info
				    ->prev_ver_sub_count--;
			m_MDS_LOG_DBG(
			    "MDTM: NCSMDS_DOWN for svc_id = %s(%d), archword :%d ,prev_ver_sub_count %u",
			    get_svc_names(svc_id), svc_id, archword_type,
			    local_svc_info->subtn_info->prev_ver_sub_count);
		}
	}
	/* Get Subtn info */
	mds_subtn_tbl_get(local_svc_hdl, svc_id, &local_subtn_info);
	/* If this function returns failure, then its invalid state. Handling
	 * required */
	if (NULL == local_subtn_info) {
		m_MDS_LOG_ERR(
		    "MCM_API:Sub fr svc_id = %s(%d) to svc_id = %s(%d) doesnt exists, ret fr mds_mcm_user_event_callback\n",
		    get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(local_svc_hdl)),
		    m_MDS_GET_SVC_ID_FROM_SVC_HDL(local_svc_hdl),
		    get_svc_names(svc_id), svc_id);
		return NCSCC_RC_FAILURE;
	}

	/* Raise selection objects of blocking send */
	if ((event_type == NCSMDS_UP) || (event_type == NCSMDS_RED_UP)) {

		curr_queue_element = local_subtn_info->await_disc_queue;

		while (curr_queue_element != NULL) {
			if (curr_queue_element->send_type !=
				MDS_SENDTYPE_BCAST &&
			    curr_queue_element->send_type !=
				MDS_SENDTYPE_RBCAST) {
				/* Condition to raise selection object for
				 * pending SEND is not checked properly in
				 * mds_c_api.c */
				if (vdest_id ==
				    m_VDEST_ID_FOR_ADEST_ENTRY) { /* We got UP
								     from ADEST
								     so check if
								     any send to
								     svc on this
								     ADEST is
								     remaining
								   */

					if (curr_queue_element->vdest ==
					    m_VDEST_ID_FOR_ADEST_ENTRY) { /* Blocked
									     send
									     is
									     to
									     send
									     to
									     ADEST
									   */

						if (adest ==
						    curr_queue_element
							->adest) { /* Raise
								      selection
								      object for
								      this entry
								      as we got
								      up from
								      that adest
								    */
							m_NCS_SEL_OBJ_IND(
							    &curr_queue_element
								 ->sel_obj);
							m_MDS_LOG_INFO(
							    "MCM:API: event_callback : Raising SEL_OBJ for VDEST id = %d Send to ADEST <0x%08x, %u>",
							    curr_queue_element
								->vdest,
							    m_MDS_GET_NODE_ID_FROM_ADEST(
								curr_queue_element
								    ->adest),
							    m_MDS_GET_PROCESS_ID_FROM_ADEST(
								curr_queue_element
								    ->adest));
						}
					}
				} else { /* We got up for VDEST so check if any
					    send to svc on this VDEST is
					    remaining */

					if (curr_queue_element->adest == 0 &&
					    curr_queue_element->vdest ==
						vdest_id &&
					    event_type ==
						NCSMDS_UP) { /* Blocked send is
								send to VDEST
								and we got
								atleast one
								ACTIVE */
						m_NCS_SEL_OBJ_IND(
						    &curr_queue_element
							 ->sel_obj);
						m_MDS_LOG_INFO(
						    "MCM:API: event_callback : Raising SEL_OBJ for Send to VDEST id = %d on ADEST <0x%08x, %u>",
						    curr_queue_element->vdest,
						    m_MDS_GET_NODE_ID_FROM_ADEST(
							curr_queue_element
							    ->adest),
						    m_MDS_GET_PROCESS_ID_FROM_ADEST(
							curr_queue_element
							    ->adest));
					}
				}
			}
			/* Increment current_queue_element pointer */
			curr_queue_element = curr_queue_element->next_msg;
		}
	}

	/* Check whether subscription is Implicit or Explicit */
	if (local_subtn_info->subtn_type == MDS_SUBTN_IMPLICIT) {
		/* Dont send event to user */
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	}

	/*  Fix for mem-leak found during testing: Allocate and set
	 ** struct to send message-element to client (only for non-implicit
	 ** callbacks)
	 */
	event_msg = m_MMGR_ALLOC_MSGELEM;
	if (event_msg == NULL) {
		m_MDS_LOG_ERR(
		    "Subscription callback processing:Out of memory\n");
		return NCSCC_RC_FAILURE;
	}
	memset(event_msg, 0, sizeof(MDS_MCM_MSG_ELEM));
	event_msg->type = MDS_EVENT_TYPE;
	event_msg->pri = MDS_SEND_PRIORITY_MEDIUM;

	/* Temp ptr to cbinfo in event_msg */
	cbinfo = &event_msg->info.event.cbinfo; /* NOTE: Aliased pointer */

	cbinfo->i_op = MDS_CALLBACK_SVC_EVENT;
	cbinfo->i_yr_svc_hdl = local_svc_info->yr_svc_hdl;
	cbinfo->i_yr_svc_id = local_svc_info->svc_id;

	cbinfo->info.svc_evt.i_change = event_type;

	if (vdest_id == m_VDEST_ID_FOR_ADEST_ENTRY) {
		/* Service is on remote ADEST */
		cbinfo->info.svc_evt.i_dest = adest;
		if ((event_type == NCSMDS_UP) || (event_type == NCSMDS_RED_UP))
			get_adest_details(adest,
					  cbinfo->info.svc_evt.i_dest_details);
		cbinfo->info.svc_evt.i_anc = 0; /* anchor same as adest */
	} else {
		/* Service is on remote VDEST */
		cbinfo->info.svc_evt.i_dest = (MDS_DEST)vdest_id;

		if (event_type == NCSMDS_RED_UP ||
		    event_type == NCSMDS_RED_DOWN ||
		    event_type == NCSMDS_CHG_ROLE) {
			cbinfo->info.svc_evt.i_anc =
			    adest; /* anchor same as adest */
			if (event_type == NCSMDS_RED_UP)
				get_adest_details(
				    adest, cbinfo->info.svc_evt.i_dest_details);
		} else {
			cbinfo->info.svc_evt.i_anc = 0;
		}
	}

	cbinfo->info.svc_evt.i_role = role;
	cbinfo->info.svc_evt.i_node_id = m_MDS_GET_NODE_ID_FROM_ADEST(adest);
	cbinfo->info.svc_evt.i_pwe_id = pwe_id;
	cbinfo->info.svc_evt.i_svc_id = svc_id;
	cbinfo->info.svc_evt.i_your_id = local_svc_info->svc_id;

	cbinfo->info.svc_evt.svc_pwe_hdl =
	    (MDS_HDL)m_MDS_GET_PWE_HDL_FROM_SVC_HDL(local_svc_info->svc_hdl);
	cbinfo->info.svc_evt.i_rem_svc_pvt_ver = svc_sub_part_ver;

	/* Post to mail box If Q Ownership is enabled Else Call user callback */
	if (local_svc_info->q_ownership == true) {

		if ((m_NCS_IPC_SEND(&local_svc_info->q_mbx, event_msg,
				    NCS_IPC_PRIORITY_NORMAL)) !=
		    NCSCC_RC_SUCCESS) {
			/* Message Queuing failed */
			m_MMGR_FREE_MSGELEM(event_msg);
			m_MDS_LOG_ERR(
			    "SVC Mailbox IPC_SEND : SVC EVENT : FAILED\n");
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		} else {
			m_MDS_LOG_INFO(
			    "SVC mailbox IPC_SEND : SVC EVENT : Success\n");
			m_MDS_LEAVE();
			return NCSCC_RC_SUCCESS;
		}
	} else {
		/* Call user callback */
		status = local_svc_info->cback_ptr(cbinfo);
		m_MMGR_FREE_MSGELEM(event_msg);
	}
	m_MDS_LEAVE();
	return status;
}

/*********************************************************

  Function NAME: mds_mcm_quiesced_tmr_expiry

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_quiesced_tmr_expiry(MDS_VDEST_ID vdest_id)
{
	uint32_t status = NCSCC_RC_SUCCESS;
	NCSMDS_CALLBACK_INFO *cbinfo;
	MDS_SVC_INFO *local_svc_info = NULL;
	MDS_MCM_MSG_ELEM *event_msg = NULL;

	m_MDS_ENTER();
	m_MDS_LOG_INFO("MCM:API: quieseced_tmr expired for VDEST id = %d",
		       vdest_id);

	/* Update vdest role to Standby */
	mds_vdest_tbl_update_role(vdest_id, V_DEST_RL_STANDBY, false);
	/* Turn off timer done in update_role */

	/* Send Quiesced ack event to all services on this vdest */

	status = mds_svc_tbl_getnext_on_vdest(vdest_id, 0, &local_svc_info);
	while (status == NCSCC_RC_SUCCESS) {
		event_msg = m_MMGR_ALLOC_MSGELEM;
		memset(event_msg, 0, sizeof(MDS_MCM_MSG_ELEM));

		event_msg->type = MDS_EVENT_TYPE;
		event_msg->pri = MDS_SEND_PRIORITY_MEDIUM;

		cbinfo = &event_msg->info.event.cbinfo;
		cbinfo->i_yr_svc_hdl = local_svc_info->yr_svc_hdl;
		cbinfo->i_yr_svc_id = local_svc_info->svc_id;
		cbinfo->i_op = MDS_CALLBACK_QUIESCED_ACK;
		cbinfo->info.quiesced_ack.i_dummy = 1; /* dummy */

		/* Post to mail box If Q Ownership is enabled Else Call user
		 * callback */
		if (local_svc_info->q_ownership == true) {

			if ((m_NCS_IPC_SEND(&local_svc_info->q_mbx, event_msg,
					    NCS_IPC_PRIORITY_NORMAL)) !=
			    NCSCC_RC_SUCCESS) {
				/* Message Queuing failed */
				m_MMGR_FREE_MSGELEM(event_msg);
				m_MDS_LOG_ERR(
				    "SVC Mailbox IPC_SEND : Quiesced Ack : FAILED\n");
				m_MDS_LOG_INFO(
				    "MCM:API: Entering : mds_mcm_quiesced_tmr_expiry");
				return NCSCC_RC_FAILURE;
			} else {
				m_MDS_LOG_DBG(
				    "SVC mailbox IPC_SEND : Quiesced Ack : SUCCESS\n");
			}
		} else {
			/* Call user callback */
			status = local_svc_info->cback_ptr(cbinfo);
			m_MMGR_FREE_MSGELEM(event_msg);
		}
		status = mds_svc_tbl_getnext_on_vdest(
		    vdest_id, local_svc_info->svc_hdl, &local_svc_info);
	}
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_subscription_tmr_expiry

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_subscription_tmr_expiry(MDS_SVC_HDL svc_hdl,
					 MDS_SVC_ID sub_svc_id)
{
	MDS_SUBSCRIPTION_INFO *subtn_info = NULL;
	MDS_AWAIT_DISC_QUEUE *tmp_await_active_queue = NULL, *bk_ptr = NULL;

	m_MDS_ENTER();
	m_MDS_LOG_INFO(
	    "MCM:API: subscription_tmr expired for svc_id = %s(%d) Subscribed to svc_id = %s(%d)",
	    get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl)),
	    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl), get_svc_names(sub_svc_id),
	    sub_svc_id);

	mds_subtn_tbl_get(svc_hdl, sub_svc_id, &subtn_info);

	if (subtn_info == NULL) {
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	}

	/* Turn timer flag off */
	subtn_info->tmr_flag = false;
	/* Point tmr_req_info to NULL, freeding will be done in expiry function
	 */
	subtn_info->tmr_req_info = NULL;
	/* Destroy timer as it will never be started again */
	m_NCS_TMR_DESTROY(subtn_info->discovery_tmr);
	/* Destroying Handle and freeing tmr_req_info done in expiry function
	 * itself */

	tmp_await_active_queue = subtn_info->await_disc_queue;

	if (tmp_await_active_queue == NULL) {
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	}

	if (tmp_await_active_queue->next_msg == NULL) {
		m_NCS_SEL_OBJ_IND(&tmp_await_active_queue->sel_obj);
		m_MDS_LEAVE();
		return NCSCC_RC_SUCCESS;
	}

	while (tmp_await_active_queue != NULL) {
		bk_ptr = tmp_await_active_queue;
		tmp_await_active_queue = tmp_await_active_queue->next_msg;

		/* Raise selection object */
		m_NCS_SEL_OBJ_IND(&bk_ptr->sel_obj);
		/* Memory for this will be get freed by awaiting request */
	}
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_await_active_tmr_expiry

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  1 - NCSCC_RC_SUCCESS
	    2 - NCSCC_RC_FAILURE

*********************************************************/
uint32_t mds_mcm_await_active_tmr_expiry(MDS_SVC_HDL svc_hdl,
					 MDS_SVC_ID sub_svc_id,
					 MDS_VDEST_ID vdest_id)
{
	MDS_SUBSCRIPTION_RESULTS_INFO *active_subtn_result_info;
	MDS_DEST active_adest;
	bool tmr_running;
	uint32_t status = NCSCC_RC_SUCCESS;

	m_MDS_ENTER();
	m_MDS_LOG_INFO(
	    "MCM:API: await_active_tmr expired for svc_id = %s(%d) Subscribed to svc_id = %s(%d) on VDEST id = %d",
	    get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl)),
	    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl), get_svc_names(sub_svc_id),
	    sub_svc_id, vdest_id);

	mds_subtn_res_tbl_get(svc_hdl, sub_svc_id, vdest_id, &active_adest,
			      &tmr_running, &active_subtn_result_info, true);
	/* Delete all pending messages */
	mds_await_active_tbl_del(active_subtn_result_info->info.active_vdest
				     .active_route_info->await_active_queue);

	/* Call user callback DOWN */
	status = mds_mcm_user_event_callback(
	    svc_hdl, m_MDS_GET_PWE_ID_FROM_SVC_HDL(svc_hdl), sub_svc_id,
	    V_DEST_RL_STANDBY, vdest_id, 0, NCSMDS_DOWN,
	    active_subtn_result_info->rem_svc_sub_part_ver,
	    MDS_SVC_ARCHWORD_TYPE_UNSPECIFIED);
	if (status != NCSCC_RC_SUCCESS) {
		/* Callback failure */
		m_MDS_LOG_ERR(
		    "MCM:API: await_active_tmr_expiry : DOWN Callback Failure for svc_id = %s(%d) subscribed to svc_id = %s(%d) on VDEST id = %d",
		    get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl)),
		    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl),
		    get_svc_names(sub_svc_id), sub_svc_id, vdest_id);
	}

	m_MDS_LOG_INFO(
	    "MCM:API: svc_down : await_active_tmr_expiry : svc_id = %s(%d) on DEST id = %d got DOWN for svc_id = %s(%d) on VDEST id = %d",
	    get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl)),
	    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl),
	    m_MDS_GET_VDEST_ID_FROM_SVC_HDL(svc_hdl), get_svc_names(sub_svc_id),
	    sub_svc_id, vdest_id);

	/* Destroy timer */
	m_NCS_TMR_DESTROY(active_subtn_result_info->info.active_vdest
			      .active_route_info->await_active_tmr);

	/* Freeing Handle and tmr_req_info done in expiry function itself */

	/* Delete Active entry from Subscription result tree */
	m_MMGR_FREE_SUBTN_ACTIVE_RESULT_INFO(
	    active_subtn_result_info->info.active_vdest.active_route_info);

	/* Delete subscription entry from tree */
	ncs_patricia_tree_del(&gl_mds_mcm_cb->subtn_results,
			      (NCS_PATRICIA_NODE *)active_subtn_result_info);

	/* Delete memory for active entry */
	m_MMGR_FREE_SUBTN_RESULT_INFO(active_subtn_result_info);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_subtn_add

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  Adest Hdl

*********************************************************/
uint32_t mds_mcm_subtn_add(MDS_SVC_HDL svc_hdl, MDS_SVC_ID subscr_svc_id,
			   NCSMDS_SCOPE_TYPE scope, MDS_VIEW view,
			   MDS_SUBTN_TYPE subtn_type)
{
	MDS_SUBTN_REF_VAL subtn_ref_val = 0;
	uint32_t status = NCSCC_RC_SUCCESS;

	m_MDS_ENTER();
	/* Add in Subscription Tabel */
	mds_subtn_tbl_add(svc_hdl, subscr_svc_id, scope, view, subtn_type);
	/*  STEP 2.c: Call mdtm_svc_subscribe(sub_svc_id, scope, svc_hdl) */
	status = mds_mdtm_svc_subscribe(
	    (PW_ENV_ID)m_MDS_GET_PWE_ID_FROM_SVC_HDL(svc_hdl), subscr_svc_id,
	    scope, svc_hdl, &subtn_ref_val);
	if (status != NCSCC_RC_SUCCESS) {
		/* MDTM is unable to subscribe */

		/* Remove Subscription info from MCM database */
		mds_subtn_tbl_del(svc_hdl, subscr_svc_id);

		m_MDS_LOG_ERR(
		    "MCM:API: mcm_subtn_add : Can't Subscribe from SVC id = %s(%d) on DEST = %d to svc_id = %s(%d) : MDTM Returned Failure",
		    get_svc_names(m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl)),
		    m_MDS_GET_SVC_ID_FROM_SVC_HDL(svc_hdl),
		    m_MDS_GET_VDEST_ID_FROM_SVC_HDL(svc_hdl),
		    get_svc_names(subscr_svc_id), subscr_svc_id);
		m_MDS_LEAVE();
		return NCSCC_RC_FAILURE;
	}

	/* Update subscription handle to subscription table */
	mds_subtn_tbl_update_ref_hdl(svc_hdl, subscr_svc_id, subtn_ref_val);
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_validate_scope

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  Adest Hdl

*********************************************************/
uint32_t mds_mcm_validate_scope(NCSMDS_SCOPE_TYPE local_scope,
				NCSMDS_SCOPE_TYPE remote_scope,
				MDS_DEST remote_adest, MDS_SVC_ID remote_svc_id,
				bool my_pcon)
{

	m_MDS_ENTER();
	switch (local_scope) {
	case NCSMDS_SCOPE_INTRANODE:

		if (m_MDS_GET_NODE_ID_FROM_ADEST(remote_adest) !=
		    m_MDS_GET_NODE_ID_FROM_ADEST(m_MDS_GET_ADEST)) {
			m_MDS_LOG_INFO(
			    "MCM:API: svc_up : Node Scope Mismatch for svc_id = %s(%d)",
			    get_svc_names(remote_svc_id), remote_svc_id);
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		}
		break;

	case NCSMDS_SCOPE_NONE:

		if (remote_scope == NCSMDS_SCOPE_INTRANODE &&
		    m_MDS_GET_NODE_ID_FROM_ADEST(remote_adest) !=
			m_MDS_GET_NODE_ID_FROM_ADEST(m_MDS_GET_ADEST)) {
			m_MDS_LOG_INFO(
			    "MCM:API: svc_up : NONE or CHASSIS Scope Mismatch (remote scope = NODE) for svc_id = %s(%d)",
			    get_svc_names(remote_svc_id), remote_svc_id);
			m_MDS_LEAVE();
			return NCSCC_RC_FAILURE;
		}
		break;

	case NCSMDS_SCOPE_INTRACHASSIS:
		/* This is not supported as of now */
		break;
	default:
		/* Intrapcon is no more supported */
		break;
	}
	m_MDS_LEAVE();
	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_adm_get_adest_hdl

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  Adest Hdl

*********************************************************/
uint32_t mds_adm_get_adest_hdl(void)
{
	m_MDS_ENTER();
	m_MDS_LEAVE();
	return m_ADEST_HDL;
}

/*********************************************************

  Function NAME: ncs_get_internal_vdest_id_from_mds_dest

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  Adest Hdl

*********************************************************/
MDS_VDEST_ID ncs_get_internal_vdest_id_from_mds_dest(MDS_DEST mdsdest)
{
	m_MDS_ENTER();
	m_MDS_LEAVE();
	return (MDS_VDEST_ID)(m_NCS_NODE_ID_FROM_MDS_DEST(mdsdest) == 0
				  ? mdsdest
				  : m_VDEST_ID_FOR_ADEST_ENTRY);
}

/*********************************************************

  Function NAME: mds_mcm_init

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  Adest Hdl

*********************************************************/
uint32_t mds_mcm_init(void)
{
	NCS_PATRICIA_PARAMS pat_tree_params;
	MDS_VDEST_INFO *vdest_for_adest_node;

	/* STEP 1: Initialize MCM-CB. */
	gl_mds_mcm_cb = m_MMGR_ALLOC_MCM_CB;
	memset(gl_mds_mcm_cb, 0, sizeof(MDS_MCM_CB));

	/* VDEST TREE */
	memset(&pat_tree_params, 0, sizeof(NCS_PATRICIA_PARAMS));
	pat_tree_params.key_size = sizeof(MDS_VDEST_ID);
	if (NCSCC_RC_SUCCESS !=
	    ncs_patricia_tree_init(&gl_mds_mcm_cb->vdest_list,
				   &pat_tree_params)) {
		m_MDS_LOG_ERR(
		    "MCM:API: patricia_tree_init: vdest :failure");
		return NCSCC_RC_FAILURE;
	}

	/* ADEST TREE */
	memset(&pat_tree_params, 0, sizeof(NCS_PATRICIA_PARAMS));
	pat_tree_params.key_size = sizeof(MDS_DEST);
	if (NCSCC_RC_SUCCESS !=
	    ncs_patricia_tree_init(&gl_mds_mcm_cb->adest_list,
				   &pat_tree_params)) {
		m_MDS_LOG_ERR(
		    "MCM:API: patricia_tree_init: adest :failure");
		return NCSCC_RC_FAILURE;
	}

	/* SERVICE TREE */
	memset(&pat_tree_params, 0, sizeof(NCS_PATRICIA_PARAMS));
	pat_tree_params.key_size = sizeof(MDS_SVC_HDL);
	if (NCSCC_RC_SUCCESS != ncs_patricia_tree_init(&gl_mds_mcm_cb->svc_list,
						       &pat_tree_params)) {
		m_MDS_LOG_ERR(
		    "MCM:API: patricia_tree_init:service :failure");
		if (NCSCC_RC_SUCCESS !=
		    ncs_patricia_tree_destroy(&gl_mds_mcm_cb->vdest_list)) {
			m_MDS_LOG_ERR(
			    "MCM:API: patricia_tree_destroy: vdest :failure");
		}
		if (NCSCC_RC_SUCCESS !=
		    ncs_patricia_tree_destroy(&gl_mds_mcm_cb->adest_list)) {
			m_MDS_LOG_ERR(
			    "MCM:API: patricia_tree_destroy: adest :failure");
		}
		return NCSCC_RC_FAILURE;
	}

	/* SUBSCRIPTION RESULT TREE */
	memset(&pat_tree_params, 0, sizeof(NCS_PATRICIA_PARAMS));
	pat_tree_params.key_size = sizeof(MDS_SUBSCRIPTION_RESULTS_KEY);
	if (NCSCC_RC_SUCCESS !=
	    ncs_patricia_tree_init(&gl_mds_mcm_cb->subtn_results,
				   &pat_tree_params)) {
		m_MDS_LOG_ERR(
		    "MCM:API: patricia_tree_init: subscription: failure");
		if (NCSCC_RC_SUCCESS !=
		    ncs_patricia_tree_destroy(&gl_mds_mcm_cb->svc_list)) {
			m_MDS_LOG_ERR(
			    "MCM:API: patricia_tree_destroy: service :failure");
		}
		if (NCSCC_RC_SUCCESS !=
		    ncs_patricia_tree_destroy(&gl_mds_mcm_cb->vdest_list)) {
			m_MDS_LOG_ERR(
			    "MCM:API: patricia_tree_destroy: vdest :failure");
		}
		if (NCSCC_RC_SUCCESS !=
		    ncs_patricia_tree_destroy(&gl_mds_mcm_cb->adest_list)) {
			m_MDS_LOG_ERR(
			    "MCM:API: patricia_tree_destroy: adest :failure");
		}
		return NCSCC_RC_FAILURE;
	}

	/* Add VDEST for ADEST entry in tree */
	vdest_for_adest_node = m_MMGR_ALLOC_VDEST_INFO;
	memset(vdest_for_adest_node, 0, sizeof(MDS_VDEST_INFO));

	vdest_for_adest_node->vdest_id = m_VDEST_ID_FOR_ADEST_ENTRY;
	vdest_for_adest_node->policy = NCS_VDEST_TYPE_N_WAY_ROUND_ROBIN;
	vdest_for_adest_node->role = V_DEST_RL_ACTIVE;

	vdest_for_adest_node->node.key_info =
	    (uint8_t *)&vdest_for_adest_node->vdest_id;

	ncs_patricia_tree_add(&gl_mds_mcm_cb->vdest_list,
			      (NCS_PATRICIA_NODE *)vdest_for_adest_node);

	return NCSCC_RC_SUCCESS;
}

/*********************************************************

  Function NAME: mds_mcm_destroy

  DESCRIPTION:

  ARGUMENTS:

  RETURNS:  Adest Hdl

*********************************************************/
uint32_t mds_mcm_destroy(void)
{
	/* cleanup mcm database */
	mds_mcm_cleanup();

	/* destroy all patricia trees */

	/* SUBSCRIPTION RESULT TREE */
	ncs_patricia_tree_destroy(&gl_mds_mcm_cb->subtn_results);

	/* SERVICE TREE */
	ncs_patricia_tree_destroy(&gl_mds_mcm_cb->svc_list);

	/* VDEST TREE */
	ncs_patricia_tree_destroy(&gl_mds_mcm_cb->vdest_list);

	/* ADEST TREE */
	mds_adest_op_list_cleanup();
	ncs_patricia_tree_destroy(&gl_mds_mcm_cb->adest_list);

	/* Free MCM control block */
	m_MMGR_FREE_MCM_CB(gl_mds_mcm_cb);

	gl_mds_mcm_cb = NULL;

	return NCSCC_RC_SUCCESS;
}
