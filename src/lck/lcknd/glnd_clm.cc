#include <thread>
#include <saClm.h>
#include "lck/lcknd/glnd_clm.h"
#include "base/osaf_time.h"

static void handleClmNodeUpdate(GLND_CB &cb, bool isClusterMember) {
  TRACE_ENTER();
  cb.isClusterMember = isClusterMember;

  GLSV_GLA_EVT gla_evt;

  gla_evt.error = SA_AIS_OK;
  gla_evt.type = GLSV_GLA_CLM_EVT;
  gla_evt.info.gla_clm_info.isClusterMember = isClusterMember;

  if (!isClusterMember) {
    // send replies to outstanding callbacks first, then notify all the agents
    glnd_resource_req_node_down(&cb);
  }

  for (GLND_AGENT_INFO *agentInfo(glnd_agent_node_find_next(&cb, 0)); agentInfo;
       agentInfo = glnd_agent_node_find_next(&cb, agentInfo->agent_mds_id)) {
    if (!isClusterMember) {
      // behave internally as if all clients went down
      for (GLND_CLIENT_INFO *clientInfo(
               glnd_client_node_find_next(&cb, 0, agentInfo->agent_mds_id));
           clientInfo;
           clientInfo = glnd_client_node_find_next(
               &cb, clientInfo->app_handle_id, agentInfo->agent_mds_id)) {
        glnd_client_node_down(&cb, clientInfo);
        glnd_client_node_del(&cb, clientInfo);
      }
    }

    glnd_mds_msg_send_gla(&cb, &gla_evt, agentInfo->agent_mds_id);
  }

  TRACE_LEAVE();
}

static void clusterTrackCallback(
    const SaClmClusterNotificationBufferT_4 *notificationBuffer,
    SaUint32T numberOfMembers, SaInvocationT invocation,
    const SaNameT *rootCauseEntity, const SaNtfCorrelationIdsT *correlationIds,
    SaClmChangeStepT step, SaTimeT timeSupervision, SaAisErrorT error) {
  TRACE_ENTER();

  do {
    if (error != SA_AIS_OK) {
      LOG_ER("clusterTrackCallback sent failure: %i", error);
      break;
    }

    GLND_CB *cb(static_cast<GLND_CB *>(m_GLND_TAKE_GLND_CB));

    if (!cb) {
      LOG_ER("GLND cb take handle failed");
      break;
    }

    TRACE("number of items: %i", notificationBuffer->numberOfItems);

    for (SaUint32T i(0); i < notificationBuffer->numberOfItems; i++) {
      TRACE("cluster change: %i",
            notificationBuffer->notification[i].clusterChange);
      if (notificationBuffer->notification[i].clusterChange ==
          SA_CLM_NODE_LEFT) {
        handleClmNodeUpdate(*cb, false);
      } else if (notificationBuffer->notification[i].clusterChange ==
                     SA_CLM_NODE_JOINED ||
                 notificationBuffer->notification[i].clusterChange ==
                     SA_CLM_NODE_NO_CHANGE) {
        handleClmNodeUpdate(*cb, true);
      }
    }

    m_GLND_GIVEUP_GLND_CB;

  } while (false);

  TRACE_LEAVE();
}

SaAisErrorT glnd_clm_init(GLND_CB *cb) {
  TRACE_ENTER();

  SaAisErrorT rc(SA_AIS_OK);

  do {
    SaClmCallbacksT_4 callbacks = {0, clusterTrackCallback};

    SaVersionT version = {'B', 4, 0};

    while (true) {
      rc = saClmInitialize_4(&cb->clm_hdl, &callbacks, &version);

      if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_UNAVAILABLE) {
        osaf_nanosleep(&kHundredMilliseconds);
        continue;
      } else if (rc != SA_AIS_OK) {
        LOG_ER("saClmInitialize_4 failed: %i", rc);
        break;
      } else {
        break;
      }
    }

    if (rc != SA_AIS_OK)
      break;

    rc = saClmClusterTrack_4(
        cb->clm_hdl, SA_TRACK_CURRENT | SA_TRACK_CHANGES | SA_TRACK_LOCAL, 0);

    if (rc != SA_AIS_OK) {
      LOG_ER("saClmClusterTrack failed: %i", rc);
      break;
    }

    rc = saClmSelectionObjectGet(cb->clm_hdl, &cb->clm_sel_obj);
    if (rc != SA_AIS_OK) {
      LOG_ER("saClmSelectionObjectGet failed: %i", rc);
      break;
    }
  } while (false);

  if (rc != SA_AIS_OK && cb->clm_hdl) glnd_clm_deinit(cb);

  TRACE_LEAVE();
  return rc;
}

SaAisErrorT glnd_clm_deinit(GLND_CB *cb) {
  SaAisErrorT rc(saClmFinalize(cb->clm_hdl));

  if (rc != SA_AIS_OK) LOG_ER("saClmFinalize failed: %i", rc);

  cb->clm_hdl = 0;

  return rc;
}

static void clm_reinit_thread(GLND_CB *cb) {
  glnd_clm_init(cb);
}

void glnd_clm_reinit_bg(GLND_CB *cb) {
  std::thread x(&clm_reinit_thread, cb);
  x.detach();
}
