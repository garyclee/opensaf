/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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
#include <stdio.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <saNtf.h>
#include "osaf/apitest/util.h"
#include "ntf/apitest/sa_error.h"
#include "ntf/apitest/tet_ntf.h"
#include "ntf/apitest/tet_ntf_common.h"
#include "ntf/apitest/ntf_api_with_try_again.h"

#define CALLBACK_USED 1
#define TST_TAG_ND "\nTAG_ND\n"  // Tag for take SC nodes down
#define TST_TAG_NU "\nTAG_NU\n"  // Tag for start SC nodes
#define TST_TAG_STB_REBOOT "\nTAG_STB_REBOOT\n"  // Tag for reboot STANDBY SC
#define TST_TAG_SWITCHOVER "\nTAG_SWITCHOVER\n"  // Tag for switch over
SaNtfIdentifierT last_not_id = SA_NTF_IDENTIFIER_UNUSED;
int verbose;
int gl_tag_mode;
int gl_prompt_mode;
bool gl_suspending;

void assertvalue_impl(__const char *__assertion, __const char *__file,
        unsigned int __line, __const char *__function) {
  fprintf(stderr, "assert failed in %s at %u, %s(): %s\n", __file, __line,
    __function, __assertion);
}

void rc_assert_impl(const char *file, unsigned int line, int rc, int expected) {
  if (rc != expected) {
    fprintf(stderr,
      "error: in %s at %u: %d, expected %d - exiting\n", file,
      line, rc, expected);
    exit(1);
  }
}

static void sigusr2_handler(int sig) {
  if (gl_suspending)
    gl_suspending = false;
}

void install_sigusr2() {
  signal(SIGUSR2, sigusr2_handler);
}

/*
 * Loop of reading key press, stop if 'n+Enter'. Sleep 1 between reading key
 */
static void *nonblk_io_getchar(void *arg) {
  while (gl_suspending) {
    int c = getchar();
    if (c == 'n')
      gl_suspending = false;
    else
      sleep(1);
  }
  pthread_exit(nullptr);
}

/*
 * Wait for controllers (SCs):
 * - Both SCs UP(wished_scs_state=1)
 * - Both SCs DOWN(wished_scs_state=2)
 * - Standby SC REBOOTs(wished_scs_state=3)
 * - SC switch over(wished_scs_state=4)
 * Press 'n' or send USR2 to ntftest to continue the test
 */
void wait_controllers(int wished_scs_state) {
  int i = 0;
  pthread_t thread_id;
  gl_suspending = true;
  /* print slogan */
  if (wished_scs_state == 1) {
    fprintf_p(
      stdout,
      "\nNext: manually START both SCs(in UML env. ./opensaf nodestart <1|2>)");
    if (gl_tag_mode == 1) {
      fprintf_t(stdout, TST_TAG_NU);
    }
  } else if (wished_scs_state == 2) {
    fprintf_p(
      stdout,
      "\nNext: manually STOP both SCs(in UML env. ./opensaf nodestop <1|2>)");
    if (gl_tag_mode == 1) {
      fprintf_t(stdout, TST_TAG_ND);
    }
  } else if (wished_scs_state == 3) {
    fprintf_p(stdout, "\nNext: manually REBOOT standby SC");
    if (gl_tag_mode == 1) {
      fprintf_t(stdout, TST_TAG_STB_REBOOT);
    }
  } else if (wished_scs_state == 4) {
    fprintf_p(stdout, "\nNext: manually switch over("
                      "amf-adm si-swap safSi=SC-2N,safApp=OpenSAF)");
    if (gl_tag_mode == 1) {
      fprintf_t(stdout, TST_TAG_SWITCHOVER);
    }
  } else {
    fprintf_p(stderr, "wrong value wished_scs_state:%d\n",
      wished_scs_state);
    exit(EXIT_FAILURE);
  }

  /* start non-blocking io thread */
  if (pthread_create(&thread_id, NULL, nonblk_io_getchar, NULL) != 0) {
    fprintf_p(stderr, "%d, %s; pthread_create FAILED: %s\n",
      __LINE__, __FUNCTION__, strerror(errno));
    exit(EXIT_FAILURE);
  }
  fprintf_p(stdout, "\nThen press 'n' and 'Enter' (or 'pkill -USR2 ntftest') "
                    "to continue. Waiting ...\n");
  while (gl_suspending) {
    i++;
    i = i % 20;
    i > 0 ? fprintf_p(stdout, ".") : fprintf_p(stdout, "\n");
    fflush(stdout);
    sleep(1);
  }
}

extern SaNtfIdentifierT get_ntf_id(const SaNtfNotificationsT *notif) {
  switch (notif->notificationType) {
  case SA_NTF_TYPE_ALARM:
    return *notif->notification.alarmNotification.notificationHeader
          .notificationId;
    break;
  case SA_NTF_TYPE_OBJECT_CREATE_DELETE:
    return *notif->notification.objectCreateDeleteNotification
          .notificationHeader.notificationId;
    break;
  case SA_NTF_TYPE_ATTRIBUTE_CHANGE:
    return *notif->notification.attributeChangeNotification
          .notificationHeader.notificationId;
    break;
  case SA_NTF_TYPE_STATE_CHANGE:
    return *notif->notification.stateChangeNotification
          .notificationHeader.notificationId;
    break;
  case SA_NTF_TYPE_SECURITY_ALARM:
    return *notif->notification.securityAlarmNotification
          .notificationHeader.notificationId;
    break;
  default:
    assert(0);
  }
}

void poll_until_received(SaNtfHandleT ntfHandle, SaNtfIdentifierT wanted_id) {
  struct pollfd fds[1];

  SaSelectionObjectT so;
  safassert(saNtfSelectionObjectGet(ntfHandle, &so), SA_AIS_OK);

  fds[0].fd = static_cast<int>(so);
  do {
    fds[0].events = POLLIN;
    int ret = poll(fds, 1, 10000);
    if (ret <= 0) {
      if (ret == 0)
        perror("timeout");
      else
        perror(NULL);
      break;
    }
    safassert(NtfTest::saNtfDispatch(ntfHandle, SA_DISPATCH_ALL), SA_AIS_OK);
  } while (last_not_id != wanted_id);
}

