/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2021 The OpenSAF Foundation
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

#include "base/os_defs.h"
#include "imm/agent/imma_def.h"
#include "imm/apitest/immtest.h"
#include "osaf/configmake.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int get_pid_immnd()
{
	FILE *f;
	int pid;
	const char *osafimmnd_pid_file = PKGPIDDIR "/osafimmnd.pid";
	if ((f = fopen(osafimmnd_pid_file, "r")) == NULL) {
		fprintf(stderr, "Failed to open %s\n", osafimmnd_pid_file);
		return -1;
	}

	if (fscanf(f, "%d", &pid) == 0) {
		fprintf(stderr, "Could not read PID from file %s\n",
			osafimmnd_pid_file);
		return -1;
	}

	if (fclose(f) != 0) {
		fprintf(stderr, "Could not close file\n");
		return -1;
	}
	return pid;
}

static bool suppend_immnd(int pid)
{
	char command[255] = {0};
	sprintf(command, "kill -STOP %d", pid);
	if (system(command) != EXIT_SUCCESS) {
		fprintf(stderr, "suppend osafimmnd failed\n");
		return false;
	}
	return true;
}

static bool resume_immnd(int pid)
{
	char command[255] = {0};
	sprintf(command, "kill -CONT %d", pid);
	if (system(command) != EXIT_SUCCESS) {
		fprintf(stderr, "resume osafimmnd failed\n");
		return false;
	}
	return true;
}

static bool is_non_root()
{
	if (getuid() == 0 || geteuid() == 0)
		return false;
	fprintf(stdout,
		"The test case need to run under root user. Skip test\n");
	test_validate(true, true);
	return true;
}

static int64_t estimate_timeout()
{
	const SaImmClassNameT className = (SaImmClassNameT) __FUNCTION__;
	SaImmAttrDefinitionT_2 attr1 = {
	    "rdn", SA_IMM_ATTR_SANAMET,
	    SA_IMM_ATTR_RUNTIME | SA_IMM_ATTR_RDN | SA_IMM_ATTR_CACHED, NULL};
	const SaImmAttrDefinitionT_2 *attrDefinitions[] = {&attr1, NULL};
	int64_t diff_time = -1;
	safassert(immutil_saImmOmInitialize(&immOmHandle, NULL, &immVersion),
		  SA_AIS_OK);

	int pid = get_pid_immnd();
	if (pid == -1) {
		goto fail_handle;
	}
	if (!suppend_immnd(pid)) {
		goto fail_handle;
	}

	int64_t start_time = ncs_os_time_ms();
	safassert(saImmOmClassCreate_2(immOmHandle, className,
				       SA_IMM_CLASS_RUNTIME, attrDefinitions),
		  SA_AIS_ERR_TIMEOUT);
	diff_time = ncs_os_time_ms() - start_time;
	safassert(resume_immnd(pid), true);

fail_handle:
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	return diff_time;
}

static SaAisErrorT update_syncr_timeout(SaTimeT timeout, bool estimate_timeout)
{
	const char mgmt_object[] = "safRdn=immManagement,safApp=safImmService";
	const SaImmAdminOwnerNameT adminOwnerName =
	    (SaImmAdminOwnerNameT) __FUNCTION__;
	SaImmAdminOwnerHandleT ownerHandle;
	SaImmCcbHandleT ccbHandle;
	SaNameT objectName;
	const SaNameT *objectNames[] = {&objectName, NULL};
	SaTimeT *saTimeValues[] = {&timeout};
	SaImmAttrValuesT_2 v1 = {"saImmSyncrTimeout", SA_IMM_ATTR_SATIMET, 1,
				 (void **)saTimeValues};
	SaImmAttrModificationT_2 attrMod = {SA_IMM_ATTR_VALUES_REPLACE, v1};
	const SaImmAttrModificationT_2 *attrMods[] = {&attrMod, NULL};
	SaAisErrorT rc = SA_AIS_OK;

	objectName.length = sizeof(mgmt_object);
	strncpy((char *)objectName.value, mgmt_object, objectName.length + 1);

	safassert(immutil_saImmOmInitialize(&immOmHandle, NULL, &immVersion),
		  SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerInitialize(
		      immOmHandle, adminOwnerName, SA_TRUE, &ownerHandle),
		  SA_AIS_OK);
	safassert(
	    immutil_saImmOmAdminOwnerSet(ownerHandle, objectNames, SA_IMM_ONE),
	    SA_AIS_OK);

	safassert(immutil_saImmOmCcbInitialize(ownerHandle, 0, &ccbHandle),
		  SA_AIS_OK);

	rc = immutil_saImmOmCcbObjectModify_2(ccbHandle, objectNames[0],
					      attrMods);
	if (rc == SA_AIS_OK)
		safassert(immutil_saImmOmCcbApply(ccbHandle), SA_AIS_OK);

	if (estimate_timeout) {
		int pid = get_pid_immnd();
		if (pid == -1) {
			goto fail_handle;
		}
		if (!suppend_immnd(pid)) {
			goto fail_handle;
		}
		int64_t start_time = ncs_os_time_ms();
		safassert(immutil_saImmOmCcbObjectModify_2(
			      ccbHandle, objectNames[0], attrMods),
			  SA_AIS_ERR_TIMEOUT);
		int64_t diff_time = ncs_os_time_ms() - start_time;
		rc_validate(diff_time / 1000, timeout / 100);
		safassert(resume_immnd(pid), true);
	}

fail_handle:
	safassert(immutil_saImmOmCcbFinalize(ccbHandle), SA_AIS_OK);
	safassert(immutil_saImmOmAdminOwnerFinalize(ownerHandle), SA_AIS_OK);
	safassert(immutil_saImmOmFinalize(immOmHandle), SA_AIS_OK);
	return rc;
}

