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

#include <string.h>
#include <syslog.h>
#include "base/logtrace.h"
#include "base/ncssysf_def.h"
#include "fm_cb.h"
#include "osaf/consensus/consensus.h"
#include "rde/agent/rda_papi.h"

extern uint32_t fm_tmr_start(FM_TMR *tmr, SaTimeT period);
extern void fm_tmr_stop(FM_TMR *tmr);
extern void rda_cb(uint32_t cb_hdl, PCS_RDA_CB_INFO *cb_info,
                   PCSRDA_RETURN_CODE error_code);
/****************************************************************************
 * Name          : fm_rda_init
 *
 * Description   : Initializes RDA interface, Get a role and Register Callback.
 *
 * Arguments     : Pointer to Control Block
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * Notes         : None.
 *****************************************************************************/
uint32_t fm_rda_init(FM_CB *fm_cb) {
  uint32_t rc;
  SaAmfHAStateT ha_state;
  TRACE_ENTER();
  if ((rc = rda_register_callback(0, rda_cb, &ha_state)) != NCSCC_RC_SUCCESS) {
    syslog(LOG_ERR, "rda_register_callback FAILED %u", rc);
    goto done;
  }

  switch (ha_state) {
    case SA_AMF_HA_ACTIVE:
      fm_cb->role = PCS_RDA_ACTIVE;
      break;
    case SA_AMF_HA_STANDBY:
      fm_cb->role = PCS_RDA_STANDBY;
      break;
    case SA_AMF_HA_QUIESCED:
      fm_cb->role = PCS_RDA_QUIESCED;
      break;
    case SA_AMF_HA_QUIESCING:
      fm_cb->role = PCS_RDA_QUIESCING;
      break;
  }
done:
  TRACE_LEAVE();
  return rc;
}

void promote_node(FM_CB *fm_cb) {
  TRACE_ENTER();

  Consensus consensus_service;
  if (consensus_service.PrioritisePartitionSize() == true) {
    // Allow topology events to be processed first. The MDS thread may
    // be processing MDS down events and updating cluster_size concurrently.
    // We need cluster_size to be as accurate as possible, without waiting
    // too long for node down events.
    std::this_thread::sleep_for(
      std::chrono::seconds(
        consensus_service.PrioritisePartitionSizeWaitTime()));
  }

  uint32_t rc;
  rc = consensus_service.PromoteThisNode(true, fm_cb->cluster_size);
  if (rc != SA_AIS_OK && rc != SA_AIS_ERR_EXIST) {
    LOG_ER("Unable to set active controller in consensus service");
    opensaf_quick_reboot("Unable to set active controller "
      "in consensus service");
    return;
  } else if (rc == SA_AIS_ERR_EXIST) {
    // @todo if we don't reboot, we don't seem to recover from this. Can we
    // improve?
    LOG_ER(
        "A controller is already active. We were separated from the "
        "cluster?");
    opensaf_quick_reboot("A controller is already active. We were separated "
                         "from the cluster?");
    return;
  }

  PCS_RDA_REQ rda_req;

  /* set the RDA role to active */
  memset(&rda_req, 0, sizeof(PCS_RDA_REQ));
  rda_req.req_type = PCS_RDA_SET_ROLE;
  rda_req.info.io_role = PCS_RDA_ACTIVE;

  rc = pcs_rda_request(&rda_req);
  if (rc != PCSRDA_RC_SUCCESS) {
    LOG_ER("pcs_rda_request() failed)");
  }
}

/****************************************************************************
 * Name          : fm_rda_set_role
 *
 * Description   : Sends role set request.
 *
 * Arguments     :  Pointer to Control block and role.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE.
 *
 * Notes         : None.
 *****************************************************************************/
uint32_t fm_rda_set_role(FM_CB *fm_cb, PCS_RDA_ROLE role) {
  PCS_RDA_REQ rda_req;
  uint32_t rc;
  TRACE_ENTER();

  osafassert(role == PCS_RDA_ACTIVE);

  Consensus consensus_service;
  if (consensus_service.IsEnabled() == true) {
    // Start supervision timer, make sure we obtain lock within
    // 2* FMS_TAKEOVER_REQUEST_VALID_TIME, otherwise reboot the node.
    // This is needed in case we are in a split network situation
    // the current active will fail-over work running on this node.
    LOG_NO("Starting consensus service supervision: %u s",
           consensus_service.TakeoverValidTime());
    fm_tmr_start(&fm_cb->consensus_service_supervision_tmr,
                 200 * consensus_service.TakeoverValidTime());

    std::thread(&promote_node, fm_cb).detach();
    return NCSCC_RC_SUCCESS;
  }

  /* set the RDA role to active */
  memset(&rda_req, 0, sizeof(PCS_RDA_REQ));
  rda_req.req_type = PCS_RDA_SET_ROLE;
  rda_req.info.io_role = role;

  rc = pcs_rda_request(&rda_req);
  if (rc != PCSRDA_RC_SUCCESS) {
    syslog(LOG_INFO,
           "fm_rda_set_role() Failed: CurrentState: %s, AskedState: %s",
           role_string[fm_cb->role], role_string[role]);
    return NCSCC_RC_FAILURE;
  }

  syslog(LOG_INFO,
         "fm_rda_set_role() Success: CurrentState: %s, AskedState: %s",
         role_string[fm_cb->role], role_string[role]);

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}
