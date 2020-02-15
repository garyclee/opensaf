/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2019 - All Rights Reserved.
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

#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <aio.h>
#include <saLog.h>
#include <poll.h>
#include <pthread.h>

#include "log/apitest/logtest.h"

// Utility macro to evaluate returns of expession `a`. If the expression
// returns failure, the macro sets the test case failed and the test will
// discontinue.
#define AIS_EVALUATE(a)				         \
  do {						         \
	  SaAisErrorT rc = (a);				 \
	  if (rc != SA_AIS_OK) {			 \
		test_validate(rc, SA_AIS_OK);		 \
		return;					 \
	  }						 \
  } while (0)

#define MAX_DATA  256

//>
// 02 test cases about configuring `logResilienceTimeout`:
// 1) try to set a valid value, expect getting OK.
// 2) try to set an invalid value, expect getting NOK.
//<

// TC#1: Set a valid value to `logResilienceTimeout`
void config_logResilienceTimeout_1()
{
	const char* command = "immcfg -a logResilienceTimeout=16 "
		LOGTST_IMM_LOG_CONFIGURATION;
	rc_validate(systemCall(command), EXIT_SUCCESS);

	/* Restore the value back to the default value */
	command = "immcfg -a logResilienceTimeout=15 "
		LOGTST_IMM_LOG_CONFIGURATION;
	(void)systemCall(command);

}

// TC#2: Set an invalid value to `logResilienceTimeout`
void config_logResilienceTimeout_2()
{
	const char* command = "immcfg -a logResilienceTimeout=10 "
		LOGTST_IMM_LOG_CONFIGURATION " 2> /dev/null";
	rc_validate(systemCall(command), EXIT_FAILURE);
}

//>
// 02 test cases about configuring `logMaxPendingWriteRequests`:
// 1) try to set a valid value, expect getting OK.
// 2) try to set an invalid value, expect getting NOK.
//<

// TC#3: Set a valid value to `logMaxPendingWriteRequests`
void config_logMaxPendingWriteRequests_1()
{
	const char* command = "immcfg -a logMaxPendingWriteRequests=100 "
		LOGTST_IMM_LOG_CONFIGURATION;
	rc_validate(systemCall(command), EXIT_SUCCESS);

	/* Restore the value back to the default value */
	command = "immcfg -a logMaxPendingWriteRequests=0 "
		LOGTST_IMM_LOG_CONFIGURATION;
	(void)systemCall(command);

}

// TC#4: Set an invalid value to `logMaxPendingWriteRequests`
void config_logMaxPendingWriteRequests_2()
{
	const char* command = "immcfg -a logMaxPendingWriteRequests=1001 "
		LOGTST_IMM_LOG_CONFIGURATION " 2> /dev/null";
	rc_validate(systemCall(command), EXIT_FAILURE);
}


//>
// These below code is for testing the log resilience, and only run when
// compiling these tests and log server with SIMULATE_NFS_UNRESPONSE flag on.
//
// Once the flag is on, a delay is added into log server before writing log
// record, it works as the case the underlying file system is unresponsive.
// And the delay only takes affect when given a non-zero value to
// `logMaxPendingWriteRequests`. When everything is set, 16 seconds delay
// is inserted every 02 write async requests.
//<

#ifdef SIMULATE_NFS_UNRESPONSE

// Generate a random invocation as starting point.
static time_t g_invocation = 0;

// The offset that forming the final invocation sent to log server.
// the final invocation = g_invocation + g_offset
static SaInvocationT g_ack_invocation = 0;
static SaAisErrorT g_ack_ais_code = SA_AIS_OK;

static void configure_cache_capacity(uint32_t capacity)
{
	char command[MAX_DATA];
	snprintf(command, MAX_DATA, "immcfg -a logMaxPendingWriteRequests=%u %s "
		 "2>/dev/null", capacity, LOGTST_IMM_LOG_CONFIGURATION);
	while (systemCall(command) != EXIT_SUCCESS) {
		usleep(500*1000);
	}
}