static void print_header(SaNtfNotificationHeaderT *notificationHeader,
       SaNtfSubscriptionIdT subscriptionId,
       SaNtfNotificationTypeT notificationType) {
  SaTimeT totalTime;
  char time_str[24];
  struct tm result;

  /* Notification ID */
  (void)printf("notificationID = %d\n",
               static_cast<int>(*(notificationHeader->notificationId)));

  (void)printf("subscriptionId = %u\n", (unsigned int)subscriptionId);

  /* Event type */
  (void)printf("eventType = ");
  print_event_type(*notificationHeader->eventType, notificationType);

  /* Notification Object */
  (void)printf("notificationObject.length = %u\n",
         notificationHeader->notificationObject->length);

  (void)printf("notificationObject value: \"%s\"\n",
         notificationHeader->notificationObject->value);

  /* Notifying Object */
  (void)printf("notifyingObject.length = %u\n",
         notificationHeader->notifyingObject->length);

  (void)printf("notifyingObject value: \"%s\"\n",
         notificationHeader->notifyingObject->value);

  /* Notification Class ID */
  (void)printf("VendorID = %d\nmajorID = %d\nminorID = %d\n",
         notificationHeader->notificationClassId->vendorId,
         notificationHeader->notificationClassId->majorId,
         notificationHeader->notificationClassId->minorId);

  /* Event Time */
  SaTimeT ntfTime = *notificationHeader->eventTime;

  totalTime = (ntfTime / (SaTimeT)SA_TIME_ONE_SECOND);
  (void)strftime(time_str, sizeof(time_str), "%d-%m-%Y %T",
           localtime_r((const time_t *)&totalTime, &result));

  (void)printf("eventTime = %lld = %s\n", (SaTimeT)ntfTime, time_str);

  /* Length of Additional text */
  (void)printf("lengthadditionalText = %d\n",
         notificationHeader->lengthAdditionalText);

  /* Additional text */
  (void)printf("additionalText = %s\n\n",
         notificationHeader->additionalText);
}

/* Notification callback function */
void newNotification(SaNtfSubscriptionIdT subscriptionId,
         const SaNtfNotificationsT *notification) {
  SaInt32T i;
  SaNtfNotificationHandleT notificationHandle = 0;
  switch (notification->notificationType) {
  case SA_NTF_TYPE_ALARM:
    (void)printf(
        "\n===  notificationType: alarm notification ===\n");
    notificationHandle = notification->notification
           .alarmNotification.notificationHandle;

    print_header(
        const_cast<SaNtfNotificationHeaderT *>(
            &notification->notification
            .alarmNotification.notificationHeader),
            subscriptionId, notification->notificationType);

    /* Probable Cause */
    (void)printf("probableCause = ");
    print_probable_cause(*(notification->notification
             .alarmNotification.probableCause));

    (void)printf("perceivedSeverity = ");
    print_severity(*(notification->notification.alarmNotification
             .perceivedSeverity));

    break;

  case SA_NTF_TYPE_STATE_CHANGE:
    (void)printf(
        "===  notificationType:  state change notification ===\n");
    notificationHandle =
        notification->notification.stateChangeNotification
      .notificationHandle;

    print_header(
        const_cast<SaNtfNotificationHeaderT *>(
            &notification->notification
            .stateChangeNotification.notificationHeader),
            subscriptionId, notification->notificationType);

    (void)printf("sourceIndicator = ");
    print_source_indicator(
        *(notification->notification.stateChangeNotification
        .sourceIndicator));

    printf("Num of StateChanges: %d\n",
           notification->notification.stateChangeNotification
         .numStateChanges);

    /* Changed states */
    for (i = 0; i < notification->notification
            .stateChangeNotification.numStateChanges; i++) {
      print_change_states(
          &notification->notification.stateChangeNotification
         .changedStates[i]);
    }
    break;

  case SA_NTF_TYPE_OBJECT_CREATE_DELETE:
    (void)printf(
        "===  notificationType: object create/delete notification ===\n");
    notificationHandle =
        notification->notification.objectCreateDeleteNotification
      .notificationHandle;
    print_header(
        const_cast<SaNtfNotificationHeaderT *>(
            &notification->notification
            .objectCreateDeleteNotification.notificationHeader),
            subscriptionId, notification->notificationType);

    (void)printf("sourceIndicator = ");
    print_source_indicator(
        *(notification->notification.objectCreateDeleteNotification
        .sourceIndicator));

    /* Object Attributes */
    for (i = 0;
         i < notification->notification
           .objectCreateDeleteNotification.numAttributes; i++) {
      print_object_attributes(
          &notification->notification
         .objectCreateDeleteNotification
         .objectAttributes[i]);
    }
    break;

  case SA_NTF_TYPE_ATTRIBUTE_CHANGE:
    (void)printf(
        "===  notificationType: attribute change notification ===\n");
    notificationHandle =
        notification->notification.attributeChangeNotification
      .notificationHandle;
    print_header(
        const_cast<SaNtfNotificationHeaderT *>(
            &notification->notification
            .attributeChangeNotification.notificationHeader),
            subscriptionId, notification->notificationType);

    (void)printf("sourceIndicator = ");
    print_source_indicator(
        *(notification->notification.attributeChangeNotification
        .sourceIndicator));

    /* Changed Attributes */
    for (i = 0; i < notification->notification
            .attributeChangeNotification.numAttributes; i++) {
      print_changed_attributes(
          &notification->notification
         .attributeChangeNotification
         .changedAttributes[i]);
    }
    break;

  case SA_NTF_TYPE_SECURITY_ALARM:
    (void)printf(
        "===  notificationType:  security alarm notification ===\n");
    notificationHandle =
        notification->notification.securityAlarmNotification
      .notificationHandle;
    print_header(
        const_cast<SaNtfNotificationHeaderT *>(
            &notification->notification
            .securityAlarmNotification.notificationHeader),
            subscriptionId, notification->notificationType);

    (void)printf("probableCause = ");
    print_probable_cause(
        *(notification->notification.securityAlarmNotification
        .probableCause));

    (void)printf("Severity = ");
    print_severity(*(notification->notification
             .securityAlarmNotification.severity));

    print_security_alarm_types(
        const_cast<SaNtfSecurityAlarmNotificationT *>(
            &notification->notification.securityAlarmNotification));

    break;

  default:
    (void)printf("unknown notification type %d",
                 static_cast<int>(notification->notificationType));
    break;
  }
  (void)printf("==========================================\n");

  if (notificationHandle != 0)
    safassert(saNtfNotificationFree(notificationHandle), SA_AIS_OK);
}

/* 3.15.3.4 SaNtfNotificationDiscardedCallbackT */
void discardedNotification(
    SaNtfSubscriptionIdT subscriptionId,
    SaNtfNotificationTypeT notificationType, SaUint32T numberDiscarded,
    const SaNtfIdentifierT *discardedNotificationIdentifiers) {
  unsigned int i = 0;
  (void)printf("Discarded callback function  notificationType: %d\n "
      "subscriptionId  : %u \n"
      "numberDiscarded : %u \n",
       static_cast<int>(notificationType),
       static_cast<unsigned int>(subscriptionId),
       static_cast<unsigned int>(numberDiscarded));
  for (i = 0; i < numberDiscarded; i++) {
    (void)printf("[%u]",
           (unsigned int)discardedNotificationIdentifiers[i]);
  }
  (void)printf("\n");
}

