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

void saImmOmDispatch_01(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmDispatch_02(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_ONE);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmDispatch_03(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = saImmOmDispatch(-1, SA_DISPATCH_ONE);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmDispatch_04(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = saImmOmDispatch(immOmHandle, -1);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmDispatch() API:
 * Test:Dispatching with uninitialized handle and SA_DISPATCH_ONE
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmDispatch_with_uninitialized_handle_with_SA_DISPATCH_ONE(void)
{
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_ONE);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saImmOmDispatch() API:
 * Test: Dispatching with finalized handle and SA_DISPATCH_ONE
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:call saImmOmFinalize()
 * step3:Now call the saImmOmDispatch() with immOmHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmDispatch_with_finalized_handle_with_SA_DISPATCH_ONE(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	immutil_saImmOmFinalize(immOmHandle);
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_ONE);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saImmOmDispatch() API:
 * Test:Dispatching with initialized handle and SA_DISPATCH_BLOCKING
 * Result: Shall pass with return code SA_AIS_OK
 */
void saImmOmDispatch_with_SA_DISPATCH_BLOCKING(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_BLOCKING);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmDispatch() API:
 * Test: Dispatching with invalid handle and SA_DISPATCH_ALL
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:Now call the saImmOmDispatch() with -1
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmDispatch_with_invalid_handle_with_SA_DISPATCH_ALL(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = saImmOmDispatch(-1, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmDispatch() API:
 * Test:Dispatching with uninitialized handle and SA_DISPATCH_ALL
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmDispatch_with_uninitialized_handle_with_SA_DISPATCH_ALL(void)
{
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saImmOmDispatch() API:
 * Test: Dispatching with finalized handle and SA_DISPATCH_ALL
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:call saImmOmFinalize()
 * step3:Now call the saImmOmDispatch() with immOmHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */

void saImmOmDispatch_with_finalized_handle_with_SA_DISPATCH_ALL(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	immutil_saImmOmFinalize(immOmHandle);
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saImmOmDispatch() API:
 * Test: Dispatching with invalid handle and SA_DISPATCH_BLOCKING
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:Now call the saImmOmDispatch() with -1
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmDispatch_with_invalid_handle_with_SA_DISPATCH_BLOCKING(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = saImmOmDispatch(-1, SA_DISPATCH_BLOCKING);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmDispatch() API:
 * Test:Dispatching with uninitialized handle and SA_DISPATCH_BLOCKING
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmDispatch_with_uninitialized_handle_with_SA_DISPATCH_BLOCKING(void)
{
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_BLOCKING);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saImmOmDispatch() API:
 * Test: Dispatching with finalized handle and SA_DISPATCH_BLOCKING
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:call saImmOmFinalize()
 * step3:Now call the saImmOmDispatch() with immOmHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmDispatch_with_finalized_handle_with_SA_DISPATCH_BLOCKING(void)
{
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	immutil_saImmOmFinalize(immOmHandle);
	rc = saImmOmDispatch(immOmHandle, SA_DISPATCH_BLOCKING);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}
