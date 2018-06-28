/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2017 The OpenSAF Foundation
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
 * Author(s): Genband
 *
 */
#include <cstring>
#include <vector>
#include <saMsg.h>
#include "base/ncs_edu_pub.h"
#include "base/logtrace.h"
#include "base/ncs_queue.h"
#include "base/ncspatricia.h"
#include "base/ncssysf_tmr.h"
#include "mbc/mbcsv_papi.h"
#include "mds/mds_papi.h"
#include "msg/common/mqsv_def.h"
#include "msg/common/mqsv_asapi.h"
#include "mqd_tmr.h"
#include "mqd_db.h"
#include "mqd_dl_api.h"
#include "mqd_ntf.h"

extern MQDLIB_INFO gl_mqdinfo;

static const int operStateSubId = 1;

static void sendStateChangeNotification(MQD_CB *cb,
                                        const SaNameT& queueGroup,
                                        SaUint16T status) {
  TRACE_ENTER();

  do {
    if (status == SA_MSG_QUEUE_CAPACITY_REACHED)
      status = SA_MSG_QUEUE_GROUP_CAPACITY_REACHED;
    else if (status == SA_MSG_QUEUE_CAPACITY_AVAILABLE)
      status = SA_MSG_QUEUE_GROUP_CAPACITY_AVAILABLE;

    const SaNameT notifyingObject = {
      sizeof("safApp=safMsgService") - 1,
      "safApp=safMsgService"
    };

    SaNtfStateChangeNotificationT ntfStateChange = { 0 };

    SaAisErrorT rc(saNtfStateChangeNotificationAllocate(cb->ntfHandle,
                                                        &ntfStateChange,
                                                        1,
                                                        0,
                                                        0,
                                                        1,
                                                        0));

    if (rc != SA_AIS_OK) {
      LOG_ER("saNtfStateChangeNotificationAllocate " "failed: %i", rc);
      break;
    }

    *ntfStateChange.notificationHeader.eventType = SA_NTF_OBJECT_STATE_CHANGE;

    memcpy(ntfStateChange.notificationHeader.notificationObject,
           &queueGroup,
           sizeof(SaNameT));

    memcpy(ntfStateChange.notificationHeader.notifyingObject,
           &notifyingObject,
           sizeof(SaNameT));

    ntfStateChange.notificationHeader.notificationClassId->vendorId =
      SA_NTF_VENDOR_ID_SAF;
    ntfStateChange.notificationHeader.notificationClassId->majorId =
      SA_SVC_MSG;
    ntfStateChange.notificationHeader.notificationClassId->minorId =
      (status == SA_MSG_QUEUE_GROUP_CAPACITY_REACHED) ? 0x67 : 0x68;

    *ntfStateChange.sourceIndicator = SA_NTF_OBJECT_OPERATION;

    ntfStateChange.changedStates[0].stateId = SA_MSG_DEST_CAPACITY_STATUS;

    ntfStateChange.changedStates[0].oldStatePresent =
      (status == SA_MSG_QUEUE_GROUP_CAPACITY_REACHED) ? SA_FALSE : SA_TRUE;

    if (status == SA_MSG_QUEUE_GROUP_CAPACITY_AVAILABLE) {
      ntfStateChange.changedStates[0].oldState =
        SA_MSG_QUEUE_GROUP_CAPACITY_REACHED;
    }

    ntfStateChange.changedStates[0].newState = status;

    rc = saNtfNotificationSend(ntfStateChange.notificationHandle);

    if (rc != SA_AIS_OK)
      LOG_ER("saNtfNotificationSend failed: %i", rc);

    rc = saNtfNotificationFree(ntfStateChange.notificationHandle);

    if (rc != SA_AIS_OK)
      LOG_ER("saNtfNotificationFree failed: %i", rc);
  } while (false);

  TRACE_LEAVE();
}

static void updateQueueGroup(MQD_CB *cb,
                             const SaNameT& queueName,
                             SaUint16T status) {
  TRACE_ENTER();

  bool member(false), allFull(true);
  typedef std::vector<SaNameT> GroupList;
  GroupList groupList;

  m_NCS_LOCK(&cb->mqd_cb_lock, NCS_LOCK_WRITE);

  // maybe there is a better way to do this?
  for (MQD_OBJ_NODE *objNode(reinterpret_cast<MQD_OBJ_NODE *>
         (ncs_patricia_tree_getnext(&cb->qdb, 0)));
       objNode;
       objNode = reinterpret_cast<MQD_OBJ_NODE *>(ncs_patricia_tree_getnext(
         &cb->qdb,
         reinterpret_cast<uint8_t *>(&objNode->oinfo.name)))) {

    if (objNode->oinfo.type == MQSV_OBJ_QGROUP) {
      NCS_Q_ITR itr = { 0 };

      for (MQD_OBJECT_ELEM *objElem(static_cast<MQD_OBJECT_ELEM *>
             (ncs_queue_get_next(&objNode->oinfo.ilist, &itr)));
           objElem;
           objElem = static_cast<MQD_OBJECT_ELEM *>
             (ncs_queue_get_next(&objNode->oinfo.ilist, &itr))) {
        if (objElem->pObject->type == MQSV_OBJ_QUEUE) {
          if (queueName.length == objElem->pObject->name.length &&
              !memcmp(queueName.value,
                      objElem->pObject->name.value,
                      queueName.length)) {
            member = true;
            if (status == SA_MSG_QUEUE_CAPACITY_REACHED)
              objElem->pObject->capacityReached = true;
            else if (status == SA_MSG_QUEUE_CAPACITY_AVAILABLE)
              objElem->pObject->capacityReached = false;

            groupList.push_back(objNode->oinfo.name);
          } else if (!objElem->pObject->capacityReached) {
            allFull = false;
          }
        }
      }
    }
  }

  m_NCS_UNLOCK(&cb->mqd_cb_lock, NCS_LOCK_WRITE);

  if (member && ((status == SA_MSG_QUEUE_CAPACITY_REACHED && allFull) ||
      (status == SA_MSG_QUEUE_CAPACITY_AVAILABLE && allFull))) {
    for (auto& it : groupList)
      sendStateChangeNotification(cb, it, status);
  } else {
    // don't care about anything else
  }

  TRACE_LEAVE();
}