void fillInDefaultValues(
    saNotificationAllocationParamsT *notificationAllocationParams,
    saNotificationFilterAllocationParamsT *notificationFilterAllocationParams,
    saNotificationParamsT *notificationParams) {
  /* Default notification allocation parameters */
  /* Common notification header*/
  notificationAllocationParams->numCorrelatedNotifications = 0;
  notificationAllocationParams->lengthAdditionalText =
      (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1);
  notificationAllocationParams->numAdditionalInfo = 0;

  /* Alarm specific */
  notificationAllocationParams->numSpecificProblems = 0;
  notificationAllocationParams->numMonitoredAttributes = 0;
  notificationAllocationParams->numProposedRepairActions = 0;

  /* State change specific */
  notificationAllocationParams->numStateChanges =
      DEFAULT_NUMBER_OF_CHANGED_STATES;

  /* Object Create/Delete specific */
  notificationAllocationParams->numObjectAttributes =
      DEFAULT_NUMBER_OF_OBJECT_ATTRIBUTES;

  /* Attribute Change specific */
  notificationAllocationParams->numAttributes =
      DEFAULT_NUMBER_OF_CHANGED_ATTRIBUTES;

  notificationParams->changedStates[0].stateId = MY_APP_OPER_STATE;
  notificationParams->changedStates[0].oldStatePresent = SA_FALSE;
  notificationParams->changedStates[0].newState = SA_NTF_DISABLED;

  notificationParams->changedStates[1].stateId = MY_APP_USAGE_STATE;
  notificationParams->changedStates[1].oldStatePresent = SA_FALSE;
  notificationParams->changedStates[1].newState = SA_NTF_IDLE;

  notificationParams->changedStates[2].stateId = MY_APP_OPER_STATE;
  notificationParams->changedStates[2].oldStatePresent = SA_TRUE;
  notificationParams->changedStates[2].oldState = SA_NTF_DISABLED;
  notificationParams->changedStates[2].newState = SA_NTF_ENABLED;

  notificationParams->changedStates[3].stateId = MY_APP_USAGE_STATE;
  notificationParams->changedStates[3].oldStatePresent = SA_TRUE;
  notificationParams->changedStates[3].oldState = SA_NTF_IDLE;
  notificationParams->changedStates[3].newState = SA_NTF_ACTIVE;

  notificationParams->objectAttributes[0].attributeId = 0;
  notificationParams->objectAttributes[0].attributeType =
      SA_NTF_VALUE_INT32;
  notificationParams->objectAttributes[0].attributeValue.int32Val = 1;

  notificationParams->changedAttributes[0].attributeId = 0;
  notificationParams->changedAttributes[0].attributeType =
      SA_NTF_VALUE_INT32;
  notificationParams->changedAttributes[0].oldAttributePresent = SA_FALSE;
  notificationParams->changedAttributes[0].newAttributeValue.int32Val = 1;

  /* Misc */
  notificationAllocationParams->variableDataSize =
      SA_NTF_ALLOC_SYSTEM_LIMIT;

  /* Default notification filter allocation parameters */
  notificationFilterAllocationParams->numEventTypes = 0;
  notificationFilterAllocationParams->numNotificationObjects = 0;
  notificationFilterAllocationParams->numNotifyingObjects = 0;
  notificationFilterAllocationParams->numNotificationClassIds = 0;

  /* Alarm specific */
  notificationFilterAllocationParams->numProbableCauses = 0;
  notificationFilterAllocationParams->numPerceivedSeverities = 2;
  notificationFilterAllocationParams->numTrends = 0;

  notificationFilterAllocationParams->numSourceIndicators = 0;
  notificationFilterAllocationParams->numSeverities = 0;
  notificationFilterAllocationParams->numSecurityAlarmDetectos = 0;
  notificationFilterAllocationParams->numServiceUsers = 0;
  notificationFilterAllocationParams->numServiceProviders = 0;
  notificationFilterAllocationParams->numChangedStates = 0;

  /* Default notification parameters */
  notificationParams->additionalText = (SaStringT)malloc(
      notificationAllocationParams->lengthAdditionalText);

  notificationParams->notificationType = SA_NTF_TYPE_ALARM;

  (void)strncpy(notificationParams->additionalText,
          DEFAULT_ADDITIONAL_TEXT,
          notificationAllocationParams->lengthAdditionalText);
  notificationParams->notificationObject.length =
      strlen(DEFAULT_NOTIFICATION_OBJECT);
  (void)memcpy(notificationParams->notificationObject.value,
         DEFAULT_NOTIFICATION_OBJECT,
         notificationParams->notificationObject.length);
  notificationParams->notifyingObject.length =
      strlen(DEFAULT_NOTIFYING_OBJECT);
  (void)memcpy(notificationParams->notifyingObject.value,
         DEFAULT_NOTIFYING_OBJECT,
         notificationParams->notifyingObject.length);
  notificationParams->notificationClassId.vendorId = ERICSSON_VENDOR_ID;
  notificationParams->notificationClassId.majorId = 0;
  notificationParams->notificationClassId.minorId = 0;
  notificationParams->eventTime = (SaTimeT)SA_TIME_UNKNOWN;

  /* Alarm specific */
  notificationParams->probableCause = SA_NTF_BANDWIDTH_REDUCED;
  notificationParams->perceivedSeverity = SA_NTF_SEVERITY_WARNING;
  notificationParams->alarmEventType = SA_NTF_ALARM_COMMUNICATION;

  /* State change specific */
  notificationParams->stateChangeSourceIndicator =
      SA_NTF_OBJECT_OPERATION;
  notificationParams->stateChangeEventType = SA_NTF_OBJECT_STATE_CHANGE;

  /* Object Create Delete specific */
  notificationParams->objectCreateDeleteSourceIndicator =
      SA_NTF_UNKNOWN_OPERATION;
  notificationParams->objectCreateDeleteEventType =
      SA_NTF_OBJECT_CREATION;

  /* Attribute Change params */
  notificationParams->attributeChangeSourceIndicator =
      SA_NTF_UNKNOWN_OPERATION;
  notificationParams->attributeChangeEventType = SA_NTF_ATTRIBUTE_ADDED;

  /* Security Alarm params */
  notificationParams->securityAlarmEventType = SA_NTF_INTEGRITY_VIOLATION;
  notificationParams->severity = SA_NTF_SEVERITY_CRITICAL;
  notificationParams->securityAlarmProbableCause =
      SA_NTF_UNAUTHORIZED_ACCESS_ATTEMPT;
  notificationParams->securityAlarmDetector.valueType =
      SA_NTF_VALUE_INT32;
  notificationParams->securityAlarmDetector.value.int32Val = 1;
  notificationParams->serviceProvider.valueType = SA_NTF_VALUE_INT32;
  notificationParams->serviceProvider.value.int32Val = 1;
  notificationParams->serviceUser.valueType = SA_NTF_VALUE_INT32;
  notificationParams->serviceUser.value.int32Val = 1;

  /* Reader parameters */
  notificationParams->searchCriteria.searchMode =
      SA_NTF_SEARCH_ONLY_FILTER;
  notificationParams->searchCriteria.notificationId = 0;
  notificationParams->searchCriteria.eventTime = SA_TIME_UNKNOWN;

  notificationParams->searchDirection = SA_NTF_SEARCH_YOUNGER;

  /* One day */
  notificationParams->timeout = (24 * 3600);
  /* no hang att all */
  notificationParams->hangTimeout = 0;
  /* send the same message repeatSends times */
  notificationParams->repeateSends = 1;
  notificationParams->noSubscriptionIds = 4;
  notificationParams->subscriptionId[0] = 1;
  notificationParams->subscriptionId[1] = 3;
  notificationParams->subscriptionId[2] = 2;
  notificationParams->subscriptionId[3] = 4;

  notificationParams->testHandle = 1;
  notificationParams->testVersion.releaseCode = 'A';
}

