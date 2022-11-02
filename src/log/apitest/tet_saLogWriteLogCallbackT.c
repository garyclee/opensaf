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

#include <poll.h>
#include <unistd.h>
#include "base/saf_error.h"
#include "base/osaf_time.h"

#include "logtest.h"

static SaInvocationT cb_invocation[8];
static SaAisErrorT cb_error[8];
static int cb_index;
const uint64_t kWaitTime = 10*1000; // Wait for timeout is 10 seconds

static void logWriteLogCallbackT(SaInvocationT invocation, SaAisErrorT error)
{
	cb_invocation[cb_index] = invocation;
	cb_error[cb_index] = error;
	cb_index++;
}

SaAisErrorT logWrite(SaInvocationT invocation, SaLogAckFlagsT ackFlags,
		     const SaLogRecordT *logRecord)
{
	struct timespec timeout_time;
	osaf_set_millis_timeout(kWaitTime, &timeout_time);
	SaAisErrorT rc = saLogWriteLogAsync(logStreamHandle, invocation,
					    ackFlags, logRecord);
	while (rc == SA_AIS_ERR_TRY_AGAIN && !osaf_is_timeout(&timeout_time)) {
		osaf_nanosleep(&kHundredMilliseconds);
		rc = saLogWriteLogAsync(logStreamHandle, invocation, ackFlags,
					logRecord);
	}

	if (rc != SA_AIS_OK)
		fprintf(stderr, " saLogWriteLogAsync FAILED: %s\n",
			saf_error(rc));
	return rc;
}

void saLogWriteLogCallbackT_01(void)
{
	SaInvocationT invocation;
	struct pollfd fds[1];
	int ret;

	invocation = random();
	logCallbacks.saLogWriteLogCallback = logWriteLogCallbackT;
	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		return;
	}
	rc = saLogSelectionObjectGet(logHandle, &selectionObject);
	if (rc != SA_AIS_OK) {
		fprintf(stderr, " saLogSelectionObjectGet failed: %d \n",
			(int)rc);
		test_validate(rc, SA_AIS_OK);
		goto done;
	}
	rc = logStreamOpen(&systemStreamName);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}
	strcpy((char *)genLogRecord.logBuffer->logBuf, __FUNCTION__);
	genLogRecord.logBuffer->logBufSize = strlen(__FUNCTION__) + 1;

	struct timespec timeout_time;
	osaf_set_millis_timeout(2 * kWaitTime, &timeout_time);

write_log:
	cb_index = 0;
	rc = logWrite(invocation, SA_LOG_RECORD_WRITE_ACK, &genLogRecord);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	fds[0].fd = (int)selectionObject;
	fds[0].events = POLLIN;
	ret = poll(fds, 1, 6000);
	if (ret != 1) {
		fprintf(stderr, " poll log callback failed: %d \n", ret);
		test_validate(ret, 1);
		goto done;
	}
	rc = saLogDispatch(logHandle, SA_DISPATCH_ONE);
	if (rc != SA_AIS_OK) {
		fprintf(stderr, " saLogDispatch failed: %d \n", (int)rc);
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	if (cb_error[0] == SA_AIS_ERR_TRY_AGAIN &&
			!osaf_is_timeout(&timeout_time)) {
	  osaf_nanosleep(&kHundredMilliseconds);
	  printf("Get try again error, re-write \n");
	  goto write_log;
	}

	if (cb_invocation[0] == invocation)
		test_validate(cb_error[0], SA_AIS_OK);
	else
		test_validate(SA_AIS_ERR_LIBRARY, SA_AIS_OK);

done:
	logFinalize();
}

void saLogWriteLogCallbackT_02(void)
{
	SaInvocationT invocation[3];
	struct pollfd fds[1];
	int ret, i;

	invocation[0] = random();
	invocation[1] = random();
	invocation[2] = random();
	logCallbacks.saLogWriteLogCallback = logWriteLogCallbackT;
	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		fprintf(stderr, " logInitialize failed: %d \n", (int)rc);
		test_validate(rc, SA_AIS_OK);
		return;
	}
	rc = saLogSelectionObjectGet(logHandle, &selectionObject);
	if (rc != SA_AIS_OK) {
		fprintf(stderr, " saLogSelectionObjectGet failed: %d \n",
			(int)rc);
		test_validate(rc, SA_AIS_OK);
		goto done;
	}
	rc = logStreamOpen(&systemStreamName);
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	strcpy((char *)genLogRecord.logBuffer->logBuf, __FUNCTION__);
	genLogRecord.logBuffer->logBufSize = strlen(__FUNCTION__) + 1;

	struct timespec timeout_time;
	osaf_set_millis_timeout(2 * kWaitTime, &timeout_time);
