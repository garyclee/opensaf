/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2016 The OpenSAF Foundation
 * Copyright Ericsson AB 2016, 2017 - All Rights Reserved.
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

#include "log/agent/lga_state.h"
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include "base/osaf_poll.h"
#include "base/ncs_osprm.h"
#include "base/osaf_time.h"
#include "base/saf_error.h"
#include "base/osaf_extended_name.h"
#include "log/agent/lga_mds.h"
#include "log/agent/lga_util.h"
#include "log/agent/lga_agent.h"
#include "log/agent/lga_common.h"

/* Selection object for terminating recovery thread for state 2 (after timeout)
 * NOTE: Only for recovery2_thread
 */
static NCS_SEL_OBJ state2_terminate_sel_obj;

static pthread_t recovery2_thread_id = 0;

/******************************************************************************
 * Functions used with server down recovery handling
 ******************************************************************************/

static int start_recovery2_thread(void);
static void set_lga_recovery_state(RecoveryState state);

/******************************************************************************
 * Functions for sending messages to the server used in the recovery functions
 * Handles TRY AGAIN
 ******************************************************************************/

/******************************************************************************
 * Recovery 2 thread handling functions
 ******************************************************************************/

/**
 * Thread for handling recovery step 2
 * Wait for timeout or stop request
 * After timeout, recover all not already recovered clients
 *
 * @param int timeout_time_in Timeout time in s
 * @return nullptr
 */
static void *recovery2_thread(void *dummy) {
  int rc = 0;
  struct timespec seed_ts;
  int64_t timeout_ms;

  TRACE_ENTER();

  // Create a random timeout time in msec within an interval
  // Set seed. Use nanoseconds from clock */
  osaf_clock_gettime(CLOCK_MONOTONIC, &seed_ts);
  srandom((unsigned int)seed_ts.tv_nsec);
  timeout_ms = (int64_t)(random() % 100 + 400) * 1000;

  // Wait for timeout or a signal to terminate
  rc = osaf_poll_one_fd(state2_terminate_sel_obj.rmv_obj, timeout_ms);
  if (rc == -1) {
    TRACE("%s osaf_poll_one_fd Fail %s", __func__, strerror(errno));
    goto done;
  }

  if (rc == 0) {
    // Timeout; Set recovery state 2
    // Not allowed to continue if any API is in critical section
    // handling client data
    lga_recovery2_lock();
    set_lga_recovery_state(RecoveryState::kRecovery2);
    lga_recovery2_unlock();
    TRACE("%s Poll timeout. Enter kRecovery2 state", __func__);
  } else {
    //  Stop signal received
    TRACE("%s Stop signal received", __func__);
    goto done;
  }

  // Recover all log clients. During this time, any call of LOG APIs
  // return TRY_AGAIN until the recovery is done.
  LogAgent::instance()->RecoverAllLogClients();

  // All clients are recovered (or removed). Or recovery is aborted
  // Change to not recovering state RecoveryState::kNormal
  set_lga_recovery_state(RecoveryState::kNormal);
  TRACE("\t Setting lga_state = RecoveryState::kNormal");

done:
  // Cleanup and Exit thread
  (void)ncs_sel_obj_destroy(&state2_terminate_sel_obj);
  pthread_exit(nullptr);

  TRACE_LEAVE();
  return nullptr;
}

/**
 * Setup and start the thread for recovery state 2
 *
 * @return -1 on error
 */
static int start_recovery2_thread(void) {
  int rc = 0;
  pthread_attr_t attr;

  TRACE_ENTER();

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // Create a selection object for signaling the recovery2 thread
  uint32_t ncs_rc = ncs_sel_obj_create(&state2_terminate_sel_obj);
  if (ncs_rc != NCSCC_RC_SUCCESS) {
    TRACE("%s ncs_sel_obj_create Fail", __func__);
    rc = -1;
    goto done;
  }

  if (pthread_create(&recovery2_thread_id, &attr, recovery2_thread, nullptr) !=
      0) {
    TRACE("\t pthread_create FAILED: %s", strerror(errno));
    rc = -1;
    (void)ncs_sel_obj_destroy(&state2_terminate_sel_obj);
    goto done;
  }
  pthread_attr_destroy(&attr);

done:
  TRACE_LEAVE2("rc = %d", rc);
  return rc;
}

/**
 * Stops the recovery 2 thread
 * It is safe to call this function also if the thread is not running
 */
