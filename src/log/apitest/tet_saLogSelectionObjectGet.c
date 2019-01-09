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

/* Object to test: saLogSelectionObjectGet() API:
 * Test: Getting the SelectionObject with valid handle
 * step1:Call saLogInitialize() API and it returns SA_AIS_OK
 * step2:Now call the saLogSelectionObjectGet() with logHandle
 * Result: Shall pass with return code SA_AIS_OK
 */
void saLogSelectionObjectGet_with_valid_handle(void)
{
	rc = logInitialize();
	if (rc == SA_AIS_OK)
		rc = saLogSelectionObjectGet(logHandle, &selectionObject);
	test_validate(rc, SA_AIS_OK);
	logFinalize();
}

/* Object to test: saLogSelectionObjectGet() API:
 * Test: Getting the SelectionObject with invalid handle(0)
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogSelectionObjectGet_with_invalid_handle(void)
{
	rc = saLogSelectionObjectGet(0, &selectionObject);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saLogSelectionObjectGet() API:
 * Test: Getting the SelectionObject with finalized handle
 * step1:Call saLogInitialize() API and it returns SA_AIS_OK
 * step2:call logFinalize()
 * step3:Now call the saLogSelectionObjectGet() with logHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saLogSelectionObjectGet_with_finalized_handle(void)
{
	SaVersionT log_version = kLogVersion;
	rc = saLogInitialize(&logHandle, &logCallbacks, &log_version);
	logFinalize();
	rc = saLogSelectionObjectGet(logHandle, &selectionObject);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

__attribute__((constructor)) static void saLibraryLifeCycle_constructor(void)
{
	test_suite_add(1, "Library Life Cycle");
	test_case_add(1, saLogSelectionObjectGet_with_valid_handle,
		      "saLogSelectionObjectGet() OK");
	test_case_add(1, saLogSelectionObjectGet_with_invalid_handle,
		      "saLogSelectionObjectGet() with NULL log handle");
	test_case_add(1, saLogSelectionObjectGet_with_finalized_handle,
		      "saLogSelectionObjectGet() with finalized log handle");
}