write_log:
	cb_index = 0;
	rc = logWrite(invocation[0], SA_LOG_RECORD_WRITE_ACK, &genLogRecord);
	if (rc == SA_AIS_OK)
		rc = logWrite(invocation[1], SA_LOG_RECORD_WRITE_ACK,
			      &genLogRecord);
	if (rc == SA_AIS_OK)
		rc = logWrite(invocation[2], SA_LOG_RECORD_WRITE_ACK,
			      &genLogRecord);

	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	sleep(1); /* Let the writes make it to disk and back */
	fds[0].fd = (int)selectionObject;
	fds[0].events = POLLIN;
	ret = poll(fds, 1, 6000);
	if (ret != 1) {
		fprintf(stderr, " poll log callback failed: %d \n", ret);
		test_validate(ret, 1);
		goto done;
	}
	rc = saLogDispatch(logHandle, SA_DISPATCH_ALL);
	if (rc != SA_AIS_OK) {
		fprintf(stderr, " saLogDispatch failed: %d \n", (int)rc);
		test_validate(rc, SA_AIS_OK);
		goto done;
	}

	if (cb_index != 3) {
		printf("cb_index = %d\n", cb_index);
		test_validate(SA_AIS_ERR_LIBRARY, SA_AIS_OK);
		goto done;
	}

	for (i = 0; i < 3; i++) {
		if (cb_error[i] == SA_AIS_ERR_TRY_AGAIN &&
				!osaf_is_timeout(&timeout_time)) {
			osaf_nanosleep(&kHundredMilliseconds);
			printf("Get try again error, re-write \n");
			goto write_log;
		}

		if ((cb_invocation[i] != invocation[i]) ||
		    (cb_error[i] != SA_AIS_OK)) {
			printf("%llu expected %llu, %d\n", cb_invocation[i],
			       invocation[i], cb_error[i]);
			test_validate(cb_error[i], SA_AIS_OK);
			goto done;
		}
	}

	test_validate(SA_AIS_OK, SA_AIS_OK);

done:
	logFinalize();
}

void saLogWriteLogCallbackT_03(void)
{
	SaInvocationT invocation;
	struct pollfd fds[1];
	int ret;

	invocation = random();
	logCallbacks.saLogWriteLogCallback = logWriteLogCallbackT;
	rc = logInitialize();
	if (rc != SA_AIS_OK) {
		test_validate(rc, SA_AIS_OK);
		return;
	}
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed: %s \n", strerror(errno));
		test_validate(SA_AIS_ERR_LIBRARY, SA_AIS_OK);
	} else if (pid == 0) {
		// While exiting this process, the destructor of log agent
		// will be called.
		exit(0);
	} else {
		sleep(2); /* Sleep to make sure the child process exited. */
		rc = saLogSelectionObjectGet(logHandle, &selectionObject);
		if (rc != SA_AIS_OK) {
			fprintf(stderr,
				"saLogSelectionObjectGet failed: %d \n",
				(int)rc);
			test_validate(rc, SA_AIS_OK);
			goto done;
		}
		rc = logStreamOpen(&systemStreamName);
		if (rc != SA_AIS_OK) {
			test_validate(rc, SA_AIS_OK);
			goto done;
		}
		strcpy((char *)genLogRecord.logBuffer->logBuf, __FUNCTION__);
		genLogRecord.logBuffer->logBufSize = strlen(__FUNCTION__) + 1;

		struct timespec timeout_time;
		osaf_set_millis_timeout(2 * kWaitTime, &timeout_time);

	write_log:
		cb_index = 0;
		rc = logWrite(invocation, SA_LOG_RECORD_WRITE_ACK,
			      &genLogRecord);
		if (rc != SA_AIS_OK) {
			test_validate(rc, SA_AIS_OK);
			goto done;
		}

		fds[0].fd = (int)selectionObject;
		fds[0].events = POLLIN;
		ret = poll(fds, 1, 6000);
		if (ret != 1) {
			fprintf(stderr, "poll log callback failed: %d \n",
				ret);
			test_validate(ret, 1);
			goto done;
		}
		rc = saLogDispatch(logHandle, SA_DISPATCH_ONE);
		if (rc != SA_AIS_OK) {
			fprintf(stderr, "saLogDispatch failed: %d \n",
				(int)rc);
			test_validate(rc, SA_AIS_OK);
			goto done;
		}

		if (cb_error[0] == SA_AIS_ERR_TRY_AGAIN &&
				!osaf_is_timeout(&timeout_time)) {
			osaf_nanosleep(&kHundredMilliseconds);
			printf("Get try again error, re-write \n");
			goto write_log;
		}

		if (cb_invocation[0] == invocation) {
			test_validate(cb_error[0], SA_AIS_OK);
		} else {
			test_validate(SA_AIS_ERR_LIBRARY, SA_AIS_OK);
		}

	done:
		logFinalize();
	}
}