void fill_header_part(SaNtfNotificationHeaderT *notificationHeader,
          saNotificationParamsT *notificationParams,
          SaUint16T lengthAdditionalText) {
  *notificationHeader->eventType = notificationParams->eventType;
  *notificationHeader->eventTime = (SaTimeT)notificationParams->eventTime;

  *(notificationHeader->notificationObject) =
      notificationParams->notificationObject;
  *(notificationHeader->notifyingObject) =
      notificationParams->notifyingObject;

  /* vendor id 33333 is not an existing SNMP enterprise number.
  Just an example */
  notificationHeader->notificationClassId->vendorId =
      notificationParams->notificationClassId.vendorId;

  /* sub id of this notification class within "name space" of vendor ID */
  notificationHeader->notificationClassId->majorId =
      notificationParams->notificationClassId.majorId;
  notificationHeader->notificationClassId->minorId =
      notificationParams->notificationClassId.minorId;

  /* set additional text and additional info */
  (void)strncpy(notificationHeader->additionalText,
          notificationParams->additionalText, lengthAdditionalText);
}

SaNtfCallbacksT ntfSendCallbacks = {newNotification, discardedNotification};

/**
 * Compare the values of two SaNtfValueT
 *
 * @param type
 * @param val1
 * @param val2
 * @return 0 - ok, 1 - bad
 */
int cmpSaNtfValueT(const SaNtfValueTypeT type, const SaNtfValueT *val1,
       const SaNtfValueT *val2) {
  switch (type) {
  case SA_NTF_VALUE_UINT8:
    return (val1->uint8Val == val2->uint8Val) ? 0 : 1;
  case SA_NTF_VALUE_INT8:
    return (val1->int8Val == val2->int8Val) ? 0 : 1;
  case SA_NTF_VALUE_UINT16:
    return (val1->uint16Val == val2->uint16Val) ? 0 : 1;
  case SA_NTF_VALUE_INT16:
    return (val1->int16Val == val2->int16Val) ? 0 : 1;
  case SA_NTF_VALUE_UINT32:
    return (val1->uint32Val == val2->uint32Val) ? 0 : 1;
  case SA_NTF_VALUE_INT32:
    return (val1->int32Val == val2->int32Val) ? 0 : 1;
  case SA_NTF_VALUE_FLOAT:
    return (val1->floatVal == val2->floatVal) ? 0 : 1;
  case SA_NTF_VALUE_UINT64:
    return (val1->uint64Val == val2->uint64Val) ? 0 : 1;
  case SA_NTF_VALUE_INT64:
    return (val1->int64Val == val2->int64Val) ? 0 : 1;
  case SA_NTF_VALUE_DOUBLE:
    return (val1->doubleVal == val2->doubleVal) ? 0 : 1;

  /* TODO: Not supported yet */
  case SA_NTF_VALUE_LDAP_NAME:
  case SA_NTF_VALUE_STRING:
  case SA_NTF_VALUE_IPADDRESS:
  case SA_NTF_VALUE_BINARY:
  case SA_NTF_VALUE_ARRAY:
    safassert(SA_AIS_ERR_NOT_SUPPORTED, SA_AIS_OK);
    return 1;
  default:
    safassert(SA_AIS_ERR_INVALID_PARAM, SA_AIS_OK);
    return 1;
    break;
  }
}

/**
 * Fills the header with default data.
 * AdditionalText will be strlen(DEFAULT_ADDITIONAL_TEXT) + 1
 *
 * @params *head
 */
void fillHeader(SaNtfNotificationHeaderT *head) {
  int i;

  *(head->eventType) = SA_NTF_ALARM_COMMUNICATION;
  *(head->eventTime) = SA_TIME_UNKNOWN;

  head->notificationObject->length = strlen(DEFAULT_NOTIFICATION_OBJECT);
  (void)memcpy(head->notificationObject->value,
         DEFAULT_NOTIFICATION_OBJECT,
         head->notificationObject->length);

  head->notifyingObject->length = strlen(DEFAULT_NOTIFYING_OBJECT);
  (void)memcpy(head->notifyingObject->value, DEFAULT_NOTIFYING_OBJECT,
         head->notifyingObject->length);
  head->notificationClassId->vendorId = ERICSSON_VENDOR_ID;
  head->notificationClassId->majorId = 92;
  head->notificationClassId->minorId = 12;

  /* set additional text */
  (void)strncpy(head->additionalText, DEFAULT_ADDITIONAL_TEXT,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1));

  for (i = 0; i < head->numCorrelatedNotifications; i++) {
    head->correlatedNotifications[i] = (SaNtfIdentifierT)(i + 400);
  }

  for (i = 0; i < head->numAdditionalInfo; i++) {
    head->additionalInfo[i].infoId = i;
    head->additionalInfo[i].infoType = SA_NTF_VALUE_INT32;
    head->additionalInfo[i].infoValue.int32Val = 444 + i;
  }
}

/**
 * Fills the filter header with default data.
 *
 * @params *head
 */
void fillFilterHeader(SaNtfNotificationFilterHeaderT *head) {
  assert(head->numEventTypes == 1);
  assert(head->numNotificationClassIds == 1);
  assert(head->numNotificationObjects == 1);
  assert(head->numNotifyingObjects == 1);

  head->eventTypes[0] = SA_NTF_ALARM_COMMUNICATION;

  head->notificationObjects[0].length =
      strlen(DEFAULT_NOTIFICATION_OBJECT);
  memcpy(head->notificationObjects[0].value, DEFAULT_NOTIFICATION_OBJECT,
         head->notificationObjects[0].length);

  head->notifyingObjects[0].length = strlen(DEFAULT_NOTIFYING_OBJECT);
  memcpy(head->notifyingObjects[0].value, DEFAULT_NOTIFYING_OBJECT,
         head->notifyingObjects[0].length);

  head->notificationClassIds[0].vendorId = ERICSSON_VENDOR_ID;
  head->notificationClassIds[0].majorId = 92;
  head->notificationClassIds[0].minorId = 12;
}

/*
 * Verify the contents between two header
 *
 * @param head1
 * @param head2
 * @return errors
 */
