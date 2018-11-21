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

/* Object to test: saLogFinalize() API:
 * Test: Finalizing with valid handle
 * step1:Call saLogInitialize() API and it returns SA_AIS_OK
 * step2:Now call the saLogFinalize() with logHandle
 * Result: Shall PASS with return code SA_AIS_OK
 */
void saLogFinalize_with_valid_hdl(void)
{
	rc = logInitialize();
	if (rc == SA_AIS_OK)
		rc = logFinalize();
	test_validate(rc, SA_AIS_OK);
}

/* Object to test: saLogFinalize() API:
 * Test: Finalizing with invalid handle(0)
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogFinalize_with_invalid_handle(void)
{
	rc = saLogFinalize(0);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saLogFinalize() API:
 * Test: Finalizing with finalized handle
 * step1:Call saLogInitialize() API and it returns SA_AIS_OK
 * step2:call logFinalize()
 * step3:Now call the saLogFinalize() with logHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogFinalize_with_finalized_handle(void)
{
	SaVersionT log_version = kLogVersion;
	rc = saLogInitialize(&logHandle, &logCallbacks, &log_version);
	logFinalize();
	rc = saLogFinalize(logHandle);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

__attribute__((constructor)) static void saLibraryLifeCycle_constructor(void)
{
	test_suite_add(1, "Library Life Cycle");
	test_case_add(1, saLogFinalize_with_valid_hdl, "saLogFinalize() OK");
	test_case_add(1, saLogFinalize_with_invalid_handle,
		      "saLogFinalize() with NULL log handle");
	test_case_add(1, saLogFinalize_with_finalized_handle,
		      "saLogFinalize() with finalized log handle");
}
