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

/* Object to test: logStreamClose() API:
 * Test: Test closing an open stream using an valid handle
 * step1:Call logInitialize()
 * step2:Call logStreamOpen()
 * step3:Now call the saLogStreamClose() with logStreamHandle
 * Result: Shall pass with return code SA_AIS_OK
 */
void saLogStreamClose_with_valid_handle(void)
{
	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		return;
	}
	rc = logStreamOpen(&systemStreamName);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}
	rc = logStreamClose();
	test_validate(rc, SA_AIS_OK);

done:
	logFinalize();
}

/* Object to test: logStreamClose() API:
 * Test: Test closing an open stream using an invalid handle
 * step1:Call logInitialize()
 * step2:Call logStreamOpen()
 * step3:Call logFinalize()
 * step4:Now call the saLogStreamClose() with logStreamHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogStreamClose_with_invalid_handle(void)
{
	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		return;
	}
	rc = logStreamOpen(&systemStreamName);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}
	logFinalize();
	rc = saLogStreamClose(0);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);

done:
	logFinalize();
}

/* Object to test: logStreamClose() API:
 * Test: Test closing a logstream  with uninitialized handle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogStreamClose_with_uninitialized_handle(void)
{
	rc = saLogStreamClose(logStreamHandle);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

__attribute__((constructor)) static void saLibraryLifeCycle_constructor(void)
{
	test_case_add(2, saLogStreamClose_with_valid_handle,
		      "saLogStreamClose OK");
	test_case_add(2, saLogStreamClose_with_invalid_handle,
		      "saLogStreamClose with invalid handle");
	test_case_add(2, saLogStreamClose_with_uninitialized_handle,
		      "saLogStreamClose with uninitialized handle");
}