int verifyNotificationHeader(const SaNtfNotificationHeaderT *head1,
           const SaNtfNotificationHeaderT *head2) {
  int errors = 0;
  int iCount; /* General counter to use */

  errors +=
      assertvalue(*(head1->notificationId) == *(head2->notificationId));

  errors += assertvalue(*(head1->eventType) == *(head2->eventType));

  errors += assertvalue(*(head1->eventTime) == *(head2->eventTime));

  errors += assertvalue(head1->lengthAdditionalText ==
            head2->lengthAdditionalText);
  errors +=
      assertvalue(strncmp(head1->additionalText, head2->additionalText,
        head1->lengthAdditionalText) == 0);

  if ((errors += assertvalue(head1->numCorrelatedNotifications ==
           head2->numCorrelatedNotifications)) == 0) {
    for (iCount = 0; iCount < head1->numCorrelatedNotifications;
         iCount++) {
      errors += assertvalue(
          head1->correlatedNotifications[iCount] ==
          head2->correlatedNotifications[iCount]);
    }
  }

  errors += assertvalue(head1->notificationObject->length ==
            head2->notificationObject->length);
  errors += assertvalue(memcmp(head1->notificationObject->value,
             head2->notificationObject->value,
             head1->notificationObject->length) == 0);

  errors += assertvalue(head1->notifyingObject->length ==
            head2->notifyingObject->length);
  errors += assertvalue(memcmp(head1->notifyingObject->value,
             head2->notifyingObject->value,
             head1->notifyingObject->length) == 0);

  if ((errors += assertvalue(head1->numAdditionalInfo ==
           head2->numAdditionalInfo)) == 0) {
    for (iCount = 0; iCount < head1->numAdditionalInfo; iCount++) {
      errors +=
          assertvalue(head1->additionalInfo[iCount].infoId ==
          head2->additionalInfo[iCount].infoId);
      errors += assertvalue(
          head1->additionalInfo[iCount].infoType ==
          head2->additionalInfo[iCount].infoType);
      errors += cmpSaNtfValueT(
          head1->additionalInfo[iCount].infoType,
          &head1->additionalInfo[iCount].infoValue,
          &head2->additionalInfo[iCount].infoValue);
    }
  }

  errors += assertvalue(head1->notificationClassId->majorId ==
            head2->notificationClassId->majorId);
  errors += assertvalue(head1->notificationClassId->minorId ==
            head2->notificationClassId->minorId);
  errors += assertvalue(head1->notificationClassId->vendorId ==
            head2->notificationClassId->vendorId);

  return errors;
}

/**
 * Creates an AlarmNotification with default values.
 *
 * @param ntfHandle
 * @param myalarmNotification
 */
SaAisErrorT
createAlarmNotification(SaNtfHandleT ntfHandle,
      SaNtfAlarmNotificationT *myAlarmNotification) {
  safassert(saNtfAlarmNotificationAllocate(
          ntfHandle, myAlarmNotification, 0,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1), 0, 0, 0,
          0, SA_NTF_ALLOC_SYSTEM_LIMIT),
      SA_AIS_OK);

  fillHeader(&myAlarmNotification->notificationHeader);

  /* determine perceived severity */
  *(myAlarmNotification->perceivedSeverity) = SA_NTF_SEVERITY_WARNING;

  /* set probable cause*/
  *(myAlarmNotification->probableCause) = SA_NTF_BANDWIDTH_REDUCED;
  *(myAlarmNotification->trend) = SA_NTF_TREND_MORE_SEVERE;

  return SA_AIS_OK;
}

/**
 * Compares the content of two alarm notifications
 *
 * @param aNtf1
 * @param aNtf2
 * @return errors
 */
int verifyAlarmNotification(const SaNtfAlarmNotificationT *aNtf1,
          const SaNtfAlarmNotificationT *aNtf2) {
  int errors = 0;
  int iCount; /* General counter */

  /* Verify header */
  errors += verifyNotificationHeader(&aNtf1->notificationHeader,
             &aNtf2->notificationHeader);

  /* Verify specific content */
  errors += assertvalue(*(aNtf1->perceivedSeverity) ==
            *(aNtf2->perceivedSeverity));
  errors +=
      assertvalue(*(aNtf1->probableCause) == *(aNtf2->probableCause));
  errors += assertvalue(*(aNtf1->trend) == *(aNtf2->trend));

  if ((errors += assertvalue(aNtf1->numSpecificProblems ==
           aNtf2->numSpecificProblems)) == 0) {
    for (iCount = 0; iCount < aNtf1->numSpecificProblems;
         iCount++) {
      errors += assertvalue(
          aNtf1->specificProblems[iCount].problemId ==
          aNtf2->specificProblems[iCount].problemId);
      errors += assertvalue(
          aNtf1->specificProblems[iCount].problemType ==
          aNtf2->specificProblems[iCount].problemType);
      errors += cmpSaNtfValueT(
          aNtf1->specificProblems[iCount].problemType,
          &aNtf1->specificProblems[iCount].problemValue,
          &aNtf2->specificProblems[iCount].problemValue);
    }
  }

  if ((errors += assertvalue(aNtf1->numMonitoredAttributes ==
           aNtf2->numMonitoredAttributes)) == 0) {
    for (iCount = 0; iCount < aNtf1->numMonitoredAttributes;
         iCount++) {
      errors += assertvalue(
          aNtf1->monitoredAttributes[iCount].attributeId ==
          aNtf2->monitoredAttributes[iCount].attributeId);
      errors += assertvalue(
          aNtf1->monitoredAttributes[iCount].attributeType ==
          aNtf2->monitoredAttributes[iCount].attributeType);
      errors += cmpSaNtfValueT(
          aNtf1->monitoredAttributes[iCount].attributeType,
          &aNtf1->monitoredAttributes[iCount].attributeValue,
          &aNtf2->monitoredAttributes[iCount].attributeValue);
    }
  }

  if ((errors += assertvalue(aNtf1->numProposedRepairActions ==
           aNtf2->numProposedRepairActions)) == 0) {
    for (iCount = 0; iCount < aNtf1->numProposedRepairActions;
         iCount++) {
      errors += assertvalue(
          aNtf1->proposedRepairActions[iCount].actionId ==
          aNtf2->proposedRepairActions[iCount].actionId);
      errors +=
          assertvalue(aNtf1->proposedRepairActions[iCount]
              .actionValueType ==
          aNtf2->proposedRepairActions[iCount]
              .actionValueType);
      errors += cmpSaNtfValueT(
          aNtf1->proposedRepairActions[iCount]
        .actionValueType,
          &aNtf1->proposedRepairActions[iCount].actionValue,
          &aNtf2->proposedRepairActions[iCount].actionValue);
    }
  }

  errors += assertvalue(aNtf1->thresholdInformation->armTime ==
            aNtf2->thresholdInformation->armTime);
  errors += assertvalue(aNtf1->thresholdInformation->thresholdId ==
            aNtf2->thresholdInformation->thresholdId);
  if ((errors += assertvalue(
     aNtf1->thresholdInformation->thresholdValueType ==
     aNtf2->thresholdInformation->thresholdValueType)) == 0) {
    errors += cmpSaNtfValueT(
        aNtf1->thresholdInformation->thresholdValueType,
        &(aNtf1->thresholdInformation->observedValue),
        &(aNtf2->thresholdInformation->observedValue));

    errors += cmpSaNtfValueT(
        aNtf1->thresholdInformation->thresholdValueType,
        &(aNtf1->thresholdInformation->thresholdHysteresis),
        &(aNtf2->thresholdInformation->thresholdHysteresis));

    errors += cmpSaNtfValueT(
        aNtf1->thresholdInformation->thresholdValueType,
        &(aNtf1->thresholdInformation->thresholdValue),
        &(aNtf2->thresholdInformation->thresholdValue));
  }

  return errors;
}

/**
 * Creates an ObjectCreateDeleteNotification with default values.
 *
 * @param ntfHandle
 * @param myObjCreDelNotification
 */
