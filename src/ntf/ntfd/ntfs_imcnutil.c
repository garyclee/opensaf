/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2013 The OpenSAF Foundation
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include "base/logtrace.h"
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "base/ncs_main_papi.h"
#include "base/osaf_utility.h"
#include "base/osaf_time.h"

#include "ntf/ntfd/ntfs.h"
#include "ntf/ntfd/ntfs_imcnutil.h"

#include "osaf/configmake.h"

typedef struct {
	SaAmfHAStateT ha_state;
	pid_t pid;
	pthread_t thread;
	/* ntfimcn functionality: true(enabled), false(disabled)  */
	bool ntfimcn_on;
} init_params_t;

static init_params_t ipar = {0, 0, 0, false};
pthread_mutex_t ntfimcn_mutex;


/**
 * Timed out wait for the imcnproc to terminate.
 *
 * @return -1 If wait timed out
 */
static int wait_imcnproc_termination(void)
{
	struct timespec wait_poll;
	pid_t rc_pid = (pid_t)0;
	int retry_cnt = 0;
	const int max_retry = 100;
	const uint64_t wait_poll_ms = 100;
	int status = 0;
	int rc = 0;

	TRACE_ENTER();
	osaf_millis_to_timespec(wait_poll_ms, &wait_poll);

	/* Wait for child process to terminate */
	retry_cnt = 0;
	do {
		rc_pid = waitpid(ipar.pid, &status, WNOHANG);
		if (rc_pid == -1) {
			if (errno == EINTR) {
				TRACE("\tGot EINTR continue waiting");
				rc_pid = (pid_t)0;
			} else if (errno == ECHILD) {
				TRACE("\t Done, the process is terminated");
				rc = 0;
				break;
			} else {
				/* EINVAL Invalid option */
				LOG_ER("%s Internal error %s", __FUNCTION__,
				       strerror(errno));
				osaf_abort(0);
			}
		}

		/* No status available. Wait and try again */
		retry_cnt++;
		if (retry_cnt <= max_retry) {
			osaf_nanosleep(&wait_poll);
		} else {
			TRACE("\tTermination timeout");
			rc = -1;
			break;
		}
	} while (rc_pid == 0);

	TRACE_LEAVE2("rc = %d, retry_cnt = %d", rc, retry_cnt);
	return rc;
}

/**
 * Send SIGKILL to osafntfimcnd then waiting for termination. If no termination
 * is detected, ignore and continue NTF. Something is wrong and we may end up
 * with a 'zombie' process but there is nothing more to do  except aborting NTF
 * causing a node restart.
 *
 */
static void kill_imcnproc_in_timewait(void)
{
	int rc = 0;
	TRACE_ENTER();

	LOG_NO("%s: SIGKILL sent to osafntfimcnd process pid = %d",
	       __FUNCTION__, ipar.pid);
	rc = kill(ipar.pid, SIGKILL);
	if (rc == -1) {
		if (errno == EPERM) {
			/* We don't have permission to kill the process! */
			LOG_ER(
			    "Child process osafntfimcn could not be killed: %s",
			    strerror(errno));
		} else if (errno == ESRCH) {
			/* The process was not found */
			LOG_NO(
			    "Child process osafntfimcn could not be killed: %s",
			    strerror(errno));
		} else {
			/* EINVAL Invalid or unsupported signal number */
			LOG_ER("%s Fatal error when killing %s", __FUNCTION__,
			       strerror(errno));
			osaf_abort(0);
		}
	}

	rc = wait_imcnproc_termination();
	if (rc == 0) {
		TRACE("\tImcn successfully killed");
	} else {
		LOG_WA("%s: The osafntfimcnd process termination has "
		       "timed out!", __FUNCTION__);
	}

	TRACE_LEAVE();
}

static pid_t create_imcnprocess(SaAmfHAStateT ha_state)
{
	char *start_args[3];
	pid_t i_pid = (pid_t)-1;

	TRACE_ENTER();

	/*
	 * Start the imcn process. Make the process aware of
	 * current HA state.
	 */
	i_pid = fork();
	if (i_pid == -1) {
		LOG_ER("Failed to fork %s", strerror(errno));
		abort();
	} else if (i_pid == 0) {
		/* We are Child. Run as subprocess */
		start_args[0] = PKGLIBDIR "/" NTFIMCN_PROC_NAME;
		start_args[1] =
		    (ha_state == SA_AMF_HA_ACTIVE) ? "active" : "standby";
		start_args[2] = 0;

		execvp(start_args[0], start_args);
		LOG_ER("Failed to start!");
		abort();
	}

	TRACE_LEAVE();
	return i_pid;
}

/**
 * Thread:
 * Start the imcn process and wait for process to exit.
 * If the process exit then restart it.
 *
 * @param _init_params[in]
 * @return
 */