static uint32_t current_pending_write_requests()
{

	const char* command = "immlist -a logCurrentPendingWriteRequests"
		" logConfig=currentConfig,safApp=safLogService | cut -d= -f2";
	FILE* fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(EXIT_FAILURE);
	}

	char data[MAX_DATA];
	while (fgets(data, sizeof(data), fp) != NULL) {}
	pclose(fp);
	return atoi(data);
}

static void saLogWriteLogCallback(SaInvocationT inv, SaAisErrorT error)
{
	g_ack_ais_code = error;
	g_ack_invocation = inv;
}

static void setup()
{
	logCallbacks.saLogWriteLogCallback = saLogWriteLogCallback;
	AIS_EVALUATE(logInitialize());
	AIS_EVALUATE(logStreamOpen(&systemStreamName));
	AIS_EVALUATE(saLogSelectionObjectGet(logHandle, &selectionObject));
}

static void switch_over()
{
	const char* command = "amf-adm si-swap safSi=SC-2N,safApp=OpenSAF 2>/dev/null";
	(void)systemCall(command);
}

static void waiting_for_ack(SaInvocationT inv) {
	struct pollfd fds[1];
	fds[0].fd = selectionObject;
	fds[0].events = POLLIN;
	int ret = -1;

retry:
	ret = poll(fds, 1, 16*1000);
	if (ret == -1 && errno == EINTR) goto retry;
	if (ret == -1) {
		fprintf(stderr, "poll failed\n");
		exit(EXIT_FAILURE);
	}

	if (ret == 0) {
		fprintf(stderr, "16seconds expired!\n");
		exit(EXIT_FAILURE);
	}

	AIS_EVALUATE(saLogDispatch(logHandle, SA_DISPATCH_ONE));
	if (g_ack_invocation != inv) {
		fprintf(stderr, "Got ack for wrong invocation"
			" (exp: %lld, got: %lld)\n", inv, g_ack_invocation);
		exit(EXIT_FAILURE);
	}
}

static void writeasync()
{
	// TODO(vu.m.nguyen): invocation is a global variable. The name convention
	// for those logtest global is very easily misused. Should be improved ?
	++invocation;
	ack_flags = SA_LOG_RECORD_WRITE_ACK;
	char record[MAX_DATA];
	snprintf(record, sizeof(record), "cached_data with invoc %lld", invocation);
	strcpy((char *)genLogRecord.logBuffer->logBuf, record);
	genLogRecord.logBuffer->logBufSize = strlen(record);
	AIS_EVALUATE(logWriteAsync(&genLogRecord));
}

static void writesync()
{
	writeasync();
	waiting_for_ack(invocation);
}


static void finalize() {
	AIS_EVALUATE(logFinalize());
}

static time_t subtract(const struct timespec* end,
		       const struct timespec* start)
{
	return end->tv_sec - start->tv_sec;
}

//>
// Given the queue capacity is set to 02, and the resilient timeout is 15second
// These tests take time; around ~20 seconds per test case.
//<

