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
 * Author(s): Ericsson AB
 *
 */
#include "osaf/apitest/utest.h"
#include "osaf/apitest/util.h"
#include "ntf/apitest/tet_ntf.h"
#include "ntf/apitest/tet_ntf_common.h"
#include "ntf/apitest/ntf_api_with_try_again.h"

/* Parameter stuct instances */
static saNotificationAllocationParamsT myNotificationAllocationParams;
static saNotificationFilterAllocationParamsT
    myNotificationFilterAllocationParams;
static saNotificationParamsT myNotificationParams;

/**
 * A successful finalization of the reader interface.
 */
void saNtfNotificationReadFinalize_01(void) {
  SaNtfSearchCriteriaT searchCriteria = {SA_NTF_SEARCH_AT_OR_AFTER_TIME, 0, 0};
  SaNtfAlarmNotificationFilterT myAlarmFilter;
  SaNtfNotificationTypeFilterHandlesT myNotificationFilterHandles = {
      0, 0, 0, 0, 0};
  SaNtfReadHandleT readHandle;
  SaNtfHandleT ntfHandle;

  fillInDefaultValues(&myNotificationAllocationParams,
          &myNotificationFilterAllocationParams,
          &myNotificationParams);

  safassert(NtfTest::saNtfInitialize(&ntfHandle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  myNotificationFilterAllocationParams.numPerceivedSeverities = 0;
  safassert(
      saNtfAlarmNotificationFilterAllocate(
    ntfHandle,      /* handle to Notification Service instance */
    &myAlarmFilter, /* put filter here */
    /* number of event types */
    myNotificationFilterAllocationParams.numEventTypes,
    /* number of notification objects */
    myNotificationFilterAllocationParams.numNotificationObjects,
    /* number of notifying objects */
    myNotificationFilterAllocationParams.numNotifyingObjects,
    /* number of notification class ids */
    myNotificationFilterAllocationParams.numNotificationClassIds,
    /* number of probable causes */
    myNotificationFilterAllocationParams.numProbableCauses,
    /* number of perceived severities */
    myNotificationFilterAllocationParams.numPerceivedSeverities,
    /* number of trend indications */
    myNotificationFilterAllocationParams.numTrends),
      SA_AIS_OK);

  myNotificationFilterHandles.alarmFilterHandle =
      myAlarmFilter.notificationFilterHandle;

  safassert(NtfTest::saNtfNotificationReadInitialize(
      searchCriteria, &myNotificationFilterHandles, &readHandle), SA_AIS_OK);

  rc = NtfTest::saNtfNotificationReadFinalize(readHandle);
  safassert(
      saNtfNotificationFilterFree(myAlarmFilter.notificationFilterHandle),
      SA_AIS_OK);
  safassert(NtfTest::saNtfFinalize(ntfHandle), SA_AIS_OK);
  free(myNotificationParams.additionalText);
  test_validate(rc, SA_AIS_OK);
}

void saNtfNotificationReadFinalize_02(void) {
  rc = NtfTest::saNtfNotificationReadFinalize(-1);
  test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

__attribute__((constructor)) static void
saNtfNotificationReadFinalize_constructor(void) {
  test_suite_add(21, "Consumer operations - Reader API 2");
  test_case_add(21, saNtfNotificationReadFinalize_01,
          "saNtfNotificationReadFinalize SA_AIS_OK");
  test_case_add(21, saNtfNotificationReadFinalize_02,
      "saNtfNotificationReadFinalize SA_AIS_ERR_BAD_HANDLE");
}
