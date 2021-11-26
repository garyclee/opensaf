/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2017 - All Rights Reserved.
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

#ifndef SRC_LOG_AGENT_LGA_STATE_H_
#define SRC_LOG_AGENT_LGA_STATE_H_

// Internal agent recovery states
enum class RecoveryState {
  // Server is up and no recovery is ongoing
  kNormal,
  // No Server (Server down) state
  kNoLogServer,
  // Server is up. Recover clients and streams when request from client.
  // Recovery1 timer is running
  kRecovery1,
  // Auto recover remaining clients and streams after recovery1 timeout
  kRecovery2
};

void lga_no_server_state_set();
void lga_serv_recov1state_set();
void lga_recovery2_lock();
void lga_recovery2_unlock();

bool is_lga_recovery_state(RecoveryState state);
void stop_recovery2_thread();
void recovery2_lock(bool *is_locked);
void recovery2_unlock(bool *is_locked);

#endif  // SRC_LOG_AGENT_LGA_STATE_H_
