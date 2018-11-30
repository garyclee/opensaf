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

void saImmOmClassCreate_2_01(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_02(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_03(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(-1, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_04(void)
{
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, "", SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	if (rc == SA_AIS_OK)
		/* an error? */
		safassert(immutil_saImmOmClassDelete(immOmHandle, ""), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_05(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_06(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", -1, SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_07(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	/* mismatch classCategory vs attribute type */
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_08(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	/* mismatch classCategory vs attribute type */
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_09(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	/* SA_IMM_ATTR_CACHED missing */
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_10(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	SaImmAttrDefinitionT_2 attr2 = {"config_attr", SA_IMM_ATTR_SAUINT32T,
					SA_IMM_ATTR_CONFIG, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, &attr2,
							   NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	/* config attributes in runtime class */
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_11(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	/* Illegal classCategory */
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, 99, attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_12(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

#define OPENSAF_IMM_OBJECT_DN "opensafImm=opensafImm,safApp=safImmService"
#define OPENSAF_IMM_ATTR_NOSTD_FLAGS "opensafImmNostdFlags"
#define OPENSAF_IMM_FLAG_SCHCH_ALLOW 0x00000001

void saImmOmClassCreate_2_13(void)
{
	SaUint32T noStdFlags = 0;
	SaImmAccessorHandleT accessorHandle;
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	const SaImmAttrNameT attName = (char *)OPENSAF_IMM_ATTR_NOSTD_FLAGS;
	SaNameT myObj;
	strcpy((char *)myObj.value, OPENSAF_IMM_OBJECT_DN);
	myObj.length = strlen((const char *)myObj.value);
	SaImmAttrNameT attNames[] = {attName, NULL};
	SaImmAttrValuesT_2 **resultAttrs;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	SaImmAttrDefinitionT_2 attr2 = {"attr2", SA_IMM_ATTR_SASTRINGT,
					SA_IMM_ATTR_CONFIG, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions2[] = {&attr1, &attr2,
							    NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAccessorInitialize(immOmHandle, &accessorHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAccessorGet_2(accessorHandle, &myObj, attNames,
				       &resultAttrs),
		  SA_AIS_OK);
	assert(resultAttrs[0] &&
	       (resultAttrs[0]->attrValueType == SA_IMM_ATTR_SAUINT32T));
	if (resultAttrs[0]->attrValuesNumber == 1) {
		noStdFlags = *((SaUint32T *)resultAttrs[0]->attrValues[0]);
	}

	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions2);
	if (noStdFlags & OPENSAF_IMM_FLAG_SCHCH_ALLOW) {
		test_validate(rc, SA_AIS_OK);
	} else {
		if (rc != SA_AIS_OK) {
			test_validate(rc, SA_AIS_ERR_EXIST);
		} else {
			test_validate(rc, SA_AIS_OK);
		}
	}
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_14(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	SaImmAttrDefinitionT_2 attr2 = {
	    "notified_rta", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_NOTIFY, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, &attr2,
							   NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	/* config attributes in runtime class */
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	// safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_15(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	SaImmAttrDefinitionT_2 attr2 = {
	    "nodupnonmulti_rta", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_NO_DUPLICATES, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, &attr2,
							   NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	/* config attributes in runtime class */
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	// safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_16(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	SaImmAttrDefinitionT_2 attr2 = {
	    "attr1", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE | SA_IMM_ATTR_NO_DANGLING,
	    NULL};
	SaImmAttrDefinitionT_2 attr3 = {
	    "attr2", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_MULTI_VALUE | SA_IMM_ATTR_NO_DANGLING,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, &attr2,
							   &attr3, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_17(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	SaImmAttrDefinitionT_2 attr2 = {"attr3", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_RUNTIME |
					    SA_IMM_ATTR_PERSISTENT |
					    SA_IMM_ATTR_NO_DANGLING,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, &attr2,
							   NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_18(void)
{
	/*
	 * Create a class that has STRONG_DEFAULT flag.
	 */
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 attr = {"attr", SA_IMM_ATTR_SAUINT32T,
				       SA_IMM_ATTR_CONFIG |
					   SA_IMM_ATTR_WRITABLE |
					   SA_IMM_ATTR_STRONG_DEFAULT,
				       &defaultVal};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, &attr, NULL};

	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_2_19(void)
{
	/*
	 * Create a class that has STRONG_DEFAULT flag without having default
	 * value.
	 */
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaImmAttrDefinitionT_2 attr = {"attr", SA_IMM_ATTR_SAUINT32T,
				       SA_IMM_ATTR_CONFIG |
					   SA_IMM_ATTR_WRITABLE |
					   SA_IMM_ATTR_STRONG_DEFAULT,
				       NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, &attr, NULL};

	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmClassCreate_2() API:
 * Test: Creating a config class with invalid handle
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:Now call the saImmOmClassCreate_2() with -1
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmClassCreate_2_with_invalid_handle_with_config_class(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(-1, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

/* Object to test: saImmOmClassCreate_2() API:
 * Test: Creating a config class with uninitialized handle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmClassCreate_2_uninitialized_handle_with_config_class(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/* Object to test: saImmOmClassCreate_2() API:
 * Test: Creating a runtime class with uninitialized handle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmClassCreate_2_with_uninitialized_handle_with_runtime_class(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}


/* Object to test: saImmOmClassCreate_2() API:
 * Test: Creating a config class with invalid handle
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:Call saImmOmFinalize()
 * step3:Now call the saImmOmClassCreate_2() with immOmHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmClassCreate_2_with_finalized_handle_config_class(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	immutil_saImmOmFinalize(immOmHandle);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}


/* Object to test: saImmOmClassCreate_2() API:
 * Test: Creating a runtime class with invalid handle
 * step1:Call saImmOmInitialize() API and it returns SA_AIS_OK
 * step2:Call saImmOmFinalize()
 * step3:Now call the saImmOmClassCreate_2() with immOmHandle
 * Result: Shall fail with return code SA_AIS_ERR_BAD_HANDLE
 */
void saImmOmClassCreate_2_with_finalized_handle_runtime_class(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	immutil_saImmOmFinalize(immOmHandle);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

/*
  Verify it is not allowed to create IMM object class with reserved name.
  NOTE: As the list of reserved class names is read from the environment
  variable IMMSV_RESERVED_CLASS_NAMES which is defined in immnd.conf file,
  these 02 below test cases could fail if "objects" or "classes" name do
  not exist in the list.
 */
void saImmOmClassCreate_with_reserved_name_01(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) "objects";
	SaImmAttrDefinitionT_2 attr1 = {"rdn", SA_IMM_ATTR_SANAMET,
					SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
					NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_with_reserved_name_02(void)
{
	const SaImmClassNameT className = (SaImmClassNameT) "classes";
	SaImmAttrDefinitionT_2 attr1 = {
		"rdn", SA_IMM_ATTR_SANAMET,
		SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED,
		NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

#define OPENSAF_IMM_NOSTD_FLAG_PARAM "opensafImmNostdFlags"
#define OPENSAF_IMM_NOSTD_FLAG_ON 1
#define OPENSAF_IMM_NOSTD_FLAG_OFF 2
#define OPENSAF_IMM_IMMSV_ADMO "safImmService"

static int enableSchemaChange()
{
	/*
	 * If schema change is already enabled, do nothing and return 1
	 * If schema change is disabled, enable it and return 0
	 */
	int rc = 0;
	SaNameT objectName;
	objectName.length = strlen(OPENSAF_IMM_OBJECT_DN);
	strncpy((char *)objectName.value, OPENSAF_IMM_OBJECT_DN,
		objectName.length);
	SaImmAccessorHandleT accessorHandle;
	const SaImmAttrNameT attName = (char *)OPENSAF_IMM_ATTR_NOSTD_FLAGS;
	SaImmAttrNameT attNames[] = {attName, NULL};
	SaImmAttrValuesT_2 **resultAttrs;

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAccessorInitialize(immOmHandle, &accessorHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAccessorGet_2(accessorHandle, &objectName, attNames,
				       &resultAttrs),
		  SA_AIS_OK);
	assert(resultAttrs[0] &&
	       (resultAttrs[0]->attrValueType == SA_IMM_ATTR_SAUINT32T));

	/* When PBE is not enabled (attrValuesNumber == 0),
	 * assume that schema change is not enabled */
	if (resultAttrs[0]->attrValuesNumber == 1 &&
	    (*((SaUint32T *)resultAttrs[0]->attrValues[0]) &
	     OPENSAF_IMM_FLAG_SCHCH_ALLOW)) {
		rc = 1;
		goto done;
	}

	SaImmAdminOwnerHandleT ownerHandle;
	const SaImmAdminOwnerNameT adminOwnerName = OPENSAF_IMM_IMMSV_ADMO;
	const SaNameT *objectNames[] = {&objectName, NULL};
	SaStringT paramName = OPENSAF_IMM_NOSTD_FLAG_PARAM;
	SaUint32T paramValue = OPENSAF_IMM_FLAG_SCHCH_ALLOW;
	SaImmAdminOperationParamsT_2 param = {paramName, SA_IMM_ATTR_SAUINT32T,
					      &paramValue};
	const SaImmAdminOperationParamsT_2 *params[] = {&param, NULL};
	SaAisErrorT operation_return_value;

	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_FALSE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerSet(ownerHandle, objectNames, SA_IMM_ONE),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOperationInvoke_2(
		      ownerHandle, &objectName, 1, OPENSAF_IMM_NOSTD_FLAG_ON,
		      params, &operation_return_value, SA_TIME_ONE_MINUTE),
		  SA_AIS_OK);

done:
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	return rc;
}

static void disableSchemaChange()
{
	SaNameT objectName;
	objectName.length = strlen(OPENSAF_IMM_OBJECT_DN);
	strncpy((char *)objectName.value, OPENSAF_IMM_OBJECT_DN,
		objectName.length);
	SaImmAdminOwnerHandleT ownerHandle;
	const SaImmAdminOwnerNameT adminOwnerName = OPENSAF_IMM_IMMSV_ADMO;
	const SaNameT *objectNames[] = {&objectName, NULL};
	SaStringT paramName = OPENSAF_IMM_NOSTD_FLAG_PARAM;
	SaUint32T paramValue = OPENSAF_IMM_FLAG_SCHCH_ALLOW;
	SaImmAdminOperationParamsT_2 param = {paramName, SA_IMM_ATTR_SAUINT32T,
					      &paramValue};
	const SaImmAdminOperationParamsT_2 *params[] = {&param, NULL};
	SaAisErrorT operation_return_value;

	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_FALSE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerSet(ownerHandle, objectNames, SA_IMM_ONE),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOperationInvoke_2(
		      ownerHandle, &objectName, 1, OPENSAF_IMM_NOSTD_FLAG_OFF,
		      params, &operation_return_value, SA_TIME_ONE_MINUTE),
		  SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
}

void saImmOmClassCreate_SchemaChange_2_01(void)
{
	/*
	 * [CONFIG_CLASS] Remove default value by adding DEFAULT_REMOVED flag
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_02(void)
{
	/*
	 * [CONFIG_CLASS] Add default value to default-removed attribute
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &hasDefault;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_03(void)
{
	/*
	 * [CONFIG_CLASS] Remove default value from attribute that doesn't have
	 * default value
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaImmAttrDefinitionT_2 noDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, NULL};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &noDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_04(void)
{
	/*
	 * [CONFIG_CLASS] Remove DEFAULT_REMOVED flag without adding default
	 * value
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaImmAttrDefinitionT_2 noDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &noDefault;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_05(void)
{
	/*
	 * [CONFIG_CLASS] Add new attribute with DEFAULT_REMOVED flag to a class
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_06(void)
{
	/*
	 * [CONFIG_CLASS] Create new class with DEFAULT_REMOVED flag
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {
	    &rdn, &defaultRemoved, NULL};

	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_07(void)
{
	/*
	 * [CONFIG_CLASS] Set value of default-removed attribute to empty when
	 * creating an object
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);

	SaImmAdminOwnerHandleT ownerHandle;
	const SaImmAdminOwnerNameT adminOwnerName =
	    (SaImmAdminOwnerNameT) __FUNCTION__;
	SaImmCcbHandleT ccbHandle;
	SaNameT objectName;
	objectName.length = strlen(__FUNCTION__);
	strncpy((char *)objectName.value, __FUNCTION__, objectName.length);
	SaNameT *nameValues[] = {&objectName};
	SaImmAttrValuesT_2 rdnValue = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				       (void **)nameValues};
	const SaImmAttrValuesT_2 *attrValues[] = {&rdnValue, NULL};
	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_TRUE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbInitialize(ownerHandle, 0, &ccbHandle), SA_AIS_OK);

	/* Create object */
	safassert(
	    immutil_saImmOmCcbObjectCreate_2(ccbHandle, className, NULL, attrValues),
	    SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	/* Check value of the attribute */
	SaImmAccessorHandleT accessorHandle;
	const SaImmAttrNameT attName = "attr";
	SaImmAttrNameT attNames[] = {attName, NULL};
	SaImmAttrValuesT_2 **resultAttrs;
	safassert(immutil_saImmOmAccessorInitialize(immOmHandle, &accessorHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAccessorGet_2(accessorHandle, &objectName, attNames,
				       &resultAttrs),
		  SA_AIS_OK);
	assert(resultAttrs[0] &&
	       (resultAttrs[0]->attrValueType == SA_IMM_ATTR_SAUINT32T));
	test_validate(resultAttrs[0]->attrValuesNumber, 0);

	/* Delete Object */
	safassert(immutil_saImmOmCcbObjectDelete(ccbHandle, &objectName), SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	safassert(immutil_saImmOmCcbFinalize(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerFinalize(ownerHandle), SA_AIS_OK);

	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_08(void)
{
	/*
	 * [CONFIG_CLASS] Set value of default-restored attribute to empty when
	 * creating an object
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);

	SaImmAdminOwnerHandleT ownerHandle;
	const SaImmAdminOwnerNameT adminOwnerName =
	    (SaImmAdminOwnerNameT) __FUNCTION__;
	SaImmCcbHandleT ccbHandle;
	SaNameT objectName;
	objectName.length = strlen(__FUNCTION__);
	strncpy((char *)objectName.value, __FUNCTION__, objectName.length);
	SaNameT *nameValues[] = {&objectName};
	SaImmAttrValuesT_2 rdnValue = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				       (void **)nameValues};
	const SaImmAttrValuesT_2 *attrValues[] = {&rdnValue, NULL};
	safassert(immutil_saImmOmAdminOwnerInitialize(immOmHandle, adminOwnerName,
					      SA_TRUE, &ownerHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmCcbInitialize(ownerHandle, 0, &ccbHandle), SA_AIS_OK);

	/* Create object */
	safassert(
	    immutil_saImmOmCcbObjectCreate_2(ccbHandle, className, NULL, attrValues),
	    SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	/* Check value of the attribute */
	SaImmAccessorHandleT accessorHandle;
	const SaImmAttrNameT attName = "attr";
	SaImmAttrNameT attNames[] = {attName, NULL};
	SaImmAttrValuesT_2 **resultAttrs;
	safassert(immutil_saImmOmAccessorInitialize(immOmHandle, &accessorHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAccessorGet_2(accessorHandle, &objectName, attNames,
				       &resultAttrs),
		  SA_AIS_OK);
	assert(resultAttrs[0] &&
	       (resultAttrs[0]->attrValueType == SA_IMM_ATTR_SAUINT32T));
	assert(resultAttrs[0]->attrValuesNumber == 1);
	test_validate(*((SaUint32T *)resultAttrs[0]->attrValues[0]),
		      defaultVal);

	/* Delete Object */
	safassert(immutil_saImmOmCcbObjectDelete(ccbHandle, &objectName), SA_AIS_OK);
	safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	safassert(immutil_saImmOmCcbFinalize(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerFinalize(ownerHandle), SA_AIS_OK);

	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_09(void)
{
	/*
	 * [RUNTIME_CLASS] Remove default value by adding DEFAULT_REMOVED flag
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_10(void)
{
	/*
	 * [RUNTIME_CLASS] Add default value to default-removed attribute
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &hasDefault;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_11(void)
{
	/*
	 * [RUNTIME_CLASS] Remove default value from attribute that doesn't have
	 * default value
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaImmAttrDefinitionT_2 noDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED, NULL};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &noDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_12(void)
{
	/*
	 * [RUNTIME_CLASS] Remove DEFAULT_REMOVED flag without adding default
	 * value
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaImmAttrDefinitionT_2 noDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED, NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &noDefault;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_13(void)
{
	/*
	 * [RUNTIME_CLASS] Add new attribute with DEFAULT_REMOVED flag to a
	 * class
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_14(void)
{
	/*
	 * [RUNTIME_CLASS] Create new class with DEFAULT_REMOVED flag
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {
	    &rdn, &defaultRemoved, NULL};

	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_RUNTIME,
				  attrDefinitions);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_15(void)
{
	/*
	 * [RUNTIME_CLASS] Set value of default-removed attribute to empty when
	 * creating an object
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);

	SaImmOiImplementerNameT implementerName =
	    (SaImmOiImplementerNameT) __FUNCTION__;
	SaNameT objectName;
	objectName.length = strlen(__FUNCTION__);
	strncpy((char *)objectName.value, __FUNCTION__, objectName.length);
	SaNameT *nameValues[] = {&objectName};
	SaImmAttrValuesT_2 rdnValue = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				       (void **)nameValues};
	const SaImmAttrValuesT_2 *attrValues[] = {&rdnValue, NULL};
	safassert(
	    immutil_saImmOiInitialize_2(&immOiHandle, &immOiCallbacks, &immVersion),
	    SA_AIS_OK);
	safassert(immutil_saImmOiImplementerSet(immOiHandle, implementerName),
		  SA_AIS_OK);

	/* Create object */
	rc = immutil_saImmOiRtObjectCreate_2(immOiHandle, className, NULL, attrValues);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);

	safassert(immutil_saImmOiImplementerClear(immOiHandle), SA_AIS_OK);
	safassert(immutil_saImmOiFinalize(immOiHandle), SA_AIS_OK);

	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_16(void)
{
	/*
	 * [RUNTIME_CLASS] Set value of default-restored attribute to empty when
	 * creating an object
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED | SA_IMM_ATTR_RDN, NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 hasDefault = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED, &defaultVal};
	SaImmAttrDefinitionT_2 defaultRemoved = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_CACHED |
		SA_IMM_ATTR_DEFAULT_REMOVED,
	    NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &defaultRemoved;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &hasDefault;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_OK);

	SaImmOiImplementerNameT implementerName =
	    (SaImmOiImplementerNameT) __FUNCTION__;
	SaNameT objectName;
	objectName.length = strlen(__FUNCTION__);
	strncpy((char *)objectName.value, __FUNCTION__, objectName.length);
	SaNameT *nameValues[] = {&objectName};
	SaImmAttrValuesT_2 rdnValue = {"rdn", SA_IMM_ATTR_SANAMET, 1,
				       (void **)nameValues};
	const SaImmAttrValuesT_2 *attrValues[] = {&rdnValue, NULL};
	safassert(
	    immutil_saImmOiInitialize_2(&immOiHandle, &immOiCallbacks, &immVersion),
	    SA_AIS_OK);
	safassert(immutil_saImmOiImplementerSet(immOiHandle, implementerName),
		  SA_AIS_OK);

	/* Create object */
	safassert(
	    immutil_saImmOiRtObjectCreate_2(immOiHandle, className, NULL, attrValues),
	    SA_AIS_OK);

	/* Check value of the attribute
	   There's no pure runtime attributes so there will be no deadlock here
	 */
	SaImmAccessorHandleT accessorHandle;
	const SaImmAttrNameT attName = "attr";
	SaImmAttrNameT attNames[] = {attName, NULL};
	SaImmAttrValuesT_2 **resultAttrs;
	safassert(immutil_saImmOmAccessorInitialize(immOmHandle, &accessorHandle),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAccessorGet_2(accessorHandle, &objectName, attNames,
				       &resultAttrs),
		  SA_AIS_OK);
	assert(resultAttrs[0] &&
	       (resultAttrs[0]->attrValueType == SA_IMM_ATTR_SAUINT32T));
	assert(resultAttrs[0]->attrValuesNumber == 1);
	test_validate(*((SaUint32T *)resultAttrs[0]->attrValues[0]),
		      defaultVal);

	/* Delete Object */
	safassert(immutil_saImmOiRtObjectDelete(immOiHandle, &objectName), SA_AIS_OK);

	safassert(immutil_saImmOiImplementerClear(immOiHandle), SA_AIS_OK);
	safassert(immutil_saImmOiFinalize(immOiHandle), SA_AIS_OK);

	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_17(void)
{
	/*
	 * Add STRONG_DEFAULT flag to an attribute
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 weak = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, &defaultVal};
	SaImmAttrDefinitionT_2 strong = {"attr", SA_IMM_ATTR_SAUINT32T,
					 SA_IMM_ATTR_CONFIG |
					     SA_IMM_ATTR_WRITABLE |
					     SA_IMM_ATTR_STRONG_DEFAULT,
					 &defaultVal};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &weak;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &strong;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

void saImmOmClassCreate_SchemaChange_2_18(void)
{
	/*
	 * Remove STRONG_DEFAULT flag from an attribute
	 */
	int schemaChangeEnabled = enableSchemaChange();
	safassert(immutil_saImmOmInitialize(&immOmHandle, &immOmCallbacks, &immVersion),
		  SA_AIS_OK);

	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 rdn = {"rdn", SA_IMM_ATTR_SANAMET,
				      SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_RDN,
				      NULL};
	SaUint32T defaultVal = 100;
	SaImmAttrDefinitionT_2 weak = {
	    "attr", SA_IMM_ATTR_SAUINT32T,
	    SA_IMM_ATTR_CONFIG | SA_IMM_ATTR_WRITABLE, &defaultVal};
	SaImmAttrDefinitionT_2 strong = {"attr", SA_IMM_ATTR_SAUINT32T,
					 SA_IMM_ATTR_CONFIG |
					     SA_IMM_ATTR_WRITABLE |
					     SA_IMM_ATTR_STRONG_DEFAULT,
					 &defaultVal};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&rdn, NULL, NULL};

	attrDefinitions[1] = &strong;
	safassert(immutil_saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_CONFIG, attrDefinitions),
		  SA_AIS_OK);
	attrDefinitions[1] = &weak;
	rc = immutil_saImmOmClassCreate_2(immOmHandle, className, SA_IMM_CLASS_CONFIG,
				  attrDefinitions);
	test_validate(rc, SA_AIS_OK);
	safassert(immutil_saImmOmClassDelete(immOmHandle, className), SA_AIS_OK);

	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	if (!schemaChangeEnabled)
		disableSchemaChange();
}

extern void saImmOmClassDescriptionGet_2_01(void);
extern void saImmOmClassDescriptionGet_2_02(void);
extern void saImmOmClassDescriptionGet_2_03(void);
extern void saImmOmClassDescriptionGet_2_04(void);
extern void saImmOmClassDescriptionGet_2_05(void);
extern void saImmOmClassDescriptionGet_2_with_className_as_null(void);
extern void saImmOmClassDescriptionMemoryFree_2_01(void);
extern void saImmOmClassDescriptionMemoryFree_2_02(void);

extern void saImmOmClassDelete_2_01(void);
extern void saImmOmClassDelete_2_02(void);
extern void saImmOmClassDelete_2_03(void);
extern void saImmOmClassDelete_2_04(void);
extern void saImmOmClassDelete_2_with_deleted_class(void);

__attribute__((constructor)) static void saImmOmInitialize_constructor(void)
{
	test_suite_add(2, "Object Class Management");
	test_case_add(2, saImmOmClassCreate_2_01,
		      "saImmOmClassCreate_2 - SA_AIS_OK - CONFIG");
	test_case_add(2, saImmOmClassCreate_2_02,
		      "saImmOmClassCreate_2 - SA_AIS_OK - RUNTIME");
	test_case_add(2, saImmOmClassCreate_2_03,
		      "saImmOmClassCreate_2 - SA_AIS_ERR_BAD_HANDLE");
	test_case_add(
	    2, saImmOmClassCreate_2_04,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, className==\"\"");
	test_case_add(
	    2, saImmOmClassCreate_2_05,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, zero length attr name");
	test_case_add(
	    2, saImmOmClassCreate_2_06,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, invalid value type");
	test_case_add(
	    2, saImmOmClassCreate_2_07,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, inconsistent attr flags, mismatch classCategory vs attribute type");
	test_case_add(
	    2, saImmOmClassCreate_2_08,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, inconsistent attr flags, mismatch classCategory vs attribute type");
	test_case_add(
	    2, saImmOmClassCreate_2_09,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, inconsistent attr flags, SA_IMM_ATTR_CACHED missing for runtime class");
	test_case_add(
	    2, saImmOmClassCreate_2_10,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, inconsistent attr flags, config attributes in runtime class");
	test_case_add(
	    2, saImmOmClassCreate_2_11,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, Invalid classCategory");
	test_case_add(
	    2, saImmOmClassCreate_2_12,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_EXIST, className already exist");
	test_case_add(
	    2, saImmOmClassCreate_2_14,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, flag SA_IMM_ATTR_NOTIFY not allowed on pure RTAs");
	test_case_add(
	    2, saImmOmClassCreate_2_15,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, flag SA_IMM_ATTR_NO_DUPLICATES only allowed on multivalued");
	test_case_add(
	    2, saImmOmClassCreate_2_16,
	    "saImmOmClassCreate_2 - SA_AIS_OK, flag SA_IMM_ATTR_NO_DANGLING");
	test_case_add(
	    2, saImmOmClassCreate_2_17,
	    "saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM, flag SA_IMM_ATTR_NO_DANGLING for PRTA");
	test_case_add(
	    2, saImmOmClassCreate_2_18,
	    "saImmOmClassCreate_2 - SA_AIS_OK, Create a class that has STRONG_DEFAULT flag");
	test_case_add(
	    2, saImmOmClassCreate_2_19,
	    "saImmOmClassCreate_2 - SA_AIS_OK, Create a class that has STRONG_DEFAULT flag without having default value");
	test_case_add(2, saImmOmClassCreate_2_with_invalid_handle_with_config_class,
		      "saImmOmClassCreate_2 - SA_AIS_ERR_BAD_HANDLE, create config"
		      " class with invalid handle");
	test_case_add(2, saImmOmClassCreate_2_uninitialized_handle_with_config_class,
		      "saImmOmClassCreate_2 - SA_AIS_ERR_BAD_HANDLE, create config"
		      " class with uninitialized handle");
	test_case_add(2, saImmOmClassCreate_2_with_uninitialized_handle_with_runtime_class,
		      "saImmOmClassCreate_2 - SA_AIS_ERR_BAD_HANDLE, create runtime"
		      "  class with uninitialized handle ");
	test_case_add(2, saImmOmClassCreate_2_with_finalized_handle_config_class,
		      "saImmOmClassCreate_2 - SA_AIS_ERR_BAD_HANDLE, create config"
		      " class with finalized handle");
	test_case_add(2, saImmOmClassCreate_2_with_finalized_handle_runtime_class,
		      "saImmOmClassCreate_2 - SA_AIS_ERR_BAD_HANDLE, create runtime"
		      " class with finalized handle");

	test_case_add(
		2, saImmOmClassCreate_with_reserved_name_01,
		"saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM,"
		"Create a config class with reserved name");
	test_case_add(
		2, saImmOmClassCreate_with_reserved_name_02,
		"saImmOmClassCreate_2 - SA_AIS_ERR_INVALID_PARAM,"
		" Create a rt class with reserved name");

	test_case_add(2, saImmOmClassDescriptionGet_2_01,
		      "saImmOmClassDescriptionGet_2 - SA_AIS_OK");
	test_case_add(2, saImmOmClassDescriptionGet_2_02,
		      "saImmOmClassDescriptionGet_2 - SA_AIS_ERR_BAD_HANDLE");
	test_case_add(
	    2, saImmOmClassDescriptionGet_2_03,
	    "saImmOmClassDescriptionGet_2 - SA_AIS_ERR_NOT_EXIST, className does not exist");
	test_case_add(
	    2, saImmOmClassDescriptionGet_2_04,
	    "saImmOmClassDescriptionGet_2 - SA_AIS_OK, Fetch includes SA_IMM_ATTR_NOTIFY");
	test_case_add(
	    2, saImmOmClassDescriptionGet_2_05,
	    "saImmOmClassDescriptionGet_2 - SA_AIS_OK, Fetch includes SA_IMM_ATTR_NO_DANGLING");
	test_case_add(2, saImmOmClassDescriptionGet_2_with_className_as_null,
		      "saImmOmClassDescriptionGet_2 - SA_AIS_ERR_INVALID_PARAM"
		      "Get class descriptor of an invalid class name");

	test_case_add(2, saImmOmClassDescriptionMemoryFree_2_01,
		      "saImmOmClassDescriptionMemoryFree_2 - SA_AIS_OK");
	test_case_add(
	    2, saImmOmClassDescriptionMemoryFree_2_02,
	    "saImmOmClassDescriptionMemoryFree_2 - SA_AIS_ERR_BAD_HANDLE");

	test_case_add(2, saImmOmClassDelete_2_01,
		      "saImmOmClassDelete_2 - SA_AIS_OK");
	test_case_add(2, saImmOmClassDelete_2_02,
		      "saImmOmClassDelete_2 - SA_AIS_ERR_BAD_HANDLE");
	test_case_add(
	    2, saImmOmClassDelete_2_03,
	    "saImmOmClassDelete_2 - SA_AIS_ERR_NOT_EXIST, className does not exist");
	test_case_add(
	    2, saImmOmClassCreate_2_13,
	    "saImmOmClassCreate_2 UPGRADE - SA_AIS_OK/SA_AIS_ERR_EXIST, Added attribute to class");
	test_case_add(
	    2, saImmOmClassDelete_2_04,
	    "saImmOmClassDelete_2  - SA_AIS_ERR_INVALID_PARAM, Empty classname");
	test_case_add(
	    2, saImmOmClassDelete_2_with_deleted_class,
	    "saImmOmClassDelete_2 - SA_AIS_ERR_NOT_EXIST "
	    "Double delete class name");

	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_01,
	    "SchemaChange - SA_AIS_OK, Remove default value by adding DEFAULT_REMOVED flag (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_02,
	    "SchemaChange - SA_AIS_OK, Add default value to default-removed attribute (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_03,
	    "SchemaChange - SA_AIS_ERR_EXIST, Remove default value from attribute that doesn't have default value (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_04,
	    "SchemaChange - SA_AIS_ERR_EXIST, Remove DEFAULT_REMOVED flag without adding default value (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_05,
	    "SchemaChange - SA_AIS_ERR_EXIST, Add new attribute with DEFAULT_REMOVED flag to a class (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_06,
	    "SchemaChange - SA_AIS_ERR_INVALID_PARAM, Create new class with DEFAULT_REMOVED flag (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_07,
	    "SchemaChange - Set value of default-removed attribute to empty when creating an object (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_08,
	    "SchemaChange - Set value of default-restored attribute to empty when creating an object (Config class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_09,
	    "SchemaChange - SA_AIS_OK, Remove default value by adding DEFAULT_REMOVED flag (Runtime class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_10,
	    "SchemaChange - SA_AIS_OK, Add default value to default-removed attribute (Runtime class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_11,
	    "SchemaChange - SA_AIS_ERR_EXIST, Remove default value from attribute that doesn't have default value (Runtime class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_12,
	    "SchemaChange - SA_AIS_ERR_EXIST, Remove DEFAULT_REMOVED flag without adding default value (Runtime class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_13,
	    "SchemaChange - SA_AIS_ERR_EXIST, Add new attribute with DEFAULT_REMOVED flag to a class (Runtime class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_14,
	    "SchemaChange - SA_AIS_ERR_INVALID_PARAM, Create new class with DEFAULT_REMOVED flag (Runtime class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_15,
	    "SchemaChange - SA_AIS_ERR_INVALID_PARAM, Set value of default-removed attribute to empty when creating an object (Runtime class)");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_16,
	    "SchemaChange - Set value of default-restored attribute to empty when creating an object (Runtime class)");

	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_17,
	    "SchemaChange - SA_AIS_OK, Add STRONG_DEFAULT flag to an attribute");
	test_case_add(
	    2, saImmOmClassCreate_SchemaChange_2_18,
	    "SchemaChange - SA_AIS_OK, Remove STRONG_DEFAULT flag from an attribute");
}
