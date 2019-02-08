/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2018 Ericsson AB 2018 - All Rights Reserved.
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
 */
#ifndef OSAF_CONSENSUS_CONSENSUS_H_
#define OSAF_CONSENSUS_CONSENSUS_H_

#include <chrono>
#include <string>
#include <vector>
#include "base/macros.h"
#include "osaf/consensus/key_value.h"
#include "saAis.h"

class Consensus {
 public:
  // Set active controller to this node
  SaAisErrorT PromoteThisNode(const bool graceful_takeover,
                              const uint64_t cluster_size);

  // Clear current active controller by releasing lock
  SaAisErrorT DemoteCurrentActive();

  // Clear this node as active controller by releasing lock
  SaAisErrorT DemoteThisNode();

  // Returns active controller as known by the consensus service
  std::string CurrentActive() const;

  // If the active controller is changed as known by the consensus service,
  // then callback will be run from a new thread, with <user_defined> returned
  // in the callback
  void MonitorLock(ConsensusCallback callback, const uint32_t user_defined);

  void MonitorTakeoverRequest(ConsensusCallback callback,
                              const uint32_t user_defined);

  // Retrieve takeover request (if any)
  SaAisErrorT ReadTakeoverRequest(std::string& request);

  // Is consensus service enabled?
  bool IsEnabled() const;

  // Is the key-value store writable?
  bool IsWritable() const;

  // Is remote fencing enabled?
  bool IsRemoteFencingEnabled() const;

  // Is relaxed node promotion enabled?
  bool IsRelaxedNodePromotionEnabled() const;

  bool PrioritisePartitionSize() const;

  // Determine if plugin is telling us to self-fence due to loss
  // of connectivity to the KV store
  bool SelfFence(const std::string& request) const;

  bool ReloadConfiguration();
  std::string PluginPath() const;

  Consensus();
  virtual ~Consensus();

  static const std::string kTakeoverRequestKeyname;

  enum class TakeoverState : std::uint8_t {
    UNDEFINED = 0,
    NEW = 1,
    ACCEPTED = 2,
    REJECTED = 3
  };

  enum class TakeoverElements : std::uint8_t {
    CURRENT_OWNER = 0,
    PROPOSED_OWNER = 1,
    PROPOSED_NETWORK_SIZE = 2,
    STATE = 3
  };

  const std::string TakeoverStateStr[4] = {"UNDEFINED", "NEW", "ACCEPTED",
                                           "REJECTED"};

  TakeoverState HandleTakeoverRequest(const uint64_t cluster_size,
                                      const std::string& request);

 private:
  bool use_consensus_{false};
  bool use_remote_fencing_{false};
  bool prioritise_partition_size_{true};
  bool relaxed_node_promotion_{false};
  uint32_t takeover_valid_time_{20};
  uint32_t max_takeover_retry_{0};
  std::string config_file_{};
  std::string plugin_path_{};

  const std::string kTestKeyname = "opensaf_write_test";
  const std::string kFmsEnvPrefix = "FMS";
  const std::chrono::milliseconds kSleepInterval =
      std::chrono::milliseconds(1000);  // in ms
  static constexpr uint32_t kLockTimeout = 0;  // lock is persistent by default
  static constexpr uint32_t kMaxRetry = 3;

  void CheckForExistingTakeoverRequest();

  SaAisErrorT CreateTakeoverRequest(const std::string& current_owner,
                                    const std::string& proposed_owner,
                                    const uint64_t cluster_size);

  SaAisErrorT ParseTakeoverRequest(const std::string& request,
                                   std::vector<std::string>& tokens) const;
  SaAisErrorT ReadTakeoverRequest(std::vector<std::string>& tokens);

  SaAisErrorT WriteTakeoverResult(const std::string& current_owner,
                                  const std::string& proposed_owner,
                                  const std::string& proposed_cluster_size,
                                  const TakeoverState result);

  SaAisErrorT RemoveTakeoverRequest();

  SaAisErrorT Demote(const std::string& node);
  bool FenceNode(const std::string& node);

  void Split(const std::string& str, std::vector<std::string>& tokens) const;
  void ProcessEnvironmentSettings();

  DELETE_COPY_AND_MOVE_OPERATORS(Consensus);
};

#endif  // OSAF_CONSENSUS_CONSENSUS_H_