static void *cnsurvail_thread(void *_init_params)
{
	init_params_t *ipar = (init_params_t *)_init_params;
	pid_t pid = (pid_t)-1;
	int status = 0;
	pid_t rc;

	TRACE_ENTER();

	while (1) {
		osaf_mutex_lock_ordie(&ntfimcn_mutex);
		/* Only start ntfimcn process if this functionality is
		 * enabled, this is to avoid restarting ntfimcn when ntfd
		 * receives SIGTERM (shutting down)
		 * NOTE: Do not add any code outside below *if @ntfimcn_on*
		 * block that may lead to a thread cancellation point while
		 * ntfimcn_mutex is being locked
		 */
		if (ipar->ntfimcn_on == true) {
			pid = create_imcnprocess(ipar->ha_state);
			ipar->pid = pid;
		}
		osaf_mutex_unlock_ordie(&ntfimcn_mutex);

		/* Wait for child process to exit */
		do {
			rc = waitpid(ipar->pid, &status, 0);
		} while ((rc == -1) && (errno == EINTR));

		if ((rc == -1) && (errno == ECHILD)) {
			/* The process does not exist, create it */
			continue;
		} else if (rc == -1) {
			/*
			 * This should never happen.
			 * To get here an options to waitpid is invalid (EINVAL)
			 */
			LOG_ER("waitpid returned with error %s",
			       strerror(errno));
			abort();
		}

		int exit_rc = WIFEXITED(status);
		int exit_stat = WEXITSTATUS(status);
		TRACE("osafntfimcnd process terminated reason %s (%d)",
		      exit_rc ? "exit" : "other", exit_stat);
	}
}

/**
 * Start the imcn process surveillance thread
 * When surveillance thread is running, this thread
 * will start and monitor ntfimcn process in cnsurvail_thread()
 * @param ha_state[in]
 */
static void start_cnprocess(SaAmfHAStateT ha_state)
{
	int rc;

	TRACE_ENTER();

	rc = pthread_mutex_init(&ntfimcn_mutex, NULL);
	if (rc != 0)
		osaf_abort(rc);

	ipar.ha_state = ha_state;
	ipar.ntfimcn_on = true;
	ipar.pid = 0;
	rc =
	    pthread_create(&ipar.thread, NULL, cnsurvail_thread, (void *)&ipar);
	if (rc != 0)
		osaf_abort(rc);

	TRACE_LEAVE();
}

/**
 * Initialize the configuration change notifier
 *
 * @param ha_state[in]
 */
void init_ntfimcn(SaAmfHAStateT ha_state) { start_cnprocess(ha_state); }

/**
 * Kill the imcn process. Use the pid saved when the process was started
 * This will cause the imcn process to be restarted in the given state.
 * This is done only if the current state is Active or Standby and if
 * the state has changed
 *
 * @param ha_state[in]
 */
void handle_state_ntfimcn(SaAmfHAStateT ha_state)
{
	TRACE_ENTER();
	if ((ha_state == SA_AMF_HA_ACTIVE) || (ha_state == SA_AMF_HA_STANDBY)) {
		osaf_mutex_lock_ordie(&ntfimcn_mutex);
		if (ha_state != ipar.ha_state) {
			ipar.ha_state = ha_state;
			TRACE("%s: Terminating osafntfimcnd process",
			      __FUNCTION__);
			kill_imcnproc_in_timewait();
		} else {
			TRACE("%s: osafntfimcnd process not restarted. "
			      "Already in correct state",
			      __FUNCTION__);
		}
		osaf_mutex_unlock_ordie(&ntfimcn_mutex);
	}
	TRACE_LEAVE();
}

/**
 * This function stops functionality of ntfimcn by:
 * First: Kill imcn process
 * Second: Cancel the surveillance thread
 * (in reversed order of start ntfimcn)
 * Use the pid and thread id saved when the process was started
 * This will terminate the process permanently.
 *
 * @return 0 if success, abort() on any error
 */
int stop_ntfimcn(void)
{
	int rc = 0;
	TRACE_ENTER();

	if (ipar.ntfimcn_on == false) goto done;
	/* Kill ntfimcn */
	osaf_mutex_lock_ordie(&ntfimcn_mutex);
	ipar.ntfimcn_on = false;
	TRACE("%s: Terminating osafntfimcnd process", __FUNCTION__);
	kill_imcnproc_in_timewait();
	osaf_mutex_unlock_ordie(&ntfimcn_mutex);

	/* Cancel the surveillance thread */
	TRACE("%s: Cancel the imcn surveillance thread", __FUNCTION__);
	rc = pthread_cancel(ipar.thread);
	if (rc != 0)
		osaf_abort(rc);
	rc = pthread_join(ipar.thread, NULL);
	if (rc != 0)
		osaf_abort(rc);
	rc = pthread_mutex_destroy(&ntfimcn_mutex);
	if (rc != 0)
		osaf_abort(rc);
done:
	TRACE_LEAVE();
	return rc;
}