void createObjectCreateDeleteNotification(
    SaNtfHandleT ntfHandle,
    SaNtfObjectCreateDeleteNotificationT *myObjCrDelNotification) {
  SaNtfNotificationHeaderT *head;
  safassert(saNtfObjectCreateDeleteNotificationAllocate(
          ntfHandle, /* handle to Notification Service instance */
          myObjCrDelNotification, 0,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1), 0, 2,
          SA_NTF_ALLOC_SYSTEM_LIMIT),
      SA_AIS_OK);

  head = &myObjCrDelNotification->notificationHeader;

  /* set additional text */
  (void)strncpy(head->additionalText, DEFAULT_ADDITIONAL_TEXT,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1));

  /* Event type */
  *(head->eventType) = SA_NTF_OBJECT_CREATION;

  /* event time to be set automatically to current
   time by saNtfNotificationSend */
  *(head->eventTime) = SA_TIME_UNKNOWN;

  /* Set Notification Object */
  head->notificationObject->length =
      (SaUint16T)(sizeof(DEFAULT_NOTIFICATION_OBJECT));
  (void)memcpy(head->notificationObject->value,
         DEFAULT_NOTIFICATION_OBJECT,
         head->notificationObject->length);

  /* Set Notifying Object */
  head->notifyingObject->length =
      (SaUint16T)(sizeof(DEFAULT_NOTIFYING_OBJECT));
  (void)memcpy(head->notifyingObject->value, DEFAULT_NOTIFYING_OBJECT,
         head->notifyingObject->length);

  /* set Notification Class Identifier */
  /* vendor id 33333 is not an existing SNMP enterprise number.
   Just an example */
  head->notificationClassId->vendorId = ERICSSON_VENDOR_ID;

  /* sub id of this notification class within "name space" of vendor ID */
  head->notificationClassId->majorId = 92;
  head->notificationClassId->minorId = 12;

  /* set additional text and additional info */
  (void)strncpy(head->additionalText, DEFAULT_ADDITIONAL_TEXT,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1));

  /* Set source indicator */
  *(myObjCrDelNotification->sourceIndicator) = SA_NTF_UNKNOWN_OPERATION;

  /* Set objectAttibutes */
  myObjCrDelNotification->objectAttributes[0].attributeId = 0;
  myObjCrDelNotification->objectAttributes[0].attributeType =
      SA_NTF_VALUE_INT32;
  myObjCrDelNotification->objectAttributes[0].attributeValue.int32Val = 1;
  myObjCrDelNotification->objectAttributes[1].attributeId = 1;
  myObjCrDelNotification->objectAttributes[1].attributeType =
      SA_NTF_VALUE_INT32;
  myObjCrDelNotification->objectAttributes[1].attributeValue.int32Val = 2;
}

/**
 * Compares the content of two notifications
 *
 * @param aNtf1
 * @param aNtf2
 * @return errors
 */
int verifyObjectCreateDeleteNotification(
    const SaNtfObjectCreateDeleteNotificationT *aNtf1,
    const SaNtfObjectCreateDeleteNotificationT *aNtf2) {
  int errors = 0;

  errors += verifyNotificationHeader(&aNtf1->notificationHeader,
             &aNtf2->notificationHeader);

  /* Verify the content */
  errors +=
      assertvalue(*aNtf1->sourceIndicator == *aNtf2->sourceIndicator);
  if ((errors +=
       assertvalue(aNtf1->numAttributes == aNtf2->numAttributes)) == 0) {
    for (int iCount = 0; iCount < aNtf1->numAttributes; iCount++) {
      errors += assertvalue(
          aNtf1->objectAttributes[iCount].attributeId ==
          aNtf2->objectAttributes[iCount].attributeId);
      errors += assertvalue(
          aNtf1->objectAttributes[iCount].attributeType ==
          aNtf2->objectAttributes[iCount].attributeType);
      errors += cmpSaNtfValueT(
          aNtf1->objectAttributes[iCount].attributeType,
          &aNtf1->objectAttributes[iCount].attributeValue,
          &aNtf2->objectAttributes[iCount].attributeValue);
    }
  }

  return errors;
}

/**
 * Create a notification.
 *
 * @param ntfhandle
 * @param myAttrChangeNotification
 */
void createAttributeChangeNotification(
    SaNtfHandleT ntfHandle,
    SaNtfAttributeChangeNotificationT *myAttrChangeNotification) {
  SaNtfNotificationHeaderT *head;

  safassert(saNtfAttributeChangeNotificationAllocate(
          ntfHandle, myAttrChangeNotification, 1,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1), 2, 2,
          SA_NTF_ALLOC_SYSTEM_LIMIT),
      SA_AIS_OK);

  head = &myAttrChangeNotification->notificationHeader;
  *(head->eventType) = SA_NTF_ATTRIBUTE_CHANGED;
  *(head->eventTime) = SA_TIME_UNKNOWN;

  /* Set source indicator */
  *(myAttrChangeNotification->sourceIndicator) = SA_NTF_OBJECT_OPERATION;

  /* set additional text */
  (void)strncpy(head->additionalText, DEFAULT_ADDITIONAL_TEXT,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1));

  /* set Notification Class Identifier */
  /* vendor id 33333 is not an existing SNMP enterprise number.
   Just an example */
  head->notificationClassId->vendorId = ERICSSON_VENDOR_ID;

  /* sub id of this notification class within "name space" of vendor ID */
  head->notificationClassId->majorId = 92;
  head->notificationClassId->minorId = 12;

  head->notificationObject->length =
      (SaUint16T)(strlen(DEFAULT_NOTIFICATION_OBJECT));
  (void)memcpy(head->notificationObject->value,
         DEFAULT_NOTIFICATION_OBJECT,
         head->notificationObject->length);

  /* Set Notifying Object */
  head->notifyingObject->length =
      (SaUint16T)(strlen(DEFAULT_NOTIFYING_OBJECT));
  (void)memcpy(head->notifyingObject->value, DEFAULT_NOTIFYING_OBJECT,
         head->notifyingObject->length);

  head->additionalInfo[0].infoId = 5;
  head->additionalInfo[0].infoType = SA_NTF_VALUE_INT32;
  head->additionalInfo[0].infoValue.int32Val = 55;

  head->additionalInfo[1].infoId = 7;
  head->additionalInfo[1].infoType = SA_NTF_VALUE_INT16;
  head->additionalInfo[1].infoValue.int16Val = 77;
  head->correlatedNotifications[0] = 45ull;
  myAttrChangeNotification->changedAttributes[0].attributeId = 1;
  myAttrChangeNotification->changedAttributes[0].attributeType =
      SA_NTF_VALUE_INT32;
  myAttrChangeNotification->changedAttributes[0]
      .newAttributeValue.int32Val = 32;
  myAttrChangeNotification->changedAttributes[0].oldAttributePresent =
      SA_TRUE;
  myAttrChangeNotification->changedAttributes[0]
      .oldAttributeValue.int32Val = 33;

  myAttrChangeNotification->changedAttributes[1].attributeId = 2;
  myAttrChangeNotification->changedAttributes[1].attributeType =
      SA_NTF_VALUE_INT16;
  myAttrChangeNotification->changedAttributes[1]
      .newAttributeValue.int16Val = 4;
  myAttrChangeNotification->changedAttributes[1].oldAttributePresent =
      SA_TRUE;
  myAttrChangeNotification->changedAttributes[1]
      .oldAttributeValue.int16Val = 44;
}