//>
// TC#5: Test if the a write request is dropped if its time is overdue
// and whether log server has kept the require in a proper time or not.
//
// Write 02 consecutive records and verify if getting SA_AIS_OK for
// the first invocation, `logCurrentPendingWriteRequests`=1 and
// getting SA_AIS_ERR_TRY_AGAIN for second invocation. In addition,
// also check how long the log server has been kept the write request
// before it is dropped.
//<
void write_two_records_while_nfs_hung()
{
	// 1) Enable log resilient
	configure_cache_capacity(2);
	// 2) Set up log client before writing a log record
	setup();
	// 3) Write a log record and wait for getting an ack.
	writesync();
	// 4) Verify if getting OK for first invocation
	uint32_t rc = 0, counter = 10;
	if (g_ack_ais_code != SA_AIS_OK) {
	        fprintf(stderr, "Got wrong ack code for first invocation\n");
		test_validate(g_ack_ais_code, SA_AIS_OK);
		goto done;
	}
	// 5) Write a second record. This time, log server will be suspended 16s.
	writeasync();

	// 6) Make sure `logCurrentPendingWriteRequests=1`
	while ((rc = current_pending_write_requests()) != 1 && counter--) {
		usleep(100*1000);
	}

	if (rc != 1) {
		fprintf(stderr, "Got wrong number of pending write reqs\n");
		rc_validate(rc, 1);
		goto done;
	}

	// Also track how long the log server keeps this write request before
	// it has been dropped.
	struct timespec start;
	osaf_clock_gettime(CLOCK_MONOTONIC, &start);

	waiting_for_ack(invocation);
	// 7) Ensure getting SA_AIS_ERR_TRY_AGAIN for the second invocation
	if (g_ack_ais_code != SA_AIS_ERR_TRY_AGAIN) {
		fprintf(stderr, "Got wrong ack code for 2nd invocation\n");
		test_validate(g_ack_ais_code, SA_AIS_ERR_TRY_AGAIN);
		goto done;
	}

	struct timespec end;
	osaf_clock_gettime(CLOCK_MONOTONIC, &end);
	time_t passed_second = subtract(&end, &start);
	// 14 = ~given resilience timeout (wont count on ns)
	// 20 = 20 seconds sleep in I/O thread delay
	// We expect log server thread must drop the write request as soon
	// as the given resilience timeout is reached.
	if (passed_second < 14 || passed_second > 20) {
		fprintf(stderr, "Log server has dropped the msg in an improper time."
			" Expected: ~15second, took: %ld\n", passed_second);
		rc_validate(passed_second, 16);
		goto done;
	}

	test_validate(g_ack_ais_code, SA_AIS_ERR_TRY_AGAIN);

done:
	// Restore settings to original value
	configure_cache_capacity(0);
	finalize();
}

//>
// TC#6: Mainly test the case getting a write request while the queue is full.
//
// Write 04 consecutive records and verify if getting SA_AIS_OK for
// the first invocation, `logCurrentPendingWriteRequests`=2 and
// getting SA_AIS_ERR_TRY_AGAIN for forth invocation as the queue is full.
//<

void write_04_records_then_queue_is_full()
{
	// 1) Enable log resilient
	configure_cache_capacity(2);
	// 2) Set up log client before writing a log record
	setup();
	// 3) Write a log record and wait for getting an ack.
	writesync();
	// 4) Verify if getting OK for first invocation
	uint32_t rc = 0, counter = 10;
	if (g_ack_ais_code != SA_AIS_OK) {
	        fprintf(stderr, "Got wrong ack code for first invocation\n");
		test_validate(g_ack_ais_code, SA_AIS_OK);
		goto done;
	}
	// 5) Write 02 records. This time, log server will be suspended 16s.
	writeasync();
	writeasync();

	// 6) Make sure `logCurrentPendingWriteRequests=2`
	while ((rc = current_pending_write_requests()) != 2 && counter--) {
		usleep(100*1000);
	}

	if (rc != 2) {
		fprintf(stderr, "Got wrong number of pending write reqs\n");
		test_validate(rc, 2);
		goto done;
	}

	// 7) Write a log record and wait for getting an ack.
	writesync();

	// 8) Verify if getting SA_AIS_ERR_TRY_AGAIN because *the queue is full*
	test_validate(g_ack_ais_code, SA_AIS_ERR_TRY_AGAIN);

done:
	configure_cache_capacity(0);
	finalize();
}

