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

/************************************************************************************************
.................................................................................................

.................................................................................................

  DESCRIPTION: This file includes following routines:

   mqd_clm_cluster_track_callback.....Function to get the callback from CLM for
the cluster track
*************************************************************************************************/

#include "base/osaf_time.h"
#include "msg/msgd/mqd.h"
#include "mqd_imm.h"
extern MQDLIB_INFO gl_mqdinfo;

/*****************************************************************************************
 *
 *  Name  : mqd_clm_cluster_track_callback
 *
 *  Description : We will get the callback from CLM for the cluster track
 *
 ******************************************************************************************/
void mqd_clm_cluster_track_callback(
	const SaClmClusterNotificationBufferT_4 *notificationBuffer,
	SaUint32T numberOfMembers,
	SaInvocationT invocation,
	const SaNameT *rootCauseEntity,
	const SaNtfCorrelationIdsT *correlationIds,
	SaClmChangeStepT step,
	SaTimeT timeSupervision,
	SaAisErrorT error)
{
	MQD_CB *pMqd = 0;
	SaClmNodeIdT node_id;
	MQD_ND_DB_NODE *pNdNode = 0;
	uint32_t counter = 0;
	TRACE_ENTER2("cluster change=%d",
		     notificationBuffer->notification[counter].clusterChange);

	if (error != SA_AIS_OK) {
		LOG_ER("mqd_clm_cluster_track_callback error: %i", error);
		goto done;
	}

	/* Get the Controll block */
	pMqd = ncshm_take_hdl(NCS_SERVICE_ID_MQD, gl_mqdinfo.inst_hdl);
	if (!pMqd) {
		LOG_ER("%s:%u: Instance Doesn't Exist", __FILE__, __LINE__);
		return;
	} else {
		for (counter = 0; counter < notificationBuffer->numberOfItems;
		     counter++) {
			node_id = notificationBuffer->notification[counter].
				clusterNode.nodeId;
			if (notificationBuffer->notification[counter]
				.clusterChange == SA_CLM_NODE_LEFT) {
				pNdNode =
				    (MQD_ND_DB_NODE *)ncs_patricia_tree_get(
					&pMqd->node_db, (uint8_t *)&node_id);
				if (pNdNode) {
					if (pMqd->ha_state ==
					    SA_AMF_HA_ACTIVE) {
						mqd_tmr_stop(
						    &pNdNode->info.timer);
						mqd_del_node_down_info(pMqd,
								       node_id);
						mqd_red_db_node_del(pMqd,
								    pNdNode);
					} else {
						pNdNode->info.is_node_down =
						    true;
					}
				} else {
					SaTimeT timeout =
            m_NCS_CONVERT_SATIME_TO_TEN_MILLI_SEC(MQD_ND_EXPIRY_TIME_STANDBY);
					TRACE_2(
					    "%s:%u: CLM Event is coming first for Node down",
					    __FILE__, __LINE__);
					pNdNode = m_MMGR_ALLOC_MQD_ND_DB_NODE;
					if (pNdNode == NULL) {
						LOG_CR(
						    "%s:%u: Failed To Allocate Memory",
						    __FILE__, __LINE__);
						return;
					}
					memset(pNdNode, 0,
					       sizeof(MQD_ND_DB_NODE));
					pNdNode->info.nodeid = node_id;
					pNdNode->info.is_clm_down = true;
					mqd_red_db_node_add(pMqd, pNdNode);
					mqd_tmr_start(&pNdNode->info.timer,
							timeout);
				}
			} else if (notificationBuffer->notification[counter].
					clusterChange == SA_CLM_NODE_JOINED) {
				pNdNode =
				    (MQD_ND_DB_NODE *)ncs_patricia_tree_get(
					&pMqd->node_db, (uint8_t *)&node_id);
				if (pNdNode) {
					mqd_tmr_stop(&pNdNode->info.timer);

					if (pMqd->ha_state ==
						SA_AMF_HA_ACTIVE) {
						mqd_red_db_node_del(pMqd,
								pNdNode);
					}
				}
			}
		}
	}
	ncshm_give_hdl(pMqd->hdl);

done:
	TRACE_LEAVE();
}

