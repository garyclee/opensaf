/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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

#include "log/apitest/log_server.h"
#include "log/apitest/logtest.h"

enum role {
	ACTIVE_NODE = 0,
	STANDBY_NODE,
	PAYLOAD_NODE
};

static bool set_facility_id(uint32_t value)
{
	char command[1000] = {0};
	snprintf(command, sizeof(command),
		 "immcfg -a saLogStreamFacilityId=%d %s 2>/dev/null", value,
		 SA_LOG_STREAM_SYSTEM);
	return systemCall(command) == EXIT_SUCCESS;
}

static void restore_facility_id()
{
	const char *command =
	    "immcfg -a saLogStreamFacilityId= " SA_LOG_STREAM_SYSTEM;
	(void)systemCall(command);
}

static void enable_streaming()
{
	const char *dest = "test;UNIX_SOCKET;/tmp/test.sock";
	char command[1000];

	memset(command, 0, sizeof(command));
	snprintf(
	    command, sizeof(command),
	    "immcfg -a logRecordDestinationConfiguration=\"%s\" %s 2>/dev/null",
	    dest, SA_LOG_CONFIGURATION_OBJECT);
	(void)systemCall(command);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command),
		 "immcfg -a saLogRecordDestination=test %s 2>/dev/null",
		 SA_LOG_STREAM_SYSTEM);
	(void)systemCall(command);
}

static void disable_streaming()
{
	char command[1000];
	/* Restore the value back to the default value */
	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command),
		 "immcfg -a saLogRecordDestination= %s 2>/dev/null",
		 SA_LOG_STREAM_SYSTEM);
	(void)systemCall(command);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command),
		 "immcfg -a logRecordDestinationConfiguration= %s 2>/dev/null",
		 SA_LOG_CONFIGURATION_OBJECT);
	(void)systemCall(command);
	restore_facility_id();
}

static void switch_over()
{
	const char *command =
	    "amf-adm si-swap safSi=SC-2N,safApp=OpenSAF 2>/dev/null";
	(void)systemCall(command);
}

static uint8_t get_role()
{
	char node[256];
	sprintf(node, "SC-%d", get_active_sc());
	if (strncmp("PL", hostname(), 2) == 0) {
		return PAYLOAD_NODE;
	}
	if (strcmp(node, hostname()) == 0)
		return ACTIVE_NODE;
	return STANDBY_NODE;
}

//>
// 02 test cases about configuring `saLogStreamFacilityId`:
// 1) try to set valid values, expect getting OK.
// 2) try to set an invalid value, expect getting NOK.
//<

// TC#1: Set valid values [0-23] to `saLogStreamFacilityId`
void config_saLogStreamFacilityId_1()
{
	bool expected_success = true;
	for (uint32_t i = 0; i < 24; i++) {
		if (!set_facility_id(i)) {
			expected_success = false;
			break;
		}
	}
	test_validate(expected_success, true);
	restore_facility_id();
}

// TC#2: Set an invalid value to `saLogStreamFacilityId`
void config_saLogStreamFacilityId_2()
{
	test_validate(set_facility_id(24), false);
}

//>
// TC#3: Verify if PRI = 38 (saLogStreamFacilityId * 8 + severity) in package
// RFC5424 format.The test must run on active node.
//
// Change `saLogStreamFacilityId = 4` and send the log record with severity = 6
//<
void streaming_log_record_then_verify_PRI_1()
{
	if (get_role() != ACTIVE_NODE) {
		test_validate(true, true);
		return;
	}

	enable_streaming();
	set_facility_id(4);

	strcpy((char *)genLogRecord.logBuffer->logBuf, __FUNCTION__);
	genLogRecord.logBuffer->logBufSize = strlen(__FUNCTION__);
	genLogRecord.logHeader.genericHdr.logSeverity = SA_LOG_SEV_INFO;

	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	rc = logStreamOpen(&systemStreamName);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	StartUnixServer();

	rc = logWriteAsync(&genLogRecord);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	test_validate(FindPRI("<38>"), true);

done:
	StopUnixServer();
	restore_facility_id();
	disable_streaming();
	logFinalize();
}

//>
// TC#4: Verify if PRI = 134 (saLogStreamFacilityId (16)* 8 + severity) in
// package RFC5424 format.The test must run on active node.
//
// Delete attribute `saLogStreamFacilityId` and send the log record with
// severity = 6(information message)
//<
void streaming_log_record_then_verify_PRI_2()
{
	if (get_role() != ACTIVE_NODE) {
		test_validate(true, true);
		return;
	}

	enable_streaming();
	restore_facility_id();

	strcpy((char *)genLogRecord.logBuffer->logBuf, __FUNCTION__);
	genLogRecord.logBuffer->logBufSize = strlen(__FUNCTION__);
	genLogRecord.logHeader.genericHdr.logSeverity = SA_LOG_SEV_INFO;

	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	rc = logStreamOpen(&systemStreamName);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	StartUnixServer();

	rc = logWriteAsync(&genLogRecord);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	test_validate(FindPRI("<134>"), true);

done:
	StopUnixServer();
	disable_streaming();
	logFinalize();
}

//>
// TC#5: This test case verify if the PRI=38 after trigger switch-over and
// streaming log record ('PRI'field in package RFC5425 ).The test must run
// on standby node.
//<
void streaming_log_record_then_verify_PRI_3()
{
	if (get_role() != STANDBY_NODE) {
		test_validate(true, true);
		return;
	}

	enable_streaming();
	set_facility_id(4);
	switch_over();

	strcpy((char *)genLogRecord.logBuffer->logBuf, __FUNCTION__);
	genLogRecord.logBuffer->logBufSize = strlen(__FUNCTION__);
	genLogRecord.logHeader.genericHdr.logSeverity = SA_LOG_SEV_INFO;

	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	rc = logStreamOpen(&systemStreamName);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	StartUnixServer();

	rc = logWriteAsync(&genLogRecord);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	test_validate(FindPRI("<38>"), true);

done:
	StopUnixServer();
	restore_facility_id();
	disable_streaming();
	logFinalize();
}

__attribute__((constructor)) static void saLibraryLifeCycle_constructor(void)
{
	test_suite_add(22, "Test log stream config facility id");
	test_case_add(22, config_saLogStreamFacilityId_1,
		      "Set valid values to saLogStreamFacilityId");
	test_case_add(22, config_saLogStreamFacilityId_2,
		      "Set a invalid value to saLogStreamFacilityId");
	test_case_add(22, streaming_log_record_then_verify_PRI_1,
		      "Streaming with configure saLogStreamFacilityId=4 and "
		      "verify PRI field");
	test_case_add(
	    22, streaming_log_record_then_verify_PRI_2,
	    "Delelte attribute saLogStreamFacilityId and verify PRI field");
	test_case_add(22, streaming_log_record_then_verify_PRI_3,
		      "Trigger switch-over then verify PRI field in package "
		      "RFC5424 format");
}
