/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
 * (C) Copyright 2017 Ericsson AB - All Rights Reserved.
 * Copyright (C) 2017, Oracle and/or its affiliates. All rights reserved.
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

/*****************************************************************************

  DESCRIPTION:   This file contains routines used for sending alarms and
  notifications.

******************************************************************************/

#include "base/logtrace.h"
#include "amf/amfd/util.h"
#include "amf/amfd/ntf.h"
#include "base/osaf_time.h"
#include <queue>

static uint32_t sendAlarmNotificationAvd(
    AVD_CL_CB* avd_cb, const std::string& ntf_object, SaUint8T* add_text,
    SaUint16T majorId, SaUint16T minorId, uint32_t probableCause,
    uint32_t perceivedSeverity, NCSCONTEXT add_info, bool add_info_is_present);

static uint32_t sendStateChangeNotificationAvd(
    AVD_CL_CB* avd_cb, const std::string& ntf_object, SaUint8T* add_text,
    SaUint16T majorId, SaUint16T minorId, uint32_t sourceIndicator,
    SaUint16T stateId, SaUint16T oldstate, SaUint16T newState,
    NCSCONTEXT add_info, bool additional_info_is_present);

/*****************************************************************************
  Name          :  avd_send_comp_inst_failed_alarm

  Description   :  This function generates a Component Instantiation Failed
alarm.

  Arguments     :  comp_name - Pointer to the Component DN.
                   node_name - Pointer to the DN of node on which the component
                               is hosted.

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_comp_inst_failed_alarm(const std::string& comp_name,
                                     const std::string& node_name) {
  const SaNameTWrapper node(node_name);
  const std::string add_text("Instantiation of Component " + comp_name +
                             " failed");

  TRACE_ENTER();
  sendAlarmNotificationAvd(avd_cb, comp_name, (SaUint8T*)add_text.c_str(),
                           SA_SVC_AMF, SA_AMF_NTFID_COMP_INSTANTIATION_FAILED,
                           SA_NTF_SOFTWARE_ERROR, SA_NTF_SEVERITY_MAJOR,
                           (NCSCONTEXT)(static_cast<const SaNameT*>(node)),
                           true /* add_info is node_name */);
  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_comp_clean_failed_alarm

  Description   :  This function generates a Component Cleanup Failed alarm.

  Arguments     :  comp_name - Pointer to the Component DN
                   node_name - Pointer to the DN of node on which the component
                               is hosted.

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_comp_clean_failed_alarm(const std::string& comp_name,
                                      const std::string& node_name) {
  const std::string add_text("Cleanup of Component " + comp_name + " failed");
  const SaNameTWrapper node(node_name);

  TRACE_ENTER();
  sendAlarmNotificationAvd(avd_cb, comp_name, (SaUint8T*)add_text.c_str(),
                           SA_SVC_AMF, SA_AMF_NTFID_COMP_CLEANUP_FAILED,
                           SA_NTF_SOFTWARE_ERROR, SA_NTF_SEVERITY_MAJOR,
                           (NCSCONTEXT) static_cast<const SaNameT*>(node),
                           true /* add_info is node_name */);
  TRACE_LEAVE();
}
/*****************************************************************************
  Name          :  avd_send_cluster_reset_alarm

  Description   :  This function generates a Cluster Reset alarm as designated
                   component failed and a cluster reset recovery as recommended
                   by the component is being done.

  Arguments     :  comp_name - Pointer to the Component DN
                   node_name - Pointer to the DN of node on which the component
                               is hosted.

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_cluster_reset_alarm(const std::string& comp_name) {
  TRACE_ENTER();
  const std::string add_text("Failure of Component " + comp_name +
                             " triggered cluster reset");
  sendAlarmNotificationAvd(avd_cb, comp_name, (SaUint8T*)add_text.c_str(),
                           SA_SVC_AMF, SA_AMF_NTFID_CLUSTER_RESET,
                           SA_NTF_SOFTWARE_ERROR, SA_NTF_SEVERITY_MAJOR,
                           nullptr, false /* No add_info */);
  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_si_unassigned_alarm

  Description   :  This function generates a Service Instance Unassigned alarm

  Arguments     :  si_name - Pointer to the SI DN

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_si_unassigned_alarm(const std::string& si_name) {
  const std::string add_text("SI designated by " + si_name +
                             " has no current active assignments to any SU");

  TRACE_ENTER();

  sendAlarmNotificationAvd(avd_cb, si_name, (SaUint8T*)add_text.c_str(),
                           SA_SVC_AMF, SA_AMF_NTFID_SI_UNASSIGNED,
                           SA_NTF_SOFTWARE_ERROR, SA_NTF_SEVERITY_MAJOR,
                           nullptr, false /* No add_info */);
  TRACE_LEAVE();
}