static bool isMSGStateChange(const SaNtfClassIdT &classId) {
  TRACE_ENTER();
  bool status(false);

  if (classId.vendorId == SA_NTF_VENDOR_ID_SAF &&
      classId.majorId == SA_SVC_MSG) {
    status = true;
  }

  TRACE_LEAVE2("%i", status);
  return status;
}

static void handleMsgObjectStateChangeNotification(
  MQD_CB *cb,
  const SaNtfStateChangeNotificationT& stateChangeNotification) {
  TRACE_ENTER();

  do {
    if (*stateChangeNotification.sourceIndicator != SA_NTF_OBJECT_OPERATION) {
      LOG_ER("expected object operation -- got %i",
             *stateChangeNotification.sourceIndicator);
      break;
    }

    if (stateChangeNotification.numStateChanges != 1) {
      LOG_ER("expected one state change -- got %i",
             stateChangeNotification.numStateChanges);
      break;
    }

    if (stateChangeNotification.changedStates[0].stateId !=
          SA_MSG_DEST_CAPACITY_STATUS) {
      LOG_ER("unknown state id: %i",
             stateChangeNotification.changedStates[0].stateId);
      break;
    }

    // if all the queues for a group are full then send a notification
    updateQueueGroup(
      cb,
      *stateChangeNotification.notificationHeader.notificationObject,
      stateChangeNotification.changedStates[0].newState);
  } while (false);

  TRACE_LEAVE();
}

static void handleStateChangeNotification(
  MQD_CB *cb,
  const SaNtfStateChangeNotificationT& stateChangeNotification) {
  TRACE_ENTER();

  if (stateChangeNotification.notificationHeader.notificationClassId &&
      isMSGStateChange(
          *stateChangeNotification.notificationHeader.notificationClassId)) {
    handleMsgObjectStateChangeNotification(cb, stateChangeNotification);
  } else {
    TRACE("ignoring non-MSG state change notification");
  }

  TRACE_LEAVE();
}

void mqdNtfNotificationCallback(SaNtfSubscriptionIdT subscriptionId,
                                const SaNtfNotificationsT *notification) {
  TRACE_ENTER();
  MQD_CB *cb(static_cast<MQD_CB *>(ncshm_take_hdl(NCS_SERVICE_ID_MQD,
                                                  gl_mqdinfo.inst_hdl)));

  if (!cb) {
    LOG_ER("can't get mqd control block");
  }

  do {
    if (cb->ha_state != SA_AMF_HA_ACTIVE)
      break;

    if (subscriptionId != operStateSubId) {
      TRACE("unknown subscription id received in mqdNtfNotificationCallback: "
            "%d",
            subscriptionId);
      break;
    }

    if (notification->notificationType == SA_NTF_TYPE_STATE_CHANGE) {
      handleStateChangeNotification(
          cb,
          notification->notification.stateChangeNotification);
    } else {
      TRACE("ignoring NTF notification of type: %i",
            notification->notificationType);
    }
  } while (false);

  ncshm_give_hdl(cb->hdl);

  TRACE_LEAVE();
}

SaAisErrorT mqdInitNtfSubscriptions(SaNtfHandleT ntfHandle) {
  TRACE_ENTER();
  SaAisErrorT rc(SA_AIS_OK);

  do {
    SaNtfStateChangeNotificationFilterT filter;

    rc = saNtfStateChangeNotificationFilterAllocate(
      ntfHandle,
      &filter, 0, 0, 0, 0, 0, 0);

    if (rc != SA_AIS_OK) {
      LOG_ER("saNtfAttributeChangeNotificationFilterAllocate"
        " failed: %i",
        rc);
      break;
    }

    SaNtfNotificationTypeFilterHandlesT filterHandles = {
      0, 0, filter.notificationFilterHandle, 0, 0
    };

    rc = saNtfNotificationSubscribe(&filterHandles,
      operStateSubId);

    if (rc != SA_AIS_OK) {
      LOG_ER("saNtfNotificationSubscribe failed: %i", rc);
      break;
    }

    rc = saNtfNotificationFilterFree(filter.notificationFilterHandle);

    if (rc != SA_AIS_OK) {
      LOG_ER("saNtfNotificationFilterFree failed: %i", rc);
      break;
    }
  } while (false);

  TRACE_LEAVE();
  return rc;
}
