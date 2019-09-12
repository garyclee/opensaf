#include <thread>
#include <saClm.h>
#include "lck/lckd/gld_clm.h"
#include "base/osaf_time.h"

static void handleClmNodeUpdate(GLSV_GLD_CB &cb, SaClmNodeIdT nodeId,
                                bool isClusterMember) {
  TRACE_ENTER();

  GLSV_GLD_EVT *evt(m_MMGR_ALLOC_GLSV_GLD_EVT);

  if (evt == GLSV_GLD_EVT_NULL) {
    LOG_CR("Event alloc failed: Error %s", strerror(errno));
    assert(false);
  }

  memset(evt, 0, sizeof(GLSV_GLD_EVT));
  evt->gld_cb = &cb;
  evt->evt_type = GLSV_GLD_EVT_GLND_DOWN_CLM;
  evt->info.glnd_clm_info.nodeId = nodeId;

  // Push the event and we are done
  if (m_NCS_IPC_SEND(&cb.mbx, evt, NCS_IPC_PRIORITY_NORMAL) ==
      NCSCC_RC_FAILURE) {
    LOG_ER("IPC send failed");
    gld_evt_destroy(evt);
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

    GLSV_GLD_CB *cb(static_cast<GLSV_GLD_CB *>(m_GLSV_GLD_RETRIEVE_GLD_CB));

    if (!cb) {
      LOG_ER("GLD cb take handle failed");
      break;
    }

    TRACE("number of items: %i", notificationBuffer->numberOfItems);

    for (SaUint32T i(0); i < notificationBuffer->numberOfItems; i++) {
      TRACE("cluster change: %i",
            notificationBuffer->notification[i].clusterChange);
      /*
       * We only care about nodes leaving; lcknd will send an up message when
       * it is operational
       */
      if (notificationBuffer->notification[i].clusterChange ==
          SA_CLM_NODE_LEFT) {
        handleClmNodeUpdate(
            *cb, notificationBuffer->notification[i].clusterNode.nodeId, false);
      }
    }

    m_GLSV_GLD_GIVEUP_GLD_CB;

  } while (false);

  TRACE_LEAVE();
}

SaAisErrorT gld_clm_init(GLSV_GLD_CB *cb) {
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

    rc = saClmClusterTrack_4(cb->clm_hdl, SA_TRACK_CHANGES_ONLY, 0);

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

  if (rc != SA_AIS_OK && cb->clm_hdl) gld_clm_deinit(cb);

  TRACE_LEAVE();
  return rc;
}

SaAisErrorT gld_clm_deinit(GLSV_GLD_CB *cb) {
  SaAisErrorT rc(saClmFinalize(cb->clm_hdl));

  if (rc != SA_AIS_OK) LOG_ER("saClmFinalize failed: %i", rc);

  cb->clm_hdl = 0;

  return rc;
}

static void clm_init_thread(GLSV_GLD_CB *cb)
{
  gld_clm_init(cb);
}

void gld_clm_reinit_bg(GLSV_GLD_CB *cb)
{
  std::thread x(&clm_init_thread, cb);
  x.detach();
}