static void stop_recovery2_thread(void) {
  uint32_t ncs_rc = 0;
  int rc = 0;

  TRACE_ENTER();

  // Check if the thread is running
  if (is_lga_recovery_state(RecoveryState::kNormal) ||
      is_lga_recovery_state(RecoveryState::kNoLogServer)) {
    // No thread to stop
    TRACE("%s RecoveryState::kNormal no thread to stop", __func__);
    goto done;
  }

  // Signal the thread to terminate
  ncs_rc = ncs_sel_obj_ind(&state2_terminate_sel_obj);
  if (ncs_rc != NCSCC_RC_SUCCESS) {
    TRACE("%s ncs_sel_obj_ind Fail", __func__);
  }

  // Join thread to wait for thread termination
  rc = pthread_join(recovery2_thread_id, nullptr);
  if (rc != 0) {
    LOG_NO("%s: Could not join recovery2 thread %s", __func__, strerror(rc));
  }

done:
  TRACE_LEAVE();
  return;
}

/******************************************************************************
 * Recovery state handling functions
 ******************************************************************************/

/**
 * Recovery state RecoveryState::kNoLogServer
 *
 * Setup server down state
 * - Mark all clients and their streams as not recovered
 * - Remove recovery thread if exist
 * - Set RecoveryState::kNoLogServer state
 *
 */
void lga_no_server_state_set(void) {
  TRACE_ENTER();

  // Stop recovery thread for state 2 if exist
  stop_recovery2_thread();

  // Set RecoveryState::kNoLogServer state
  set_lga_recovery_state(RecoveryState::kNoLogServer);
  TRACE("\t lga_state = RecoveryState::kNoLogServer");

  TRACE_LEAVE();
}

/******************************************************************************
 * Recovery state 1
 *
 * Recover when needed. This means that when a client requests a service
 * the client and all its open streams is recovered in server.
 * Handler functions for this can be found in this section.
 * kRecovery1: State set in MDS event handler when server up event
 *             lga_serv_recov1state_set(). Starting recovery thread
 *
 * kRecovery2: State set in recovery thread after timeout
 *             lga_serv_recov2state_set()
 *
 * kNormal:  State set when all clients are recovered. This is done in
 *           recovery thread when done. After setting this state the
 *           thread will exit
 ******************************************************************************/

/**
 * If previous state was RecoveryState::kNoLogServer (headless)
 * Start the recovery2_thread. This will start timeout timer for recovery 2
 * state.
 * Set state to RecoveryState::kRecovery1
 */
void lga_serv_recov1state_set(void) {
  TRACE_ENTER();
  // Only do recovery if in headless state.
  if (is_lga_recovery_state(RecoveryState::kNoLogServer)) {
    set_lga_recovery_state(RecoveryState::kRecovery1);
    TRACE("lga_state = RECOVERY1");
    start_recovery2_thread();
  }
}

/* Clients APIs must not pass recovery 2 state check if
 * recovery 2 thread is started but state has not yet been changed by the
 * thread. Also the thread must not start recovery if an API is executing and
 * has passed the recovery 2 check
 */
static pthread_mutex_t lga_recov2_lock = PTHREAD_MUTEX_INITIALIZER;

void lga_recovery2_lock(void) { osaf_mutex_lock_ordie(&lga_recov2_lock); }

void lga_recovery2_unlock(void) { osaf_mutex_unlock_ordie(&lga_recov2_lock); }

typedef struct {
  RecoveryState state;
  pthread_mutex_t lock;
} lga_state_s;

static lga_state_s lga_state = {.state = RecoveryState::kNormal,
                                .lock = PTHREAD_MUTEX_INITIALIZER};

static void set_lga_recovery_state(RecoveryState state) {
  ScopeLock lock(lga_state.lock);
  lga_state.state = state;
}

bool is_lga_recovery_state(RecoveryState state) {
  bool rc = false;
  ScopeLock lock(lga_state.lock);
  if (state == lga_state.state) rc = true;
  return rc;
}

/**
 * Protect critical areas AIS functions handling global client data so that
 * this data is not handled at the same time by the recovery 2 thread
 * Lock must be done before checking if lga_state is RECOVERY2
 * Unlock must be done just before returning from function. It is allowed to
 * call lga_recovery2_unlock() also if no previous call to lock is done
 */
void recovery2_lock(bool *is_locked) {
  if (is_lga_recovery_state(RecoveryState::kRecovery1) ||
      is_lga_recovery_state(RecoveryState::kRecovery2)) {
    lga_recovery2_lock();
    *is_locked = true;
  }
}

void recovery2_unlock(bool *is_locked) {
  if (*is_locked) {
    *is_locked = false;
    lga_recovery2_unlock();
  }
}

//<
