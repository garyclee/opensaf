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

#include "imm/apitest/immtest.h"

void saImmOmFinalize_01(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmFinalize(immOmHandle);
	test_validate(rc, SA_AIS_OK);
}

void saImmOmFinalize_02(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmFinalize(-1);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmFinalize() API:
 * Test: Finalizing with finalized handle
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:call saImmOmFinalize()
 * step3:call saImmOmFinalize() with immOmHandle handle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmFinalize_with_finalized_handle(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmFinalize(immOmHandle);
	rc = immutil_saImmOmFinalize(immOmHandle);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saImmOmFinalize() API:
 * Test: Finalizing with uninitialized handle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmFinalize_with_uninitialized_handle(void)
{
	rc = immutil_saImmOmFinalize(immOmHandle);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}