/**
 * Verify the content of an AttributeChangedNotification
 *
 * @param aNtf1
 * @param aNtf2
 * @return errors
 */
int verifyAttributeChangeNotification(
    const SaNtfAttributeChangeNotificationT *aNtf1,
    const SaNtfAttributeChangeNotificationT *aNtf2) {
  int errors = 0;

  errors += verifyNotificationHeader(&aNtf1->notificationHeader,
             &aNtf2->notificationHeader);

  errors +=
      assertvalue(*(aNtf1->sourceIndicator) == *(aNtf2->sourceIndicator));

  /* Verify the attributes */
  if ((errors +=
       assertvalue(aNtf1->numAttributes == aNtf2->numAttributes)) == 0) {
    for (int iCount = 0; iCount < aNtf1->numAttributes; iCount++) {
      errors += assertvalue(
          aNtf1->changedAttributes[iCount].attributeId ==
          aNtf2->changedAttributes[iCount].attributeId);
      errors += assertvalue(
          aNtf1->changedAttributes[iCount].attributeType ==
          aNtf2->changedAttributes[iCount].attributeType);

      /* Verify new attribute value */
      errors += cmpSaNtfValueT(
          aNtf1->changedAttributes[iCount].attributeType,
          &aNtf1->changedAttributes[iCount].newAttributeValue,
          &aNtf2->changedAttributes[iCount]
         .newAttributeValue);

      /* Verify old attribute value */
      errors += assertvalue(aNtf1->changedAttributes[iCount]
              .oldAttributePresent ==
                aNtf2->changedAttributes[iCount]
              .oldAttributePresent);
      if (aNtf1->changedAttributes[iCount]
        .oldAttributePresent == SA_TRUE) {
        errors += cmpSaNtfValueT(
            aNtf1->changedAttributes[iCount]
          .attributeType,
            &aNtf1->changedAttributes[iCount]
           .oldAttributeValue,
            &aNtf2->changedAttributes[iCount]
           .oldAttributeValue);
      }
    }
  }

  return errors;
}

/**
 * Create a StateChangeNotification
 *
 * @param ntfHandle
 * @param myStateChangeNotification
 */
void createStateChangeNotification(
    SaNtfHandleT ntfHandle,
    SaNtfStateChangeNotificationT *myStateChangeNotification) {
  SaNtfNotificationHeaderT *head;

  safassert(saNtfStateChangeNotificationAllocate(
          ntfHandle, myStateChangeNotification, 1,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1), 2, 2,
          SA_NTF_ALLOC_SYSTEM_LIMIT),
      SA_AIS_OK);

  head = &myStateChangeNotification->notificationHeader;
  *(head->eventType) = SA_NTF_OBJECT_STATE_CHANGE;
  *(head->eventTime) = SA_TIME_UNKNOWN;

  /* set additional text */
  (void)strncpy(head->additionalText, DEFAULT_ADDITIONAL_TEXT,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1));

  /* set Notification Class Identifier */
  /* vendor id 33333 is not an existing SNMP enterprise number.
   Just an example */
  head->notificationClassId->vendorId = ERICSSON_VENDOR_ID;

  /* sub id of this notification class within "name space" of vendor ID */
  head->notificationClassId->majorId = 92;
  head->notificationClassId->minorId = 12;

  head->notificationObject->length =
      (SaUint16T)(strlen(DEFAULT_NOTIFICATION_OBJECT));
  (void)memcpy(head->notificationObject->value,
         DEFAULT_NOTIFICATION_OBJECT,
         head->notificationObject->length);

  /* Set Notifying Object */
  head->notifyingObject->length =
      (SaUint16T)(strlen(DEFAULT_NOTIFYING_OBJECT));
  (void)memcpy(head->notifyingObject->value, DEFAULT_NOTIFYING_OBJECT,
         head->notifyingObject->length);

  head->additionalInfo[0].infoId = 5;
  head->additionalInfo[0].infoType = SA_NTF_VALUE_INT32;
  head->additionalInfo[0].infoValue.int32Val = 55;

  head->additionalInfo[1].infoId = 7;
  head->additionalInfo[1].infoType = SA_NTF_VALUE_INT16;
  head->additionalInfo[1].infoValue.int16Val = 77;

  /* Set source indicator */
  *(myStateChangeNotification->sourceIndicator) = SA_NTF_OBJECT_OPERATION;

  myStateChangeNotification->changedStates[0].stateId = 2;
  myStateChangeNotification->changedStates[0].oldStatePresent = SA_TRUE;
  myStateChangeNotification->changedStates[0].newState = 3;
  myStateChangeNotification->changedStates[0].oldState = 5;

  myStateChangeNotification->changedStates[1].stateId = 77;
  myStateChangeNotification->changedStates[1].oldStatePresent = SA_TRUE;
  myStateChangeNotification->changedStates[1].oldState = 78;
  myStateChangeNotification->changedStates[1].newState = 79;
}

/**
 * Compare two StateChangeNotifications.
 *
 * @param aNtf1
 * @param aNtf1
 * @return errors
 */
int verifyStateChangeNotification(const SaNtfStateChangeNotificationT *aNtf1,
          const SaNtfStateChangeNotificationT *aNtf2) {
  int errors = 0;

  errors += verifyNotificationHeader(&aNtf1->notificationHeader,
             &aNtf2->notificationHeader);

  errors +=
      assertvalue(*(aNtf1->sourceIndicator) == *(aNtf2->sourceIndicator));

  if ((errors += assertvalue(aNtf1->numStateChanges ==
           aNtf2->numStateChanges)) == 0) {
    for (int iCount = 0; iCount < aNtf1->numStateChanges; iCount++) {
      errors +=
          assertvalue(aNtf1->changedStates[iCount].stateId ==
          aNtf2->changedStates[iCount].stateId);
      errors +=
          assertvalue(aNtf1->changedStates[iCount].newState ==
          aNtf2->changedStates[iCount].newState);
      errors += assertvalue(
          aNtf1->changedStates[iCount].oldStatePresent ==
          aNtf2->changedStates[iCount].oldStatePresent);

      if (aNtf1->changedStates[iCount].oldStatePresent ==
          SA_TRUE) {
        errors += assertvalue(
            aNtf1->changedStates[iCount].oldState ==
            aNtf2->changedStates[iCount].oldState);
      }
    }
  }

  return errors;
}

/**
 * Create a SecurityAlarmNotification
 *
 * @param ntfHandle
 * @param mySecAlarmNotification
 */