//>
// Below test cases run with queue capacity set to 100.
//<
//
static int num_ack_invocation = 0;
static void polling_and_measure_time_getting_acks(struct timespec start)
{
	struct pollfd fds[1];
	int ret = -1;
	struct timespec end;

	osaf_clock_gettime(CLOCK_MONOTONIC, &end);
	time_t passed_second = subtract(&end, &start);

	fds[0].fd = selectionObject;
	fds[0].events = POLLIN;
	num_ack_invocation = 0;
	while (passed_second < 60*5) {
		ret = poll(fds, 1, 20*1000);
		if (ret == -1 && errno == EINTR) continue;
		if (ret == -1) {
			fprintf(stderr, "poll failed\n");
			exit(EXIT_FAILURE);
		}

		if (ret == 0) {
			fprintf(stderr, "20seconds expired!\n");
			exit(EXIT_FAILURE);
		}

		saLogDispatch(logHandle, SA_DISPATCH_ONE);
		if (g_ack_invocation < invocation - 9 ||
		    g_ack_invocation > invocation) {
			fprintf(stderr, "Got ack for wrong invocation"
				" (exp: [%lld - %lld], got: %lld)\n",
				invocation - 9, invocation, g_ack_invocation);
			return;
		}

		osaf_clock_gettime(CLOCK_MONOTONIC, &end);
		time_t passed_second = subtract(&end, &start);
		if (g_ack_ais_code == SA_AIS_OK) {
			if (passed_second > 16) {
				fprintf(stderr, "Got OK, but overdue time "
					"is big (%ldd)\n",
					passed_second);
				return;
			}
		} else if (g_ack_ais_code == SA_AIS_ERR_TRY_AGAIN) {
			if (passed_second > 20 || passed_second < 15) {
				fprintf(stderr, "Got try-again, but overdue time"
					" is wrong (%ld)\n",
					passed_second);
				return;
			}
		} else {
			fprintf(stderr, "Got unexpected error code: %d\n",
				g_ack_ais_code);
			return;
		}

		num_ack_invocation++;
		if (num_ack_invocation == 9) break;

	}
}

//>
// TC#7: Check if the queue is fully and correctly synced with standby
//
// Write 10 consecutive log records, then do a switchover. Verify
// if all invocations get acknowledgment from log server or not.
// Besides, also check if resilient time is actually overdue for
// ones that getting SA_AIS_ERR_TRY_AGAIN, and not yet overdue
// for those getting SA_AIS_OK.
//<
void write_10_records_then_trigger_switchover()
{
	// 1) Enable log resilient. During switchover, many log records comming
	// from SAF services are sent to log server, so run test with more
	// queue capacity to avoid impact on this test case.
	configure_cache_capacity(100);
	// 2) Set up log client before writing a log record
	setup();
	// 3) Write a log record and wait for getting an ack.
	writesync();
	// 4) Verify if getting OK for first invocation
	if (g_ack_ais_code != SA_AIS_OK) {
	        fprintf(stderr, "Got wrong ack code for first invocation\n");
		test_validate(g_ack_ais_code, SA_AIS_OK);
		goto done;
	}
	// 5) Write 9 records. This time, log server will be suspended 16s
	// every 02 requests including the above one.
	struct timespec start;
	osaf_clock_gettime(CLOCK_MONOTONIC, &start);

	for (int i = 1; i < 10; i++) writeasync();

	// 6) Do switchover; before that wwait for a while to make sure
	// no write async is interrupted by HA transitions; otherwise
	// log server will return try-again for those request arriving
	// while its state is quiesced.
	sleep(1);
	switch_over();

	// 7) Waiting for geting acks and also measure if spending time for
	// each requests is appropriate.
	polling_and_measure_time_getting_acks(start);

	if (num_ack_invocation != 9) {
		fprintf(stderr, "Got unexpected num of acks. Exp: 9, got: %d\n",
			num_ack_invocation);
		rc_validate(num_ack_invocation, 9);
		goto done;
	}

	test_validate(SA_AIS_OK, SA_AIS_OK);
done:
	configure_cache_capacity(0);
	finalize();
}

static void* polling_and_counting_acks(void* data)
{
	struct pollfd fds[1];
	int ret = -1;

	struct timespec start = *(struct timespec *)(data);
	struct timespec end;

	osaf_clock_gettime(CLOCK_MONOTONIC, &end);
	time_t passed_second = subtract(&end, &start);

	fds[0].fd = selectionObject;
	fds[0].events = POLLIN;
	num_ack_invocation = 0;
	while (passed_second < 60*5) {
		ret = poll(fds, 1, 200*1000);
		if (ret == -1 && errno == EINTR) continue;
		if (ret == -1) {
			fprintf(stderr, "poll failed\n");
			exit(EXIT_FAILURE);
		}

		if (ret == 0) {
			fprintf(stderr, "200seconds expired - got # of acks: %d!\n",
				num_ack_invocation);
			exit(EXIT_FAILURE);
		}

		if (fds[0].revents & POLLIN) {
			saLogDispatch(logHandle, SA_DISPATCH_ONE);
			if (g_ack_invocation < invocation - 49 ||
			    g_ack_invocation > invocation) {
				fprintf(stderr, "Got ack for wrong invocation"
					" (exp: [%lld - %lld], got: %lld)\n",
					invocation - 9, invocation, g_ack_invocation);
				return NULL;
			}
			num_ack_invocation++;
			if (num_ack_invocation == 49) break;
		}
		osaf_clock_gettime(CLOCK_MONOTONIC, &end);
		passed_second = subtract(&end, &start);
	}
	return NULL;
}

