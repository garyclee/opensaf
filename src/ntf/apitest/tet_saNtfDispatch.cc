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
#include "ntf/apitest/ntf_api_with_try_again.h"

void saNtfDispatch_01(void) {
  safassert(NtfTest::saNtfInitialize(&ntfHandle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = NtfTest::saNtfDispatch(ntfHandle, SA_DISPATCH_ALL);
  safassert(NtfTest::saNtfFinalize(ntfHandle), SA_AIS_OK);
  test_validate(rc, SA_AIS_OK);
}

void saNtfDispatch_02(void) {
  rc = NtfTest::saNtfDispatch(0, SA_DISPATCH_ALL);
  test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

void saNtfDispatch_03(void) {
  safassert(NtfTest::saNtfInitialize(&ntfHandle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  rc = NtfTest::saNtfDispatch(ntfHandle, static_cast<SaDispatchFlagsT>(0));
  safassert(NtfTest::saNtfFinalize(ntfHandle), SA_AIS_OK);
  test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
}

void saNtfDispatch_04(void) {
  safassert(NtfTest::saNtfInitialize(&ntfHandle, &ntfCallbacks, &ntfVersion),
      SA_AIS_OK);
  safassert(NtfTest::saNtfFinalize(ntfHandle), SA_AIS_OK);
  rc = NtfTest::saNtfDispatch(ntfHandle, SA_DISPATCH_ALL);
  test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

__attribute__((constructor)) static void saNtfDispatch_constructor(void) {
  test_suite_add(4, "Life cycle, dispatch, API 4");
  test_case_add(4, saNtfDispatch_01,
          "saNtfDispatch - SA_AIS_OK SA_DISPATCH_ALL");
  test_case_add(4, saNtfDispatch_02,
          "saNtfDispatch - invalid handle SA_AIS_ERR_BAD_HANDLE");
  test_case_add(4, saNtfDispatch_03,
          "saNtfDispatch - zero flag SA_AIS_ERR_INVALID_PARAM");
  test_case_add(4, saNtfDispatch_04,
      "saNtfDispatch - Fianlized handle SA_AIS_ERR_BAD_HANDLE");
}