void createSecurityAlarmNotification(
    SaNtfHandleT ntfHandle,
    SaNtfSecurityAlarmNotificationT *mySecAlarmNotification) {
  SaNtfNotificationHeaderT *head;

  safassert(saNtfSecurityAlarmNotificationAllocate(
          ntfHandle, mySecAlarmNotification, 1,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1), 2,
          SA_NTF_ALLOC_SYSTEM_LIMIT),
      SA_AIS_OK);

  head = &mySecAlarmNotification->notificationHeader;
  *(head->eventType) = SA_NTF_OPERATION_VIOLATION;
  *(head->eventTime) = SA_TIME_UNKNOWN;

  /* set additional text */
  (void)strncpy(head->additionalText, DEFAULT_ADDITIONAL_TEXT,
          (SaUint16T)(strlen(DEFAULT_ADDITIONAL_TEXT) + 1));

  /* set Notification Class Identifier */
  /* vendor id 33333 is not an existing SNMP enterprise number.
   Just an example */
  head->notificationClassId->vendorId = ERICSSON_VENDOR_ID;

  /* sub id of this notification class within "name space" of vendor ID */
  head->notificationClassId->majorId = 92;
  head->notificationClassId->minorId = 12;

  head->notificationObject->length =
      (SaUint16T)(strlen(DEFAULT_NOTIFICATION_OBJECT));
  (void)memcpy(head->notificationObject->value,
         DEFAULT_NOTIFICATION_OBJECT,
         head->notificationObject->length);

  /* Set Notifying Object */
  head->notifyingObject->length =
      (SaUint16T)(strlen(DEFAULT_NOTIFYING_OBJECT));
  (void)memcpy(head->notifyingObject->value, DEFAULT_NOTIFYING_OBJECT,
         head->notifyingObject->length);

  head->additionalInfo[0].infoId = 5;
  head->additionalInfo[0].infoType = SA_NTF_VALUE_INT32;
  head->additionalInfo[0].infoValue.int32Val = 55;

  head->additionalInfo[1].infoId = 7;
  head->additionalInfo[1].infoType = SA_NTF_VALUE_INT16;
  head->additionalInfo[1].infoValue.int16Val = 77;

  *(mySecAlarmNotification->severity) = SA_NTF_SEVERITY_MAJOR;

  *(mySecAlarmNotification->probableCause) =
      SA_NTF_AUTHENTICATION_FAILURE;

  mySecAlarmNotification->serviceUser->valueType = SA_NTF_VALUE_INT32;
  mySecAlarmNotification->serviceUser->value.int32Val = 468;

  mySecAlarmNotification->serviceProvider->valueType = SA_NTF_VALUE_INT32;
  mySecAlarmNotification->serviceProvider->value.int32Val = 789;

  mySecAlarmNotification->securityAlarmDetector->valueType =
      SA_NTF_VALUE_UINT64;
  mySecAlarmNotification->securityAlarmDetector->value.uint64Val =
      123412341234llu;
}

/**
 * Compare two SecurityAlarmNotifications
 *
 * @param aNtf1
 * @param aNtf2
 * @return errors
 */
int verifySecurityAlarmNotification(
    const SaNtfSecurityAlarmNotificationT *aNtf1,
    const SaNtfSecurityAlarmNotificationT *aNtf2) {
  int errors = 0;

  errors += verifyNotificationHeader(&aNtf1->notificationHeader,
             &aNtf2->notificationHeader);

  errors += assertvalue(*(aNtf1->severity) == *(aNtf2->severity));
  errors +=
      assertvalue(*(aNtf1->probableCause) == *(aNtf2->probableCause));

  if ((errors += assertvalue(aNtf1->serviceUser->valueType ==
           aNtf2->serviceUser->valueType)) == 0) {
    errors += cmpSaNtfValueT(aNtf1->serviceUser->valueType,
           &aNtf1->serviceUser->value,
           &aNtf2->serviceUser->value);
  }
  if ((errors += assertvalue(aNtf1->serviceProvider->valueType ==
           aNtf2->serviceProvider->valueType)) == 0) {
    errors += cmpSaNtfValueT(aNtf1->serviceProvider->valueType,
           &aNtf1->serviceProvider->value,
           &aNtf2->serviceProvider->value);
  }
  if ((errors += assertvalue(aNtf1->securityAlarmDetector->valueType ==
           aNtf2->securityAlarmDetector->valueType)) ==
      0) {
    errors +=
        cmpSaNtfValueT(aNtf1->securityAlarmDetector->valueType,
           &aNtf1->securityAlarmDetector->value,
           &aNtf2->securityAlarmDetector->value);
  }
  return errors;
}

/**
 * verbose printout
 * String will only be printed if verbose is enabled
 *
 * @param f [out] output stream
 * @param format [in] format string
 * @param ... [in] arguments of format string
 */
void fprintf_v(FILE *f, const char *format, ...) {
  va_list argptr;

  if (!verbose)
    return;

  va_start(argptr, format);
  vfprintf(f, format, argptr);
  va_end(argptr);
  fflush(f);
}

/**
 * printout in tag mode
 * String will only be printed if tag_mode is enabled
 *
 * @param f [out] output stream
 * @param format [in] format string
 * @param ... [in] arguments of format string
 */
void fprintf_t(FILE *f, const char *format, ...) {
  va_list argptr;

  if (!gl_tag_mode)
    return;

  va_start(argptr, format);
  vfprintf(f, format, argptr);
  va_end(argptr);
  fflush(f);
}

/**
 * printout in prompt mode
 * String will only be printed if prompt_mode is enabled
 *
 * @param f [out] output stream
 * @param format [in] format string
 * @param ... [in] arguments of format string
 */
void fprintf_p(FILE *f, const char *format, ...) {
  va_list argptr;

  if (!gl_prompt_mode)
    return;

  va_start(argptr, format);
  vfprintf(f, format, argptr);
  va_end(argptr);
  fflush(f);
}

static void print_hex_ascii_line(const unsigned char *payload,
    int len, int offset) {
    int i;
    const unsigned char *ch;

    // offset
    printf("%05d   ", offset);

    // hex
    ch = payload;
    for (i = 0; i < len; ++i) {
        printf("%02x ", *ch);
        ++ch;
        // print extra space after 8th byte for visual aid
        if (i == 7)
            printf(" ");
    }
    // print space to handle line less than 8 bytes
    if (len < 8)
        printf(" ");

    // fill hex gap with spaces if not full line
    if (len < 16) {
        int gap = 16 - len;
        for (i = 0; i < gap; ++i) {
            printf("   ");
        }
    }
    printf("   ");

    // ascii (if printable)
    ch = payload;
    for (i = 0; i < len; ++i) {
        if (isprint(*ch))
            printf("%c", *ch);
        else
            printf(".");
        ++ch;
    }

    printf("\n");

    return;
}

void print_mem(const unsigned char *mem, int len) {
    if (len <= 0 || !mem)
        return;
    const unsigned char *ch = mem;

    // data fits on one line
    int line_width = 16;          // number of bytes per line
    int offset = 0;               // zero-based offset counter
    if (len <= line_width) {
        print_hex_ascii_line(ch, len, offset);
        return;
    }

    // data spans multiple lines
    int len_rem = len;
    for ( ;; ) {
        // compute current line length
        int line_len = line_width % len_rem;
        // print line
        print_hex_ascii_line(ch, line_len, offset);
        // compute total remaining /
        len_rem = len_rem - line_len;
        // shift pointer to remaining bytes to print
        ch = ch + line_len;
        // add offset
        offset = offset + line_width;
        // check if we have line width chars or less
        if (len_rem <= line_width) {
            // print last line and get out
            print_hex_ascii_line(ch, len_rem, offset);
            break;
        }
    }
    return;
}

