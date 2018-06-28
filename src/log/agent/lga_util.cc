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

  // Create and init sel obj for mds sync
  NCS_SEL_OBJ* lgs_sync_sel = LogAgent::instance().get_lgs_sync_sel();
  m_NCS_SEL_OBJ_CREATE(lgs_sync_sel);
  std::atomic<bool>& lgs_sync_wait =
      LogAgent::instance().atomic_get_lgs_sync_wait();
  lgs_sync_wait = true;

  // register with MDS
  if ((NCSCC_RC_SUCCESS != (rc = lga_mds_init()))) {
    rc = NCSCC_RC_FAILURE;
    // Delete the lga init instances
    LogAgent::instance().RemoveAllLogClients();
    return rc;
  }

  // Block and wait for indication from MDS meaning LGS is up

  // #1179 Change timeout from 30 sec (30000) to 10 sec (10000)
  // 30 sec is probably too long for a synchronous API function
  NCS_SEL_OBJ sel = *lgs_sync_sel;
  int fd = m_GET_FD_FROM_SEL_OBJ(sel);
  osaf_poll_one_fd(fd, 10000);

  lgs_sync_wait = false;
  std::atomic<SaClmClusterChangesT>& clm_state =
      LogAgent::instance().atomic_get_clm_node_state();
  clm_state = SA_CLM_NODE_JOINED;

  // No longer needed
  m_NCS_SEL_OBJ_DESTROY(lgs_sync_sel);
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
  std::atomic<MDS_HDL>& mds_hdl = LogAgent::instance().atomic_get_mds_hdl();

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
