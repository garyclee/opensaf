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

#include "logtest.h"

/* Object to test: saLogDispatch() API
 * Test: Dispatching with valid handle and flag
 * Result: Shall pass with return code SA_AIS_OK
 */
void saLogDispatch_with_valid_hdl(void)
{
	rc = logInitialize();
	if (rc == SA_AIS_OK)
		rc = saLogDispatch(logHandle, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_OK);
	logFinalize();
}

/* Object to test: saLogDispatch() API:
 * Test: Dispatching with finalized log handle
 * step1:Call logInitialize()
 * step2:call logFinalize()
 * step3:Now call the saLogDispatch() with logHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogDispatch_with_finalized_handle(void)
{
	rc = logInitialize();
	logFinalize();
	rc = saLogDispatch(logHandle, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saLogDispatch() API:
 * Test: Dispatching with invalid handle(0)
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogDispatch_with_invalid_handle(void)
{
	rc = logInitialize();
	if (rc == SA_AIS_OK)
		rc = saLogDispatch(0, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	logFinalize();
}

__attribute__((constructor)) static void saLibraryLifeCycle_constructor(void)
{
	test_suite_add(1, "Library Life Cycle");
	test_case_add(1, saLogDispatch_with_valid_hdl, "saLogDispatch() OK");
	test_case_add(
	    1, saLogDispatch_with_finalized_handle,
	    "saLogDispatch() with finalized handle");
	test_case_add(
	    1, saLogDispatch_with_invalid_handle,
	    "saLogDispatch() with invalid handle");
}
