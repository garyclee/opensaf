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
#ifndef OSAF_CONSENSUS_CONSENSUS_ENV_H_
#define OSAF_CONSENSUS_CONSENSUS_ENV_H_

#include <string>
#include "base/mutex.h"

typedef struct _ConsensusCfg {
    bool use_consensus;
    bool use_remote_fencing;
    bool prioritise_partition_size;
    uint32_t prioritise_partition_size_mds_wait_time;
    bool relaxed_node_promotion;
    uint32_t takeover_valid_time;
    uint32_t max_takeover_retry;
    std::string plugin_path;
    std::string config_file;
} ConsensusCfg;

class ConsensusEnv {
 public:
    // Get the instance of class
    static ConsensusEnv &GetInstance();
    // Get a copy of configurations
    ConsensusCfg GetConfiguration();
    // Update the environment variables from the configuration file,
    // and update the configurations
    bool ReloadConfiguration();
    virtual ~ConsensusEnv();

 private:
    base::Mutex mutex_;
    ConsensusCfg cfg_;
    const std::string kFmsEnvPrefix = "FMS";

    // Private constructor
    ConsensusEnv();
    // Load configurations from the environment variables
    void LoadEnv();
};

#endif  // OSAF_CONSENSUS_CONSENSUS_ENV_H_
