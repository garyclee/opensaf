/*      -*- OpenSAF -*-
 *
 * (C) Copyright 2016 The OpenSAF Foundation
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
 * Author(s): Oracle
 *
 */

#include "osaf/apitest/utest.h"
#include "osaf/apitest/util.h"
#include "ntf/apitest/tet_ntf.h"
#include "ntf/apitest/tet_ntf_common.h"
#include "ntf/apitest/ntf_api_with_try_again.h"

char lock_cmd[80];
char unlock_cmd[80];

SaNtfHandleT ntf_Handle;
SaNtfStateChangeNotificationT st_not;
SaNtfAttributeChangeNotificationT ac_not;
SaNtfObjectCreateDeleteNotificationT ocd_not;
SaNtfAlarmNotificationT alarm_not;
SaNtfSecurityAlarmNotificationT salarm_not;

SaNtfObjectCreateDeleteNotificationFilterT ocd_Filter;
SaNtfAttributeChangeNotificationFilterT ac_Filter;
SaNtfStateChangeNotificationFilterT sc_Filter;
SaNtfAlarmNotificationFilterT alarm_Filter;
SaNtfSecurityAlarmNotificationFilterT sa_Filter;
SaNtfNotificationTypeFilterHandlesT all_NotificationFilterHandles;

SaNtfReadHandleT readHandle;
SaNtfSearchCriteriaT search_criteria;
SaNtfNotificationsT notification;

int lock_clm_node(void) {
  int rc = 0;
  rc = system(lock_cmd);
  return rc;
}
int unlock_clm_node(void) {
  int rc = 0;
  rc = system(unlock_cmd);
  return rc;
}

void fill_head(SaNtfNotificationHeaderT *notificationHeader) {
  *(notificationHeader->eventTime) = SA_TIME_UNKNOWN;
  notificationHeader->notificationClassId->vendorId = 1;
  notificationHeader->notificationClassId->majorId = 2;
  notificationHeader->notificationClassId->minorId = 3;
}

void send_A0101_notifications(SaNtfNotificationTypeT notificationtype) {
  int rc = 0;
  ntfVersion.minorVersion = lowestVersion.minorVersion;
  rc = lock_clm_node();
  if (rc != 0) {
    ntfVersion.minorVersion = refVersion.minorVersion;
    return;
  }
  switch (notificationtype) {
  case SA_NTF_TYPE_ALARM:
    saNtfNotificationSend_01();
    break;
  case SA_NTF_TYPE_OBJECT_CREATE_DELETE:
    saNtfNotificationSend_02();
    break;
  case SA_NTF_TYPE_ATTRIBUTE_CHANGE:
    saNtfNotificationSend_03();
    break;
  case SA_NTF_TYPE_STATE_CHANGE:
    saNtfNotificationSend_04();
    break;
  case SA_NTF_TYPE_SECURITY_ALARM:
    saNtfNotificationSend_05();
    break;
  default:
    break;
  }
  ntfVersion.minorVersion = refVersion.minorVersion;
  unlock_clm_node();
}

void ntf_clm_A0101_01() {
  send_A0101_notifications(SA_NTF_TYPE_OBJECT_CREATE_DELETE);
}

void ntf_clm_A0101_02() {
  send_A0101_notifications(SA_NTF_TYPE_ATTRIBUTE_CHANGE);
}

void ntf_clm_A0101_03() { send_A0101_notifications(SA_NTF_TYPE_STATE_CHANGE); }
void ntf_clm_A0101_04() { send_A0101_notifications(SA_NTF_TYPE_ALARM); }
void ntf_clm_A0101_05() {
  send_A0101_notifications(SA_NTF_TYPE_SECURITY_ALARM);
}

void ntf_clm_01() {
  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  rc_assert(unlock_clm_node(), 0);
}
void ntf_clm_02() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  rc = saNtfSelectionObjectGet(ntf_Handle, &selectionObject);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  rc_assert(unlock_clm_node(), 0);
}
void ntf_clm_03() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfDispatch(ntf_Handle, SA_DISPATCH_ALL);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  rc_assert(unlock_clm_node(), 0);
}
void ntf_clm_04() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfFinalize(ntf_Handle);
  test_validate(rc, SA_AIS_OK);
  rc_assert(unlock_clm_node(), 0);
}