static void restore_syncr_timeout()
{
	safassert(update_syncr_timeout(0, false), SA_AIS_OK);
}

/**
 * Verify valid value of attribute `saImmSyncrTimeout`
 *
 * Test: Trying to set an valid value for attribute `saImmSyncrTimeout`
 * Result: expect getting OK and IMM client get new timeout value.
 *
 */
void test_modify_syncr_timeout_success(void)
{
	if (is_non_root())
		return;
	if (unsetenv("IMMA_SYNCR_TIMEOUT") != 0) {
		fprintf(stderr,
			"Deletes a variable IMMA_SYNCR_TIMEOUT failed\n");
		test_validate(false, true);
		return;
	}

	safassert(update_syncr_timeout(300, true), SA_AIS_OK);
	restore_syncr_timeout();
}

/**
 * Verify invalid value of attribute `saImmSyncrTimeout`
 *
 * Test: Trying to set an invalid value for attribute `saImmSyncrTimeout`
 * Result: expect getting NOK.
 *
 */
void test_modify_syncr_timeout_failure(void)
{
	if (is_non_root())
		return;
	test_validate(update_syncr_timeout(5, false),
		      SA_AIS_ERR_BAD_OPERATION);
}

/**
 * This test case verify if IMM client get the new timeout when
 * IMM client startup after configuration.
 *
 * Test: set saImmSyncrTimeout=300
 * Result: expect MDS wait time equals to 3s
 *
 */
void test_modify_syncr_timeout_with_imm_client_restart(void)
{
	const int expected_timeout_second = 3;

	if (is_non_root())
		return;
	if (unsetenv("IMMA_SYNCR_TIMEOUT") != 0) {
		fprintf(stderr,
			"Deletes a variable IMMA_SYNCR_TIMEOUT failed\n");
		test_validate(false, true);
		return;
	}

	safassert(update_syncr_timeout(expected_timeout_second * 100, false),
		  SA_AIS_OK);
	int64_t timeout = estimate_timeout();
	rc_validate(timeout / 1000, expected_timeout_second);
	restore_syncr_timeout();
}

/**
 * Verify restore attibute with IMMA_SYNCR_TIMEOUT environment variable
 *
 * Test: set saImmSyncrTimeout=0 and IMMA_SYNCR_TIMEOUT=600
 * Result: expect MDS wait time equals to 6s
 *
 */
void test_restore_syncr_timeout_with_setenv(void)
{
	const char *expected_timeout = "600"; // in 10ms units => 6sec

	if (is_non_root())
		return;
	if (setenv("IMMA_SYNCR_TIMEOUT", expected_timeout, 1) != 0) {
		fprintf(stderr,
			"Add a variable IMMA_SYNCR_TIMEOUT=%s failed\n",
			expected_timeout);
		test_validate(false, true);
		return;
	}

	safassert(update_syncr_timeout(300, false), SA_AIS_OK);
	restore_syncr_timeout();

	int64_t timeout = estimate_timeout();
	unsetenv("IMMA_SYNCR_TIMEOUT");
	rc_validate(timeout / 1000, atoi(expected_timeout) / 100);
}

/**
 * Verify restore attibute without IMMA_SYNCR_TIMEOUT
 * environment variable
 *
 * Test: set saImmSyncrTimeout=0
 * Result: expect MDS wait time equals to 10s
 *
 */
void test_restore_syncr_timeout_without_setenv(void)
{
	if (is_non_root())
		return;
	if (unsetenv("IMMA_SYNCR_TIMEOUT") != 0) {
		fprintf(stderr,
			"Deletes a variable IMMA_SYNCR_TIMEOUT failed\n");
		test_validate(false, true);
		return;
	}

	safassert(update_syncr_timeout(300, false), SA_AIS_OK);
	restore_syncr_timeout();

	int64_t timeout = estimate_timeout();
	rc_validate(timeout / 1000, IMMSV_WAIT_TIME / 100);
}