//>
// TC#8: This test case verifies if the LOG agent triggers write callbacks
// for lost log record when cluster goes to headless. The test must run
// on PL node and headless mode is enabled. Report PASS if the test runs on SC.
//
// Write 50 consecutive records then shutdown both SCs. It is expected
// to get OK for first write request, and try-again for lost records.
//<
void write_50_records_then_cluster_goes_to_headless()
{
	if (is_test_done_on_pl() == false) {
		fprintf(stderr, "This test must run on PL node\n");
		test_validate(SA_AIS_OK, SA_AIS_OK);
		return;
	}

	pthread_t thread;

	// 1) Enable log resilient
	configure_cache_capacity(100);
	// 2) Set up log client before writing a log record
	setup();
	// 3) Write a log record and wait for getting an ack.
	writesync();
	// 4) Verify if getting OK for first invocation
	if (g_ack_ais_code != SA_AIS_OK) {
	        fprintf(stderr, "Got wrong ack code for first invocation\n");
		test_validate(g_ack_ais_code, SA_AIS_OK);
		return;
	}

	printf("-->Shutdown standby node, then enter any key to continue...\n");
	getchar();

	struct timespec start;
	osaf_clock_gettime(CLOCK_MONOTONIC, &start);

	// 5) Write 49 more records. This time, log server will be suspended 16s.
	for (int i = 1; i < 50; i++) writeasync();
	sleep(1);

	int rc = pthread_create(&thread, NULL,
				&polling_and_counting_acks, &start);
	assert(rc == 0 && "Failed to create polling thread");

	printf("-->Shutdown active node, then enter any key to continue...\n");
	getchar();

	pthread_join(thread, NULL);

	if (num_ack_invocation != 49) {
		fprintf(stderr, "Got wrong number of acks. Exp: 49, got: %d\n",
			num_ack_invocation);
		rc_validate(num_ack_invocation, 49);
		return;
	}

	test_validate(SA_AIS_OK, SA_AIS_OK);
}

#endif

__attribute__((constructor)) static void saLibraryLifeCycle_constructor(void)
{
	test_suite_add(20, "Test log resilience feature");
	test_case_add(20, config_logResilienceTimeout_1,
		      "Set a valid value to logResilienceTimeout");
	test_case_add(20, config_logResilienceTimeout_2,
		      "Set a invalid value to logResilienceTimeout");
	test_case_add(20, config_logMaxPendingWriteRequests_1,
		      "Set a valid value to logMaxPendingWriteRequests");
	test_case_add(20, config_logMaxPendingWriteRequests_2,
		      "Set a invalid value to logMaxPendingWriteRequests");
#ifdef SIMULATE_NFS_UNRESPONSE
	// initial setup - getting unique invocation for every test.
	g_invocation = time(0);
	invocation = g_invocation;

	test_case_add(20, write_two_records_while_nfs_hung,
		      "Write 02 consecutive records, then get nfs hung on the second");
	test_case_add(20, write_04_records_then_queue_is_full,
		      "Write 04 consecutive records, and the queue is full on the 3rd");
	test_case_add(20, write_10_records_then_trigger_switchover,
		      "Write 10 consecutive records, then trigger switchover");
#endif
}

#ifdef SIMULATE_NFS_UNRESPONSE
// This suite requires manual shutting down both SCs, therefore put the test
// into 'extended' tests. Only run with option -e.
void add_suite_21()
{
	test_suite_add(21, "Test if LGA notifies lost records to client");
	test_case_add(21, write_50_records_then_cluster_goes_to_headless,
		      "Write 50 records, queuing 49 records then cluster goes to headless");
}
#endif