void ntf_clm_not_allocate_APIs(SaNtfNotificationTypeT notificationtype) {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  switch (notificationtype) {
  case SA_NTF_TYPE_STATE_CHANGE:
    rc = saNtfStateChangeNotificationAllocate(ntf_Handle, &st_not,
                0, 0, 0, 0, 0);
    break;
  case SA_NTF_TYPE_OBJECT_CREATE_DELETE:
    rc = saNtfObjectCreateDeleteNotificationAllocate(
        ntf_Handle, &ocd_not, 0, 0, 0, 0, 0);
    break;
  case SA_NTF_TYPE_ATTRIBUTE_CHANGE:
    rc = saNtfAttributeChangeNotificationAllocate(
        ntf_Handle, &ac_not, 0, 0, 0, 0, 0);
    break;
  case SA_NTF_TYPE_ALARM:
    rc = saNtfAlarmNotificationAllocate(ntf_Handle, &alarm_not, 0,
                0, 0, 0, 0, 0, 0);

    break;
  case SA_NTF_TYPE_SECURITY_ALARM:
    rc = saNtfSecurityAlarmNotificationAllocate(
        ntf_Handle, &salarm_not, 0, 0, 0, 0);
    break;
  default:
    break;
  }
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

void ntf_clm_05() { ntf_clm_not_allocate_APIs(SA_NTF_TYPE_STATE_CHANGE); }
void ntf_clm_06() {
  ntf_clm_not_allocate_APIs(SA_NTF_TYPE_OBJECT_CREATE_DELETE);
}

void ntf_clm_07() { ntf_clm_not_allocate_APIs(SA_NTF_TYPE_ATTRIBUTE_CHANGE); }

void ntf_clm_08() { ntf_clm_not_allocate_APIs(SA_NTF_TYPE_ALARM); }
void ntf_clm_09() { ntf_clm_not_allocate_APIs(SA_NTF_TYPE_SECURITY_ALARM); }

void ntf_clm_10() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfStateChangeNotificationAllocate(ntf_Handle, &st_not, 0, 0, 0,
              0, 0);
  safassert(rc, SA_AIS_OK);
  fill_head(&st_not.notificationHeader);
  *(st_not.notificationHeader.eventType) = SA_NTF_OBJECT_STATE_CHANGE;
  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfNotificationSend(st_not.notificationHandle);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

void ntf_clm_11() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfStateChangeNotificationAllocate(ntf_Handle, &st_not, 0, 0, 0,
              0, 0);
  safassert(rc, SA_AIS_OK);
  fill_head(&st_not.notificationHeader);
  *(st_not.notificationHeader.eventType) = SA_NTF_OBJECT_STATE_CHANGE;
  rc_assert(lock_clm_node(), 0);
  rc = saNtfNotificationFree(st_not.notificationHandle);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

void ntf_clm_filter_allocate_APIs(SaNtfNotificationTypeT notificationtype) {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  switch (notificationtype) {
  case SA_NTF_TYPE_OBJECT_CREATE_DELETE:
    rc = saNtfObjectCreateDeleteNotificationFilterAllocate(
        ntf_Handle, &ocd_Filter, 0, 0, 0, 0, 0);
    break;
  case SA_NTF_TYPE_ATTRIBUTE_CHANGE:
    rc = saNtfAttributeChangeNotificationFilterAllocate(
        ntf_Handle, &ac_Filter, 0, 0, 0, 0, 0);
    break;
  case SA_NTF_TYPE_STATE_CHANGE:
    rc = saNtfStateChangeNotificationFilterAllocate(
        ntf_Handle, &sc_Filter, 0, 0, 0, 0, 0, 0);
    break;
  case SA_NTF_TYPE_ALARM:
    rc = saNtfAlarmNotificationFilterAllocate(
        ntf_Handle, &alarm_Filter, 0, 0, 0, 0, 0, 0, 0);
    break;
  case SA_NTF_TYPE_SECURITY_ALARM:
    rc = saNtfSecurityAlarmNotificationFilterAllocate(
        ntf_Handle, &sa_Filter, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    break;
  default:
    break;
  }
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}
void ntf_clm_12() { ntf_clm_filter_allocate_APIs(SA_NTF_TYPE_STATE_CHANGE); }
void ntf_clm_13() {
  ntf_clm_filter_allocate_APIs(SA_NTF_TYPE_OBJECT_CREATE_DELETE);
}
void ntf_clm_14() {
  ntf_clm_filter_allocate_APIs(SA_NTF_TYPE_ATTRIBUTE_CHANGE);
}
void ntf_clm_15() { ntf_clm_filter_allocate_APIs(SA_NTF_TYPE_ALARM); }
void ntf_clm_16() { ntf_clm_filter_allocate_APIs(SA_NTF_TYPE_SECURITY_ALARM); }
void ntf_clm_17() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfStateChangeNotificationFilterAllocate(ntf_Handle, &sc_Filter,
              0, 0, 0, 0, 0, 0);
  safassert(rc, SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  rc = saNtfNotificationFilterFree(sc_Filter.notificationFilterHandle);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}
void ntf_clm_18() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfStateChangeNotificationFilterAllocate(ntf_Handle, &sc_Filter,
              0, 0, 0, 0, 0, 0);
  safassert(rc, SA_AIS_OK);
  all_NotificationFilterHandles.stateChangeFilterHandle =
      sc_Filter.notificationFilterHandle;
  all_NotificationFilterHandles.objectCreateDeleteFilterHandle = 0;
  all_NotificationFilterHandles.attributeChangeFilterHandle = 0;
  all_NotificationFilterHandles.alarmFilterHandle = 0;
  all_NotificationFilterHandles.securityAlarmFilterHandle = 0;

  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfNotificationSubscribe(&all_NotificationFilterHandles, 1);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

void ntf_clm_19() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfStateChangeNotificationFilterAllocate(ntf_Handle, &sc_Filter,
              0, 0, 0, 0, 0, 0);
  safassert(rc, SA_AIS_OK);
  all_NotificationFilterHandles.stateChangeFilterHandle =
      sc_Filter.notificationFilterHandle;
  all_NotificationFilterHandles.objectCreateDeleteFilterHandle = 0;
  all_NotificationFilterHandles.attributeChangeFilterHandle = 0;
  all_NotificationFilterHandles.alarmFilterHandle = 0;
  all_NotificationFilterHandles.securityAlarmFilterHandle = 0;

  safassert(NtfTest::saNtfNotificationSubscribe(
      &all_NotificationFilterHandles, 1), SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  rc = saNtfNotificationUnsubscribe(1);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

void ntf_clm_20() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfAlarmNotificationFilterAllocate(ntf_Handle, &alarm_Filter, 0,
              0, 0, 0, 0, 0, 0);
  safassert(rc, SA_AIS_OK);
  all_NotificationFilterHandles.stateChangeFilterHandle = 0;
  all_NotificationFilterHandles.objectCreateDeleteFilterHandle = 0;
  all_NotificationFilterHandles.attributeChangeFilterHandle = 0;
  all_NotificationFilterHandles.alarmFilterHandle =
      alarm_Filter.notificationFilterHandle;
  all_NotificationFilterHandles.securityAlarmFilterHandle = 0;
  search_criteria.searchMode = SA_NTF_SEARCH_ONLY_FILTER;
  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfNotificationReadInitialize(
      search_criteria, &all_NotificationFilterHandles, &readHandle);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}
void ntf_clm_21() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfAlarmNotificationFilterAllocate(ntf_Handle, &alarm_Filter, 0,
              0, 0, 0, 0, 0, 0);
  safassert(rc, SA_AIS_OK);
  all_NotificationFilterHandles.stateChangeFilterHandle = 0;
  all_NotificationFilterHandles.objectCreateDeleteFilterHandle = 0;
  all_NotificationFilterHandles.attributeChangeFilterHandle = 0;
  all_NotificationFilterHandles.alarmFilterHandle =
      alarm_Filter.notificationFilterHandle;
  all_NotificationFilterHandles.securityAlarmFilterHandle = 0;
  search_criteria.searchMode = SA_NTF_SEARCH_ONLY_FILTER;
  safassert(NtfTest::saNtfNotificationReadInitialize(
    search_criteria, &all_NotificationFilterHandles, &readHandle), SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfNotificationReadNext(
      readHandle, SA_NTF_SEARCH_YOUNGER, &notification);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}
void ntf_clm_22() {
  safassert(NtfTest::saNtfInitialize(&ntf_Handle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = saNtfAlarmNotificationFilterAllocate(ntf_Handle, &alarm_Filter, 0,
              0, 0, 0, 0, 0, 0);
  safassert(rc, SA_AIS_OK);
  all_NotificationFilterHandles.stateChangeFilterHandle = 0;
  all_NotificationFilterHandles.objectCreateDeleteFilterHandle = 0;
  all_NotificationFilterHandles.attributeChangeFilterHandle = 0;
  all_NotificationFilterHandles.alarmFilterHandle =
      alarm_Filter.notificationFilterHandle;
  all_NotificationFilterHandles.securityAlarmFilterHandle = 0;
  search_criteria.searchMode = SA_NTF_SEARCH_ONLY_FILTER;
  safassert(NtfTest::saNtfNotificationReadInitialize(
    search_criteria, &all_NotificationFilterHandles, &readHandle), SA_AIS_OK);
  rc_assert(lock_clm_node(), 0);
  rc = NtfTest::saNtfNotificationReadFinalize(readHandle);
  rc_assert(unlock_clm_node(), 0);
  safassert(NtfTest::saNtfFinalize(ntf_Handle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

__attribute__((constructor)) static void ntf_clm_constructor(void) {
  FILE *fp;
  char rdn[80], clm_node_name[80];
  char *ptr = NULL;
  char buffer[80] = {"\0"};
  // Get the RDN of CLM node of this node.
  snprintf(buffer, sizeof(buffer), "cat /etc/opensaf/node_name");
  fp = popen(buffer, "r");
  if (fgets(rdn, sizeof(rdn), fp) != NULL) {
    if ((ptr = strchr(rdn, '\n')) != NULL)
      *ptr = '\0';
  } else {
    printf(
        "Could not retreive CLM node name from '/etc/opensaf/node_name'");
    pclose(fp);
    return;
  }
  pclose(fp);

  // Get the cluster name
  char cluster_name[80];
  snprintf(buffer, sizeof(buffer), "immfind -c SaClmCluster");
  fp = popen(buffer, "r");
  if (fgets(cluster_name, sizeof(cluster_name), fp) != NULL) {
    if ((ptr = strchr(cluster_name, '\n')) != NULL)
      *ptr = '\0';
  } else {
    printf(
        "Could not retreive CLM cluster name using 'immfind -c SaClmCluster'");
    pclose(fp);
    return;
  }
  pclose(fp);

  if (snprintf(clm_node_name,
               strlen("safNode=") + strlen(rdn) + strlen(cluster_name) + 2,
               "safNode=%s,%s", rdn, cluster_name) >=
                 static_cast<int>(strlen("safNode=") + strlen(rdn) +
                                  strlen(cluster_name) + 2)) {
    std::cerr << "truncation on clm_node_name: " << clm_node_name << std::endl;
  }
  if (snprintf(lock_cmd, strlen("immadm -o 2 ") + strlen(clm_node_name) + 1,
               "immadm -o 2 %s", clm_node_name) >=
                 static_cast<int>(strlen("immadm -o 2 ") +
                                  strlen(clm_node_name) + 1)) {
    std::cerr << "truncation on lock_cmd: " << lock_cmd << std::endl;
  }
  if (snprintf(unlock_cmd, strlen("immadm -o 1 ") + strlen(clm_node_name) + 1,
               "immadm -o 1 %s", clm_node_name) >=
                 static_cast<int>(strlen("immadm -o 1 ") +
                                  strlen(clm_node_name) + 1)) {
    std::cerr << "truncation on unlock_cmd: " << unlock_cmd << std::endl;
  }

  // For debugging purpose.
  // printf("node_name:'%s'\n",clm_node_name);
  // printf("lock_cmd:'%s'\n",lock_cmd);
  // printf("unlock_cmd:'%s'\n",unlock_cmd);

  // The following tests are added only if not running on an Active
  // controller node
  int rc = system("rdegetrole");
  if (rc == 0) {
    char role[80];
    // Command rdegetrole returning OK means controller node.
    memset(buffer, '\0', sizeof(buffer));
    memset(role, '\0', sizeof(role));
    snprintf(buffer, sizeof(buffer), "rdegetrole");
    fp = popen(buffer, "r");
    if (fgets(role, sizeof(role), fp) != NULL) {
      if ((ptr = strchr(role, '\n')) != NULL)
        *ptr = '\0';
      if (!strcmp(const_cast<char *>(role), "ACTIVE")) {
        printf("Active controller node. "
          "Do not run CLM tests\n");
        pclose(fp);
        return;
      }
      printf("Standby controller node. Run CLM tests\n");
    }
    pclose(fp);
  } else {
    printf("Payload node. Run CLM tests\n");
  }

  test_suite_add(40, "Ntf CLM Integration test suite\n");

  // API with A0101 initialization should work.
  test_case_add(40, ntf_clm_A0101_01,
      "Send Object Ntf with A0101 init on CLM locked node - SA_AIS_OK");
  test_case_add(40, ntf_clm_A0101_02,
      "Send Attribute Ntf with A0101 on CLM locked node - SA_AIS_OK");
  test_case_add(40, ntf_clm_A0101_03,
      "Send State Change Ntf with A0101 on CLM locked node - SA_AIS_OK");
  test_case_add(40, ntf_clm_A0101_04,
      "Send Alarm Ntf with A0101 init on CLM locked node - SA_AIS_OK");
  test_case_add(40, ntf_clm_A0101_05,
      "Send Security Ntf with A0101 on CLM locked node - SA_AIS_OK");

  // Each API with A0102 initialization.
  test_case_add(40, ntf_clm_01,
      "saNtfInitialize() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_02,
      "saNtfSelectionObjectGet() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_03,
      "saNtfDispatch() with A0102 on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_04,
      "NtfTest::saNtfFinalize() with A0102 on CLM locked node - SA_AIS_OK");
  test_case_add(40, ntf_clm_05,
      "saNtfStateChangeNotificationAllocate() with A0102 "
      "on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_06,
      "saNtfObjectCreateDeleteNotificationAllocate() with A0102 "
      "on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_07,
      "saNtfAttributeChangeNotificationAllocate() with A0102 "
      "on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_08,
      "saNtfAlarmNotificationAllocate() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_09,
      "saNtfSecurityAlarmNotificationAllocate() with A0102 on CLM locked node -"
      " SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_10,
      "saNtfNotificationSend() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_11,
      "saNtfNotificationFree() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_12,
      "saNtfStateChangeNotificationFilterAllocate() with A0102 "
      "on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_13,
      "saNtfObjectCreateDeleteNotificationFilterAllocate() with A0102 "
      "on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_14,
      "saNtfAttributeChangeNotificationFilterAllocate() with A0102 "
      "on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_15,
      "saNtfAlarmNotificationFilterAllocate() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_16,
      "saNtfSecurityAlarmNotificationFilterAllocate() with A0102 "
      "on CLM locked node - SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_17,
      "saNtfNotificationFilterFree() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_18,
      "saNtfNotificationSubscribe() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_19,
      "saNtfNotificationUnsubscribe() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_20,
      "saNtfNotificationReadInitialize() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_21,
      "saNtfNotificationReadNext() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
  test_case_add(40, ntf_clm_22,
      "saNtfNotificationReadFinalize() with A0102 on CLM locked node - "
      "SA_AIS_ERR_UNAVAILABLE");
}
