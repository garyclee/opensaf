/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2016 The OpenSAF Foundation
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "rde/rded/role.h"
#include <cinttypes>
#include <cstdint>
#include "base/getenv.h"
#include "base/logtrace.h"
#include "base/ncs_main_papi.h"
#include "base/ncssysf_def.h"
#include "base/process.h"
#include "base/time.h"
#include "osaf/consensus/consensus.h"
#include "rde/rded/rde_cb.h"

const char* const Role::role_names_[] = {"Undefined", "ACTIVE",    "STANDBY",
                                         "QUIESCED",  "QUIESCING", "Invalid"};

const char* const Role::pre_active_script_ = PKGLIBDIR "/opensaf_sc_active";

PCS_RDA_ROLE Role::role() const { return role_; }

const char* Role::to_string(PCS_RDA_ROLE role) {
  return role >= 0 && role < sizeof(role_names_) / sizeof(role_names_[0])
             ? role_names_[role]
             : role_names_[0];
}

void Role::MonitorCallback(const std::string& key, const std::string& new_value,
                           SYSF_MBX mbx) {
  TRACE_ENTER();

  rde_msg* msg = static_cast<rde_msg*>(malloc(sizeof(rde_msg)));
  if (key == Consensus::kTakeoverRequestKeyname) {
    // don't send this to the main thread straight away, as it will
    // need some time to process topology changes.
    msg->type = RDE_MSG_TAKEOVER_REQUEST_CALLBACK;
    std::this_thread::sleep_for(std::chrono::seconds(4));
  } else {
    msg->type = RDE_MSG_NEW_ACTIVE_CALLBACK;
  }

  uint32_t status;
  status = m_NCS_IPC_SEND(&mbx, msg, NCS_IPC_PRIORITY_NORMAL);
  osafassert(status == NCSCC_RC_SUCCESS);
}

Role::Role(NODE_ID own_node_id)
    : known_nodes_{},
      role_{PCS_RDA_QUIESCED},
      own_node_id_{own_node_id},
      proc_{new base::Process()},
      election_end_time_{},
      discover_peer_timeout_{base::GetEnv("RDE_DISCOVER_PEER_TIMEOUT",
                                          kDefaultDiscoverPeerTimeout)},
      pre_active_script_timeout_{base::GetEnv(
          "RDE_PRE_ACTIVE_SCRIPT_TIMEOUT", kDefaultPreActiveScriptTimeout)} {}

timespec* Role::Poll(timespec* ts) {
  timespec* timeout = nullptr;
  if (role_ == PCS_RDA_UNDEFINED) {
    timespec now = base::ReadMonotonicClock();
    if (election_end_time_ >= now) {
      *ts = election_end_time_ - now;
      timeout = ts;
    } else {
      RDE_CONTROL_BLOCK* cb = rde_get_control_block();
      SaAisErrorT rc;
      Consensus consensus_service;

      rc = consensus_service.PromoteThisNode(true, cb->cluster_members.size());
      if (rc != SA_AIS_OK && rc != SA_AIS_ERR_EXIST) {
        LOG_ER("Unable to set active controller in consensus service");
        opensaf_reboot(0, nullptr,
                       "Unable to set active controller in consensus service");
      }

      if (rc == SA_AIS_ERR_EXIST) {
        LOG_WA("Another controller is already active");
        return timeout;
      }

      ExecutePreActiveScript();
      LOG_NO("Switched to ACTIVE from %s", to_string(role()));
      role_ = PCS_RDA_ACTIVE;
      rde_rda_send_role(role_);

      // register for callback if active controller is changed
      // in consensus service
      if (cb->monitor_lock_thread_running == false) {
        cb->monitor_lock_thread_running = true;
        consensus_service.MonitorLock(MonitorCallback, cb->mbx);
      }
      if (cb->monitor_takeover_req_thread_running == false) {
        cb->monitor_takeover_req_thread_running = true;
        consensus_service.MonitorTakeoverRequest(MonitorCallback, cb->mbx);
      }
    }
  }
  return timeout;
}

void Role::ExecutePreActiveScript() {
  int argc = 1;
  char* argv[] = {const_cast<char*>(pre_active_script_), nullptr};
  proc_->Execute(argc, argv,
                 std::chrono::milliseconds(pre_active_script_timeout_));
}

void Role::AddPeer(NODE_ID node_id) {
  auto result = known_nodes_.insert(node_id);
  if (result.second) ResetElectionTimer();
}

uint32_t Role::SetRole(PCS_RDA_ROLE new_role) {
  PCS_RDA_ROLE old_role = role_;
  if (new_role == PCS_RDA_ACTIVE &&
      (old_role == PCS_RDA_UNDEFINED || old_role == PCS_RDA_QUIESCED)) {
    LOG_NO("Requesting ACTIVE role");
    new_role = PCS_RDA_UNDEFINED;
  }
  if (new_role != old_role) {
    LOG_NO("RDE role set to %s", to_string(new_role));
    if (new_role == PCS_RDA_ACTIVE) {
      ExecutePreActiveScript();

      // register for callback if active controller is changed
      // in consensus service
      Consensus consensus_service;
      RDE_CONTROL_BLOCK* cb = rde_get_control_block();
      if (cb->monitor_lock_thread_running == false) {
        cb->monitor_lock_thread_running = true;
        consensus_service.MonitorLock(MonitorCallback, cb->mbx);
      }
      if (cb->monitor_takeover_req_thread_running == false) {
        cb->monitor_takeover_req_thread_running = true;
        consensus_service.MonitorTakeoverRequest(MonitorCallback, cb->mbx);
      }
    }
    role_ = new_role;
    if (new_role == PCS_RDA_UNDEFINED) {
      known_nodes_.clear();
      ResetElectionTimer();
    } else {
      rde_rda_send_role(new_role);
    }
  }
  return UpdateMdsRegistration(new_role, old_role);
}

void Role::ResetElectionTimer() {
  election_end_time_ = base::ReadMonotonicClock() +
                       base::MillisToTimespec(discover_peer_timeout_);
}

uint32_t Role::UpdateMdsRegistration(PCS_RDA_ROLE new_role,
                                     PCS_RDA_ROLE old_role) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  bool mds_registered_before = old_role != PCS_RDA_QUIESCED;
  bool mds_registered_after = new_role != PCS_RDA_QUIESCED;
  if (mds_registered_after != mds_registered_before) {
    if (mds_registered_after) {
      if (rde_mds_register() != NCSCC_RC_SUCCESS) {
        LOG_ER("rde_mds_register() failed");
        rc = NCSCC_RC_FAILURE;
      }
    } else {
      if (rde_mds_unregister() != NCSCC_RC_SUCCESS) {
        LOG_ER("rde_mds_unregister() failed");
        rc = NCSCC_RC_FAILURE;
      }
    }
  }
  return rc;
}

void Role::SetPeerState(PCS_RDA_ROLE node_role, NODE_ID node_id) {
  if (role() == PCS_RDA_UNDEFINED) {
    if (node_role == PCS_RDA_ACTIVE || node_role == PCS_RDA_STANDBY ||
        (node_role == PCS_RDA_UNDEFINED && node_id < own_node_id_)) {
      SetRole(PCS_RDA_QUIESCED);
      LOG_NO("Giving up election against 0x%" PRIx32
             " with role %s. "
             "My role is now %s",
             node_id, to_string(node_role), to_string(role()));
    }
  }
}