static void * _mqd_del_node_down_info(void *arg)
{
	NODE_ID nodeid = *(NODE_ID *) arg;
	SaAisErrorT rc = SA_AIS_OK;
	SaImmOiHandleT immOiHandle = 0;
	MQD_CB *pMqd = ncshm_take_hdl(NCS_SERVICE_ID_MQD, gl_mqdinfo.inst_hdl);

	TRACE_ENTER2("nodeid=%u", nodeid);

	free(arg);

	do {
		MQD_OBJ_NODE *pNode = 0;
		MQD_A2S_MSG msg;
		SaImmOiImplementerNameT implementer_name;
		int retries = 5;
		char i_name[256] = {0};
		SaVersionT imm_version = {'A', 0x02, 0x01};

		if (!pMqd) {
			LOG_ER("%s:%u: Instance Doesn't Exist", __FILE__,
					__LINE__);
			break;
		}

		rc = immutil_saImmOiInitialize_2(&immOiHandle, NULL,
				&imm_version);
		if (rc != SA_AIS_OK) {
			LOG_ER("saImmOiInitialize_2 failed with return value="
					"%d", rc);
			break;
		}

		snprintf(i_name, SA_MAX_NAME_LENGTH, "%s%u", "MsgQueueService",
				nodeid);
		implementer_name = i_name;

		while (retries--) {
			rc = immutil_saImmOiImplementerSet(immOiHandle,
							implementer_name);
			if (rc == SA_AIS_OK)
				break;
			else if (rc == SA_AIS_ERR_EXIST) {
				/*
				 * imm has not yet removed implementer for
				 * remote node. try again.
				 */
				osaf_nanosleep(&kOneSecond);
				continue;
			}
			else {
				LOG_ER("saImmOiImplementerSet failed with "
						"return value=%d", rc);
				break;
			}
		}

		if (rc != SA_AIS_OK) {
			LOG_ER("immutil_saImmOiImplementerSet failed: %i", rc);
			break;
		}

		m_NCS_LOCK(&pMqd->mqd_cb_lock, NCS_LOCK_WRITE);

		pNode = (MQD_OBJ_NODE *)ncs_patricia_tree_getnext(&pMqd->qdb,
				(uint8_t *)0);
		while (pNode) {
			SaNameT name;
			name = pNode->oinfo.name;
			if (m_NCS_NODE_ID_FROM_MDS_DEST(
						pNode->oinfo.info.q.dest) ==
					nodeid) {
				ASAPi_DEREG_INFO dereg;
				memset(&dereg, 0, sizeof(ASAPi_DEREG_INFO));
				dereg.objtype = ASAPi_OBJ_QUEUE;
				dereg.queue = pNode->oinfo.name;

				rc = immutil_saImmOiRtObjectDelete(immOiHandle,
						&dereg.queue);
				if (rc != SA_AIS_OK)
					LOG_ER("Deleting MsgQGrp object %s "
							"FAILED with return "
							"value=%d",
							dereg.queue.value, rc);
				if (mqd_asapi_dereg_hdlr(pMqd, &dereg, NULL) !=
						NCSCC_RC_SUCCESS)
					LOG_ER("mqd_asapi_dereg_hdlr failed");
			}
			pNode = (MQD_OBJ_NODE *)ncs_patricia_tree_getnext(
					&pMqd->qdb, (uint8_t *)&name);
		}

		m_NCS_UNLOCK(&pMqd->mqd_cb_lock, NCS_LOCK_WRITE);

		/* Send an async Update to the standby */
		memset(&msg, 0, sizeof(MQD_A2S_MSG));
		msg.type = MQD_A2S_MSG_TYPE_MQND_TIMER_EXPEVT;
		msg.info.nd_tmr_exp_evt.nodeid = nodeid;

		/* Send async update to the standby for MQD redundancy */
		mqd_a2s_async_update(pMqd, MQD_A2S_MSG_TYPE_MQND_TIMER_EXPEVT,
				(void *)(&msg.info.nd_tmr_exp_evt));

	} while (false);

	if (immOiHandle) {
		rc = immutil_saImmOiFinalize(immOiHandle);
		if (rc != SA_AIS_OK)
			LOG_ER("saImmOiFinalize failed with return value=%d",
					rc);
	}

	if (pMqd)
		ncshm_give_hdl(pMqd->hdl);

	TRACE_LEAVE();
	return 0;
}

void mqd_del_node_down_info(MQD_CB *pMqd, NODE_ID nodeid)
{
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	TRACE_ENTER();

	NODE_ID *arg = malloc(sizeof(NODE_ID));
	*arg = nodeid;

	if (pthread_create(&thread, &attr, _mqd_del_node_down_info, arg) != 0) {
		LOG_CR("pthread_create FAILED: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	pthread_attr_destroy(&attr);
	TRACE_LEAVE();
}