/*****************************************************************************


  Description   :  This function generates a Proxy Status of a Component
                   Changed to Unproxied alarm.

  Arguments     :  comp_name - Pointer to the Component DN

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_comp_proxy_status_unproxied_alarm(const std::string& comp_name) {
  const std::string add_text("Component " + comp_name + " become orphan");

  TRACE_ENTER();

  sendAlarmNotificationAvd(avd_cb, comp_name, (SaUint8T*)add_text.c_str(),
                           SA_SVC_AMF, SA_AMF_NTFID_COMP_UNPROXIED,
                           SA_NTF_SOFTWARE_ERROR, SA_NTF_SEVERITY_MAJOR,
                           nullptr, false /* No add_info */);
  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_admin_state_chg_ntf

  Description   :  This function generates a Admin State Change notification

  Arguments     :  name - Pointer to the specific DN
                   minor_id - Notification Class Identifier
                   old_state - Previous Administrative State
                   new_state - Present Administrative State

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_admin_state_chg_ntf(const std::string& name,
                                  SaAmfNotificationMinorIdT minor_id,
                                  SaAmfAdminStateT old_state,
                                  SaAmfAdminStateT new_state) {
  const std::string add_text("Admin state of " + name + " changed");

  TRACE_ENTER();

  sendStateChangeNotificationAvd(
      avd_cb, name, (SaUint8T*)add_text.c_str(), SA_SVC_AMF, minor_id,
      SA_NTF_MANAGEMENT_OPERATION, SA_AMF_ADMIN_STATE, old_state, new_state,
      nullptr, false);

  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_oper_chg_ntf

  Description   :  This function generates a Operational State Change
notification.

  Arguments     :  name - Pointer to the specific DN
                   minor_id - Notification Class Identifier
                   old_state - Previous Operational State
                   new_state - Present Operational State

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_oper_chg_ntf(const std::string& name,
                           SaAmfNotificationMinorIdT minor_id,
                           SaAmfOperationalStateT old_state,
                           SaAmfOperationalStateT new_state,
                           const std::string* maintenanceCampaign) {
  const std::string add_text("Oper state " + name + " changed");
  const SaNameTWrapper mc(maintenanceCampaign ? *maintenanceCampaign : "");

  TRACE_ENTER();

  sendStateChangeNotificationAvd(
      avd_cb, name, (SaUint8T*)add_text.c_str(), SA_SVC_AMF, minor_id,
      SA_NTF_OBJECT_OPERATION, SA_AMF_OP_STATE, old_state, new_state,
      (NCSCONTEXT) static_cast<const SaNameT*>(mc),
      maintenanceCampaign ? !maintenanceCampaign->empty() : false);
  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_gen_su_pres_state_chg_ntf

  Description   :  This function sends a SU Presence State Change notification.

  Arguments     :  su_name - Pointer to the SU DN
                   old_state - Previous Presence State
                   new_state - Present Presence State

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_su_pres_state_chg_ntf(const std::string& su_name,
                                    SaAmfPresenceStateT old_state,
                                    SaAmfPresenceStateT new_state) {
  const std::string add_text("Presence state of SU " + su_name + " changed");

  TRACE_ENTER();

  sendStateChangeNotificationAvd(avd_cb, su_name, (SaUint8T*)add_text.c_str(),
                                 SA_SVC_AMF, SA_AMF_NTFID_SU_PRESENCE_STATE,
                                 SA_NTF_OBJECT_OPERATION, SA_AMF_PRESENCE_STATE,
                                 old_state, new_state, nullptr, false);

  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_su_ha_state_chg_ntf

  Description   :  This function sends a SU HA State Change notification

  Arguments     :  su_name - Pointer to the SU DN
                   si_name - Pointer to the SI DN
                   old_state - Previous HA State
                   new_state - Present HA State

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_su_ha_state_chg_ntf(const std::string& su_name,
                                  const std::string& si_name,
                                  SaAmfHAStateT old_state,
                                  SaAmfHAStateT new_state) {
  const std::string add_text("The HA state of SI " + si_name +
                             " assigned to SU " + su_name + " changed");
  const SaNameTWrapper si(si_name);

  TRACE_ENTER();

  sendStateChangeNotificationAvd(
      avd_cb, su_name, (SaUint8T*)add_text.c_str(), SA_SVC_AMF,
      SA_AMF_NTFID_SU_SI_HA_STATE, SA_NTF_OBJECT_OPERATION, SA_AMF_HA_STATE,
      old_state, new_state, (NCSCONTEXT) static_cast<const SaNameT*>(si),
      true /* Si_name */);

  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_si_ha_readiness_state_chg_ntf

  Description   :  This function sends a SU HA Readiness State Change
notification

  Arguments     :  su_name - Pointer to the SU DN
                   si_name - Pointer to the SI DN
                   old_state - Previous HA Readiness State
                   new_state - Present HA Readiness State

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_su_ha_readiness_state_chg_ntf(const std::string& su_name,
                                            const std::string& si_name,
                                            SaAmfHAReadinessStateT old_state,
                                            SaAmfHAReadinessStateT new_state) {
  const std::string add_text("The HA readiness state of SI " + si_name +
                             " assigned to SU " + su_name + " changed");

  TRACE_ENTER();

  const SaNameTWrapper si(si_name);
  sendStateChangeNotificationAvd(
      avd_cb, su_name, (SaUint8T*)add_text.c_str(), SA_SVC_AMF,
      SA_AMF_NTFID_SU_SI_HA_READINESS_STATE, SA_NTF_OBJECT_OPERATION,
      SA_AMF_HA_READINESS_STATE, old_state, new_state,
      (NCSCONTEXT) static_cast<const SaNameT*>(si), true /* Si_name */);
  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_si_assigned_ntf

  Description   :  This function sends a SI Assignment State Change notification

  Arguments     :  si_name - Pointer to the SI DN
                   old_state - Previous Assignment State
                   new_state - Present Assignment State

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_si_assigned_ntf(const std::string& si_name,
                              SaAmfAssignmentStateT old_state,
                              SaAmfAssignmentStateT new_state) {
  const std::string add_text("The Assignment state of SI " + si_name +
                             " changed");

  TRACE_ENTER();

  sendStateChangeNotificationAvd(
      avd_cb, si_name, (SaUint8T*)add_text.c_str(), SA_SVC_AMF,
      SA_AMF_NTFID_SI_ASSIGNMENT_STATE, SA_NTF_OBJECT_OPERATION,
      SA_AMF_ASSIGNMENT_STATE, old_state, new_state, nullptr, false);

  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_send_comp_proxy_status_proxied_ntf

  Description   :  This function sends a 'Proxy Status of a Component Changed to
                   Proxied' notification when a component once again is proxied.

  Arguments     :  comp_name - Pointer to the Component DN
                   old_status - Previous  proxy status
                   new_status - Present proxy status

  Return Values :

  Notes         :
*****************************************************************************/
void avd_send_comp_proxy_status_proxied_ntf(const std::string& comp_name,
                                            SaAmfProxyStatusT old_status,
                                            SaAmfProxyStatusT new_status) {
  const std::string add_text("Component " + comp_name + " is now proxied");

  TRACE_ENTER();

  sendStateChangeNotificationAvd(avd_cb, comp_name, (SaUint8T*)add_text.c_str(),
                                 SA_SVC_AMF, SA_AMF_NTFID_COMP_PROXY_STATUS,
                                 SA_NTF_OBJECT_OPERATION, SA_AMF_PROXY_STATUS,
                                 old_status, new_status, nullptr, false);
  TRACE_LEAVE();
}

/*****************************************************************************
  Name          :  avd_alarm_clear

  Description   :  This function sends a alarm clear for a specific previous
                   sent alarm.

  Arguments     :  name - Pointer to the specific DN
                   minor_id - Notification Class Identifier
                   probableCause - same as cause for original alarm

  Return Values :

  Notes         :
*****************************************************************************/
void avd_alarm_clear(const std::string& name, SaUint16T minorId,
                     uint32_t probableCause) {
  const std::string add_text("Previous raised alarm of " + name +
                             " is now cleared");

  TRACE_ENTER();

  sendAlarmNotificationAvd(avd_cb, name, (SaUint8T*)add_text.c_str(),
                           SA_SVC_AMF, minorId, probableCause,
                           SA_NTF_SEVERITY_CLEARED, nullptr, false);
  TRACE_LEAVE();
}

static SaAisErrorT fill_ntf_header_part_avd(
    SaNtfNotificationHeaderT* notificationHeader, SaNtfEventTypeT eventType,
    const std::string& comp_name, SaUint8T* add_text, SaUint16T majorId,
    SaUint16T minorId, SaInt8T* avd_name, NCSCONTEXT add_info,
    bool additional_info_is_present,
    SaNtfNotificationHandleT notificationHandle) {
  *notificationHeader->eventType = eventType;
  *notificationHeader->eventTime = (SaTimeT)SA_TIME_UNKNOWN;

  osaf_extended_name_alloc(comp_name.c_str(),
                           notificationHeader->notificationObject);
  osaf_extended_name_alloc(avd_name, notificationHeader->notifyingObject);

  notificationHeader->notificationClassId->vendorId = SA_NTF_VENDOR_ID_SAF;
  notificationHeader->notificationClassId->majorId = majorId;
  notificationHeader->notificationClassId->minorId = minorId;

  (void)strcpy(notificationHeader->additionalText, (SaInt8T*)add_text);

  /* Fill the additional info if present */
  if (additional_info_is_present == true) {
    if (minorId == SA_AMF_NTFID_ERROR_REPORT) {
      SaAmfRecommendedRecoveryT* recovery =
          (SaAmfRecommendedRecoveryT*)(add_info);
      notificationHeader->additionalInfo[0].infoId = SA_AMF_AI_APPLIED_RECOVERY;
      notificationHeader->additionalInfo[0].infoType = SA_NTF_VALUE_UINT64;
      notificationHeader->additionalInfo[0].infoValue.uint64Val = *recovery;
    } else {
      SaStringT dest_ptr;
      SaAisErrorT ret;
      SaNameT* name = (SaNameT*)(add_info);

      if ((minorId == SA_AMF_NTFID_COMP_INSTANTIATION_FAILED) ||
          (minorId == SA_AMF_NTFID_COMP_CLEANUP_FAILED)) {
        /* node_name */
        notificationHeader->additionalInfo[0].infoId = SA_AMF_NODE_NAME;
        notificationHeader->additionalInfo[0].infoType = SA_NTF_VALUE_LDAP_NAME;

      } else if ((minorId == SA_AMF_NTFID_SU_SI_HA_STATE) ||
                 (minorId == SA_AMF_NTFID_SU_SI_HA_READINESS_STATE)) {
        /* si_name */
        notificationHeader->additionalInfo[0].infoId = SA_AMF_SI_NAME;
        notificationHeader->additionalInfo[0].infoType = SA_NTF_VALUE_LDAP_NAME;

      } else if (minorId == SA_AMF_NTFID_SU_OP_STATE) {
        /* maintenance campaign */
        notificationHeader->additionalInfo[0].infoId =
            SA_AMF_MAINTENANCE_CAMPAIGN_DN;
        notificationHeader->additionalInfo[0].infoType = SA_NTF_VALUE_LDAP_NAME;
      }

      ret = saNtfPtrValAllocate(
          notificationHandle, sizeof(SaNameT) + 1, (void**)&dest_ptr,
          &(notificationHeader->additionalInfo[0].infoValue));

      if (ret != SA_AIS_OK) {
        LOG_ER("%s: saNtfPtrValAllocate Failed (%u)", __FUNCTION__, ret);
        return static_cast<SaAisErrorT>(NCSCC_RC_FAILURE);
      }

      memcpy(dest_ptr, name, sizeof(SaNameT));
    }
  }
  return SA_AIS_OK;
}

/*
 * @brief      Tries to send notification if not sent already. If not successful
 * then does not try to Free() it. Same function will be called from job queue
 *             to complete remaining task.
 * @param[in]  ptr to NtfSend
 * @return     SaAisErrorT.
 * TODO:       Make it a member function of NtfSend() when all notification
 * related handling is moved in separate thread.
 */
SaAisErrorT avd_try_send_notification(NtfSend* job) {
  TRACE_ENTER2("Ntf Type:%x, sent status:%u", job->myntf.notificationType,
               job->already_sent);

  SaNtfNotificationsT* myntf = &job->myntf;
  SaAisErrorT rc = SA_AIS_OK;
  SaNtfNotificationHeaderT* header = nullptr;
  SaNtfNotificationHandleT* notificationHandle = nullptr;

  if (myntf->notificationType == SA_NTF_TYPE_STATE_CHANGE) {
    header = &myntf->notification.stateChangeNotification.notificationHeader;
    notificationHandle =
        &myntf->notification.stateChangeNotification.notificationHandle;
  } else if (myntf->notificationType == SA_NTF_TYPE_ALARM) {
    header = &myntf->notification.alarmNotification.notificationHeader;
    notificationHandle =
        &myntf->notification.alarmNotification.notificationHandle;
  }

  osafassert(notificationHandle != nullptr);
  // Try to send the notification if not sent.
  if (job->already_sent == false) {
    rc = saNtfNotificationSend(*notificationHandle);
    if ((rc == SA_AIS_ERR_TRY_AGAIN) || (rc == SA_AIS_ERR_TIMEOUT)) {
      TRACE("Notification Send unsuccesful TRY_AGAIN or TIMEOUT rc:%u", rc);
      goto done;
    } else {
      // To remember only Free is pending as NotificationFree() may hit with TRY
      // AGAIN.
      job->already_sent = true;
    }
    osaf_extended_name_free(header->notificationObject);
    osaf_extended_name_free(header->notifyingObject);
  }

  rc = saNtfNotificationFree(*notificationHandle);
  if ((rc == SA_AIS_ERR_TRY_AGAIN) || (rc == SA_AIS_ERR_TIMEOUT)) {
    TRACE("Notification Free unsuccesful TRY_AGAIN or TIMEOUT rc:%u", rc);
  } else {
    *notificationHandle = 0;
  }

done:
  TRACE_LEAVE();
  return rc;
}
uint32_t sendAlarmNotificationAvd(
    AVD_CL_CB* avd_cb, const std::string& ntf_object, SaUint8T* add_text,
    SaUint16T majorId, SaUint16T minorId, uint32_t probableCause,
    uint32_t perceivedSeverity, NCSCONTEXT add_info, bool add_info_is_present) {
  uint32_t status = NCSCC_RC_FAILURE;
  SaUint16T add_info_items = 0;
  SaUint64T allocation_size = 0;

  if (!avd_cb->active_services_exist) {
    // TODO #3051
    LOG_ER("Alarm lost for %s", ntf_object.c_str());
    return status;
  }
  if (avd_cb->ntfHandle == 0) {
    LOG_ER(
        "NTF handle has not been initialized, alarm notification "
        "for (%s) will be lost",
        ntf_object.c_str());
    return status;
  }
  NtfSend* job = new NtfSend{};

  if (add_info_is_present) {
    add_info_items = 1;
    allocation_size = SA_NTF_ALLOC_SYSTEM_LIMIT;
  }

  status = saNtfAlarmNotificationAllocate(
      avd_cb->ntfHandle, &job->myntf.notification.alarmNotification,
      /* numCorrelatedNotifications */
      0,
      /* lengthAdditionalText */
      strlen((char*)add_text) + 1,
      /* numAdditionalInfo */
      add_info_items,
      /* numSpecificProblems */
      0,
      /* numMonitoredAttributes */
      0,
      /* numProposedRepairActions */
      0,
      /*variableDataSize */
      allocation_size);

  if (status != SA_AIS_OK) {
    LOG_ER("%s: saNtfAlarmNotificationAllocate Failed (%u)", __FUNCTION__,
           status);
    delete job;
    return NCSCC_RC_FAILURE;
  }

  status = fill_ntf_header_part_avd(
      &job->myntf.notification.alarmNotification.notificationHeader,
      SA_NTF_ALARM_PROCESSING, ntf_object, add_text, majorId, minorId,
      const_cast<SaInt8T*>(AMF_NTF_SENDER), add_info, add_info_is_present,
      job->myntf.notification.alarmNotification.notificationHandle);

  if (status != SA_AIS_OK) {
    LOG_ER("%s: fill_ntf_header_part_avd Failed (%u)", __FUNCTION__, status);
    saNtfNotificationFree(
        job->myntf.notification.alarmNotification.notificationHandle);
    delete job;
    return NCSCC_RC_FAILURE;
  }

  *(job->myntf.notification.alarmNotification.probableCause) =
      static_cast<SaNtfProbableCauseT>(probableCause);
  *(job->myntf.notification.alarmNotification.perceivedSeverity) =
      static_cast<SaNtfSeverityT>(perceivedSeverity);

  job->myntf.notificationType = SA_NTF_TYPE_ALARM;

  // Give atleast one try for of Send() and Free() API.
  Fifo::queue(job);

  return status;
}

static uint32_t sendStateChangeNotificationAvd(
    AVD_CL_CB* avd_cb, const std::string& ntf_object, SaUint8T* add_text,
    SaUint16T majorId, SaUint16T minorId, uint32_t sourceIndicator,
    SaUint16T stateId, SaUint16T oldstate, SaUint16T newState,
    NCSCONTEXT add_info, bool additional_info_is_present) {
  uint32_t status = NCSCC_RC_FAILURE;
  SaUint16T add_info_items = 0;
  SaUint64T allocation_size = 0;
  SaUint16T num_of_changedStates = 1;

  if (!avd_cb->active_services_exist) {
    // TODO #3051
    LOG_WA("State change notification lost for '%s'", ntf_object.c_str());
    return status;
  }

  if (avd_cb->ntfHandle == 0) {
    LOG_WA(
        "NTF handle has not been initialized, state change notification "
        "for (%s) will be lost",
        ntf_object.c_str());
    return status;
  }

  if (additional_info_is_present == true) {
    add_info_items = 1;
    allocation_size = SA_NTF_ALLOC_SYSTEM_LIMIT;
  } else {
    add_info_items = 0;
    allocation_size = 0;
  }

  if (stateId == STATE_ID_NA) {
    num_of_changedStates = 0;
  }
  NtfSend* job = new NtfSend{};
  status = saNtfStateChangeNotificationAllocate(
      avd_cb->ntfHandle, /* handle to Notification Service instance */
      &job->myntf.notification.stateChangeNotification,
      /* number of correlated notifications */
      0,
      /* length of additional text */
      strlen((char*)add_text) + 1,
      /* number of additional info items */
      add_info_items,
      /* number of state changes */
      num_of_changedStates,
      /* use default allocation size */
      allocation_size);

  if (status != SA_AIS_OK) {
    LOG_ER("%s: saNtfStateChangeNotificationAllocate Failed (%u)", __FUNCTION__,
           status);
    delete job;
    return NCSCC_RC_FAILURE;
  }

  status = fill_ntf_header_part_avd(
      &job->myntf.notification.stateChangeNotification.notificationHeader,
      SA_NTF_OBJECT_STATE_CHANGE, ntf_object, add_text, majorId, minorId,
      const_cast<SaInt8T*>(AMF_NTF_SENDER), add_info,
      additional_info_is_present,
      job->myntf.notification.stateChangeNotification.notificationHandle);

  if (status != SA_AIS_OK) {
    LOG_ER("%s: fill_ntf_header_part_avd Failed (%u)", __FUNCTION__, status);
    saNtfNotificationFree(
        job->myntf.notification.stateChangeNotification.notificationHandle);
    delete job;
    return NCSCC_RC_FAILURE;
  }

  *(job->myntf.notification.stateChangeNotification.sourceIndicator) =
      static_cast<SaNtfSourceIndicatorT>(sourceIndicator);

  if (num_of_changedStates == 1) {
    job->myntf.notification.stateChangeNotification.changedStates->stateId =
        stateId;
    job->myntf.notification.stateChangeNotification.changedStates
        ->oldStatePresent = SA_TRUE;
    job->myntf.notification.stateChangeNotification.changedStates->oldState =
        oldstate;
    job->myntf.notification.stateChangeNotification.changedStates->newState =
        newState;
  }
  job->myntf.notificationType = SA_NTF_TYPE_STATE_CHANGE;

  // Give atleast one try for of Send() and Free() API.
  Fifo::queue(job);

  return status;
}

void avd_send_error_report_ntf(const std::string& name,
                               SaAmfRecommendedRecoveryT recovery) {
  TRACE_ENTER();
  std::string add_text;
  SaAmfNotificationMinorIdT minorid;
  bool additional_info_is_present;

  if ((recovery >= SA_AMF_NO_RECOMMENDATION) &&
      (recovery <= SA_AMF_CONTAINER_RESTART)) {
    add_text = "Error reported on " + name + " with recovery " +
               amf_recovery[recovery];
    minorid = SA_AMF_NTFID_ERROR_REPORT;
    additional_info_is_present = true;
  } else {
    add_text = "Error reported on " + name + " is now cleared";
    minorid = SA_AMF_NTFID_ERROR_CLEAR;
    additional_info_is_present = false;
  }

  sendStateChangeNotificationAvd(
      avd_cb, name, (SaUint8T*)add_text.c_str(), SA_SVC_AMF, minorid,
      SA_NTF_UNKNOWN_OPERATION, STATE_ID_NA, OLD_STATE_NA, NEW_STATE_NA,
      (NCSCONTEXT)&recovery, additional_info_is_present);

  TRACE_LEAVE();
}

SaAisErrorT avd_ntf_init(AVD_CL_CB* cb) {
  SaAisErrorT error = SA_AIS_OK;
  SaNtfHandleT ntf_handle;
  TRACE_ENTER();

  // reset handle
  cb->ntfHandle = 0;

  /*
   * TODO: to be re-factored as CLM initialization thread
   */
  for (;;) {
    SaVersionT ntfVersion = {'A', 0x01, 0x01};

    error = saNtfInitialize(&ntf_handle, NULL, &ntfVersion);
    if (error == SA_AIS_ERR_TRY_AGAIN || error == SA_AIS_ERR_TIMEOUT ||
        error == SA_AIS_ERR_UNAVAILABLE) {
      if (error != SA_AIS_ERR_TRY_AGAIN) {
        LOG_WA("saNtfInitialize returned %u", (unsigned)error);
      }
      osaf_nanosleep(&kHundredMilliseconds);
      continue;
    }
    if (error == SA_AIS_OK) {
      break;
    } else {
      LOG_ER("Failed to Initialize with NTF: %u", error);
      goto done;
    }
  }
  cb->ntfHandle = ntf_handle;
  TRACE("Successfully initialized NTF");

done:
  TRACE_LEAVE();
  return error;
}

static void* avd_ntf_init_thread(void* arg) {
  TRACE_ENTER();
  AVD_CL_CB* cb = static_cast<AVD_CL_CB*>(arg);

  if (avd_ntf_init(cb) != SA_AIS_OK) {
    LOG_ER("avd_clm_init FAILED");
    goto done;
  }

done:
  TRACE_LEAVE();
  return nullptr;
}

SaAisErrorT avd_start_ntf_init_bg(void) {
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  if (pthread_create(&thread, &attr, avd_ntf_init_thread, avd_cb) != 0) {
    LOG_ER("pthread_create FAILED: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  pthread_attr_destroy(&attr);

  return SA_AIS_OK;
}

AvdJobDequeueResultT NtfSend::exec(AVD_CL_CB* cb) {
  AvdJobDequeueResultT res;
  TRACE_ENTER2("Ntf Type:%x, sent status:%u", myntf.notificationType,
               already_sent);

  SaAisErrorT rc = avd_try_send_notification(this);
  if (rc == SA_AIS_OK) {
    delete Fifo::dequeue();
    res = JOB_EXECUTED;
  } else if (rc == SA_AIS_ERR_TRY_AGAIN) {
    TRACE("TRY-AGAIN");
    res = JOB_ETRYAGAIN;
  } else if (rc == SA_AIS_ERR_TIMEOUT) {
    TRACE("TIMEOUT");
    res = JOB_ETRYAGAIN;
  } else {
    delete Fifo::dequeue();
    LOG_ER("%s: Notification Send FAILED %u", __FUNCTION__, rc);
    res = JOB_ERR;
  }

  TRACE_LEAVE();
  return res;
}

NtfSend::~NtfSend() {
  SaAisErrorT rc = SA_AIS_OK;
  if (myntf.notificationType == SA_NTF_TYPE_STATE_CHANGE) {
    if (myntf.notification.stateChangeNotification.notificationHandle != 0)
      rc = saNtfNotificationFree(
          myntf.notification.stateChangeNotification.notificationHandle);
  } else if (myntf.notificationType == SA_NTF_TYPE_ALARM) {
    if (myntf.notification.alarmNotification.notificationHandle != 0)
      rc = saNtfNotificationFree(
          myntf.notification.alarmNotification.notificationHandle);
  }
  if (rc != SA_AIS_OK) {
    TRACE("Notification Free failed rc:%u", rc);
  }
}
