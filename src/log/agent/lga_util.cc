/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2008, 2017 - All Rights Reserved.
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

#include "log/agent/lga_util.h"
#include <stdlib.h>
#include <syslog.h>
#include "base/ncs_hdl_pub.h"
#include "base/ncs_main_papi.h"
#include "base/osaf_poll.h"
#include "log/agent/lga_state.h"
#include "log/agent/lga_mds.h"
#include "base/osaf_extended_name.h"
#include "base/osaf_utility.h"
#include "log/agent/lga_agent.h"
#include "log/agent/lga_common.h"

// Variables used during startup/shutdown only
static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int client_counter = 0;

/**
 * @return unsigned int
 */
static unsigned int lga_create() {
  unsigned int rc = NCSCC_RC_SUCCESS;

  // Register with MDS
  if ((NCSCC_RC_SUCCESS != (rc = lga_mds_init()))) {
    TRACE("lga_mds_init FAILED");
    rc = NCSCC_RC_FAILURE;
    // Delete the lga init instances
    LogAgent::instance()->RemoveAllLogClients();
    return rc;
  }

  // Wait for log server up
  rc = LogAgent::instance()->WaitLogServerUp(LGS_WAIT_TIME);
  if (rc != NCSCC_RC_SUCCESS) {
    TRACE("WaitLogServerUp FAILED");
    // Delete the lga init instances
    LogAgent::instance()->RemoveAllLogClients();
    // Unregister MDS
    lga_mds_deinit();
    return rc;
  }

  return rc;
}

/**
 * Delete log agent
 *
 * @return unsigned int
 */
static unsigned int lga_delete() {
  unsigned int rc = NCSCC_RC_SUCCESS;

  // Unregister in MDS
  rc = lga_mds_deinit();
  if (rc != NCSCC_RC_SUCCESS) {
    TRACE("lga_mds_deinit FAILED");
    rc = NCSCC_RC_FAILURE;
  }

  return rc;
}

/**
 * Initiate the agent when first used.
 * Start NCS service
 * Register with MDS
 *
 * @return unsigned int
 */
unsigned int lga_startup() {
  unsigned int rc = NCSCC_RC_SUCCESS;
  ScopeLock lock(init_lock);
  std::atomic<MDS_HDL>& mds_hdl = LogAgent::instance()->atomic_get_mds_hdl();

  TRACE_ENTER();

  if (mds_hdl == 0) {
    if ((rc = ncs_agents_startup()) != NCSCC_RC_SUCCESS) {
      TRACE("ncs_agents_startup FAILED");
      goto done;
    }

    if ((rc = lga_create()) != NCSCC_RC_SUCCESS) {
      mds_hdl = 0;
      ncs_agents_shutdown();
      goto done;
    }
  }

done:
  TRACE_LEAVE2("rc: %u", rc);
  return rc;
}

/**
 * Shutdown the agent when not in use
 * Stop NCS service and unregister MDS
 *
 * @return unsigned int
 */
unsigned int lga_shutdown() {
  unsigned int rc = NCSCC_RC_SUCCESS;
  ScopeLock lock(init_lock);
  std::atomic<MDS_HDL>& mds_hdl = LogAgent::instance()->atomic_get_mds_hdl();
  TRACE_ENTER();

  if (mds_hdl) {
    rc = lga_delete();
    if (rc != NCSCC_RC_SUCCESS) {
      TRACE("lga_delete FAILED");
      goto done;
    }
    ncs_agents_shutdown();
    mds_hdl = 0;
  }

done:
  TRACE_LEAVE2("rc: %u", rc);
  return rc;
}

/**
 * Increase user counter
 * The function help to trace number of clients
 */
void lga_increase_user_counter(void) {
  ScopeLock lock(init_lock);

    ++client_counter;
    TRACE_2("client_counter: %u", client_counter);
}

/**
 * Decrease user counter
 * The function help to trace number of clients
 */
void lga_decrease_user_counter(void) {
  TRACE_ENTER2("client_counter: %u", client_counter);
  ScopeLock lock(init_lock);

  if (client_counter > 0)
    --client_counter;
}

/**
 * Get number of user
 */
unsigned int lga_get_number_of_user(void){
  ScopeLock lock(init_lock);

  return client_counter;
}

/**
 * Check if the name is valid or not.
 */
bool lga_is_extended_name_valid(const SaNameT* name) {
  if (name == nullptr) return false;
  if (osaf_is_extended_name_valid(name) == false) return false;

  if (osaf_extended_name_length(name) > kOsafMaxDnLength) {
    return false;
  }

  return true;
}

/*
 * To enable tracing early in saLogInitialize, use a GCC constructor
 */
__attribute__((constructor)) void logtrace_init_constructor(void) {
  char* value;

  // Initialize trace system first of all so we can see what is going.
  if ((value = getenv("LOGSV_TRACE_PATHNAME")) != nullptr) {
    if (logtrace_init("lga", value, CATEGORY_ALL) != 0) {
      // Error, we cannot do anything
      return;
    }
  }
}
