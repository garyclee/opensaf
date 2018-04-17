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

static const SaNameT rdn = {
    .value = "Obj2", .length = sizeof("Obj2"),
};

void saImmOiObjectImplementerRelease_01(void)
{
	const SaImmOiImplementerNameT implementerName =
	    (SaImmOiImplementerNameT) __FUNCTION__;

	const SaImmAdminOwnerNameT adminOwnerName =
	    (SaImmAdminOwnerNameT) __FUNCTION__;
	SaImmAdminOwnerHandleT ownerHandle;
	SaImmCcbHandleT ccbHandle;
	const SaNameT *nameValues[] = {&rdn};
	SaImmAttrValuesT_2 v2 = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				 (void **)nameValues};
	SaUint32T int1Value1 = 7;
	SaUint32T *int1Values[] = {&int1Value1};
	SaImmAttrValuesT_2 v1 = {"attr1", SA_IMM_ATTR_SAUINT32T, 1,
				 (void **)int1Values};
	const SaImmAttrValuesT_2 *attrValues[] = {&v1, &v2, NULL};
	safassert(immutil_saImmOmInitialize(&immOmHandle, NULL, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_TRUE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbInitialize(ownerHandle, 0, &ccbHandle), SA_AIS_OK);
	/* Create test object under root */
	safassert(immutil_saImmOmCcbObjectCreate_2(ccbHandle, "TestClassConfig", NULL,
					   attrValues),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	safassert(
	    immutil_saImmOiInitialize_2(&immOiHandle, &immOiCallbacks, &immVersion),
	    SA_AIS_OK);
	safassert(immutil_saImmOiImplementerSet(immOiHandle, implementerName),
		  SA_AIS_OK);
	safassert(immutil_saImmOiObjectImplementerSet(immOiHandle, &rdn, SA_IMM_ONE),
		  SA_AIS_OK);

	test_validate(
	    immutil_saImmOiObjectImplementerRelease(immOiHandle, &rdn, SA_IMM_ONE),
	    SA_AIS_OK);
	safassert(immutil_saImmOiFinalize(immOiHandle), SA_AIS_OK);

	safassert(immutil_saImmOmCcbObjectDelete(ccbHandle, &rdn), SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmCcbFinalize(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOiObjectImplementerRelease_02(void)
{
	const SaImmOiImplementerNameT implementerName =
	    (SaImmOiImplementerNameT) __FUNCTION__;
	const SaImmAdminOwnerNameT adminOwnerName =
	    (SaImmAdminOwnerNameT) __FUNCTION__;
	SaImmAdminOwnerHandleT ownerHandle;
	SaImmCcbHandleT ccbHandle;
	const SaNameT *nameValues[] = {&rdn};
	SaImmAttrValuesT_2 v2 = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				 (void **)nameValues};
	SaUint32T int1Value1 = 7;
	SaUint32T *int1Values[] = {&int1Value1};
	SaImmAttrValuesT_2 v1 = {"attr1", SA_IMM_ATTR_SAUINT32T, 1,
				 (void **)int1Values};
	const SaImmAttrValuesT_2 *attrValues[] = {&v1, &v2, NULL};
	safassert(immutil_saImmOmInitialize(&immOmHandle, NULL, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_TRUE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbInitialize(ownerHandle, 0, &ccbHandle), SA_AIS_OK);
	/* Create test object under root */
	safassert(immutil_saImmOmCcbObjectCreate_2(ccbHandle, "TestClassConfig", NULL,
					   attrValues),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	safassert(
	    immutil_saImmOiInitialize_2(&immOiHandle, &immOiCallbacks, &immVersion),
	    SA_AIS_OK);
	safassert(immutil_saImmOiImplementerSet(immOiHandle, implementerName),
		  SA_AIS_OK);
	safassert(immutil_saImmOiObjectImplementerSet(immOiHandle, &rdn, SA_IMM_ONE),
		  SA_AIS_OK);
	test_validate(immutil_saImmOiObjectImplementerRelease(-1, &rdn, SA_IMM_ONE),
		      SA_AIS_ERR_BAD_HANDLE);
	safassert(
	    immutil_saImmOiObjectImplementerRelease(immOiHandle, &rdn, SA_IMM_ONE),
	    SA_AIS_OK);
	safassert(immutil_saImmOiFinalize(immOiHandle), SA_AIS_OK);

	safassert(immutil_saImmOmCcbObjectDelete(ccbHandle, &rdn), SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmCcbFinalize(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOiObjectImplementerRelease_03(void)
{
	const SaImmOiImplementerNameT implementerName =
	    (SaImmOiImplementerNameT) __FUNCTION__;
	SaNameT nonExistentRdn = {
	    .value = "XXX", .length = sizeof("XXX"),
	};
	const SaImmAdminOwnerNameT adminOwnerName =
	    (SaImmAdminOwnerNameT) __FUNCTION__;
	SaImmAdminOwnerHandleT ownerHandle;
	SaImmCcbHandleT ccbHandle;
	const SaNameT *nameValues[] = {&rdn};
	SaImmAttrValuesT_2 v2 = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				 (void **)nameValues};
	SaUint32T int1Value1 = 7;
	SaUint32T *int1Values[] = {&int1Value1};
	SaImmAttrValuesT_2 v1 = {"attr1", SA_IMM_ATTR_SAUINT32T, 1,
				 (void **)int1Values};
	const SaImmAttrValuesT_2 *attrValues[] = {&v1, &v2, NULL};
	safassert(immutil_saImmOmInitialize(&immOmHandle, NULL, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_TRUE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbInitialize(ownerHandle, 0, &ccbHandle), SA_AIS_OK);
	/* Create test object under root */
	safassert(immutil_saImmOmCcbObjectCreate_2(ccbHandle, "TestClassConfig", NULL,
					   attrValues),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	safassert(
	    immutil_saImmOiInitialize_2(&immOiHandle, &immOiCallbacks, &immVersion),
	    SA_AIS_OK);
	safassert(immutil_saImmOiImplementerSet(immOiHandle, implementerName),
		  SA_AIS_OK);
	safassert(immutil_saImmOiObjectImplementerSet(immOiHandle, &rdn, SA_IMM_ONE),
		  SA_AIS_OK);
	test_validate(immutil_saImmOiObjectImplementerRelease(
			  immOiHandle, &nonExistentRdn, SA_IMM_ONE),
		      SA_AIS_ERR_NOT_EXIST);
	safassert(
	    immutil_saImmOiObjectImplementerRelease(immOiHandle, &rdn, SA_IMM_ONE),
	    SA_AIS_OK);
	safassert(immutil_saImmOiFinalize(immOiHandle), SA_AIS_OK);

	safassert(immutil_saImmOmCcbObjectDelete(ccbHandle, &rdn), SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmCcbFinalize(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOiObjectImplementerRelease_04(void)
{
	const SaImmOiImplementerNameT implementerName =
	    (SaImmOiImplementerNameT) __FUNCTION__;
	const SaImmAdminOwnerNameT adminOwnerName =
	    (SaImmAdminOwnerNameT) __FUNCTION__;
	SaImmAdminOwnerHandleT ownerHandle;
	SaImmCcbHandleT ccbHandle;
	const SaNameT *nameValues[] = {&rdn};
	SaImmAttrValuesT_2 v2 = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				 (void **)nameValues};
	SaUint32T int1Value1 = 7;
	SaUint32T *int1Values[] = {&int1Value1};
	SaImmAttrValuesT_2 v1 = {"attr1", SA_IMM_ATTR_SAUINT32T, 1,
				 (void **)int1Values};
	const SaImmAttrValuesT_2 *attrValues[] = {&v1, &v2, NULL};
	safassert(immutil_saImmOmInitialize(&immOmHandle, NULL, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_TRUE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbInitialize(ownerHandle, 0, &ccbHandle), SA_AIS_OK);
	/* Create test object under root */
	safassert(immutil_saImmOmCcbObjectCreate_2(ccbHandle, "TestClassConfig", NULL,
					   attrValues),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	safassert(
	    immutil_saImmOiInitialize_2(&immOiHandle, &immOiCallbacks, &immVersion),
	    SA_AIS_OK);
	safassert(immutil_saImmOiImplementerSet(immOiHandle, implementerName),
		  SA_AIS_OK);
	safassert(immutil_saImmOiObjectImplementerSet(immOiHandle, &rdn, SA_IMM_ONE),
		  SA_AIS_OK);
	test_validate(immutil_saImmOiObjectImplementerRelease(immOiHandle, &rdn, -1),
		      SA_AIS_ERR_INVALID_PARAM);
	safassert(
	    immutil_saImmOiObjectImplementerRelease(immOiHandle, &rdn, SA_IMM_ONE),
	    SA_AIS_OK);
	safassert(immutil_saImmOiFinalize(immOiHandle), SA_AIS_OK);

	safassert(immutil_saImmOmCcbObjectDelete(ccbHandle, &rdn), SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmCcbFinalize(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}
