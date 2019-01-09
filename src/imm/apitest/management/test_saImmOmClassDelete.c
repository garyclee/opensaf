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

void saImmOmClassDelete_2_01(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassDelete(immOmHandle, className);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassDelete_2_02(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassDelete(-1, className);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassDelete_2_03(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassDelete(immOmHandle, className);
	test_validate(rc, SA_AIS_ERR_NOT_EXIST);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassDelete_2_04(void)
{
	const SaImmClassNameT className =
	    (SaImmClassNameT) ""; /* The empty classname */

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassDelete(immOmHandle, className);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmClassDelete() API:
 * Test: deleting the class which is already deleted
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:Call saImmOmClassCreate_2()
 * step3:Now call the saImmOmClassDelete()
 * step4:Now again call this saImmOmClassDelete()
 * Result: Shall fail with return code SA_AIS_ERR_NOT_EXIST
 */
void saImmOmClassDelete_2_with_deleted_class(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
	SA_AIS_OK);
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	immutil_saImmOmClassDelete(immOmHandle, className);
	rc = immutil_saImmOmClassDelete(immOmHandle, className);
	test_validate(rc, SA_AIS_ERR_NOT_EXIST);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}
