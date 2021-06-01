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
#include "osaf/consensus/consensus_env.h"
#include "base/getenv.h"
#include "base/logtrace.h"
#include "base/config_file_reader.h"

ConsensusEnv& ConsensusEnv::GetInstance() {
    TRACE_ENTER();
    static ConsensusEnv env;

    return env;
}

ConsensusCfg ConsensusEnv::GetConfiguration() {
    TRACE_ENTER();
    base::Lock lock(mutex_);

    return cfg_;
}

bool ConsensusEnv::ReloadConfiguration() {
    TRACE_ENTER();
    ConfigFileReader reader;
    ConfigFileReader::SettingsMap map;
    base::Lock lock(mutex_);

    if (cfg_.config_file.empty() == true) {
        LOG_ER("config file not defined");
        return false;
    }
    map = reader.ParseFile(cfg_.config_file);
    for (const auto& kv : map) {
        if (kv.first.compare(0, kFmsEnvPrefix.size(), kFmsEnvPrefix) != 0) {
            // we only care about environment variables beginning with 'FMS'
            continue;
        }
        int rc;
        TRACE("Setting '%s' to '%s'", kv.first.c_str(), kv.second.c_str());
        rc = setenv(kv.first.c_str(), kv.second.c_str(), 1);
        osafassert(rc == 0);
    }
    LoadEnv();

    return true;
}

ConsensusEnv::ConsensusEnv() {
    TRACE_ENTER();

    base::Lock lock(mutex_);
    cfg_.use_consensus = false;
    cfg_.use_remote_fencing = false;
    cfg_.prioritise_partition_size = true;
    cfg_.prioritise_partition_size_mds_wait_time = 4;
    cfg_.relaxed_node_promotion = false;
    cfg_.takeover_valid_time = 20;
    cfg_.max_takeover_retry = 0;
    cfg_.plugin_path = "";
    cfg_.config_file = "";
    LoadEnv();
}

ConsensusEnv::~ConsensusEnv() {
    TRACE_ENTER();
}

void ConsensusEnv::LoadEnv() {
    TRACE_ENTER();
    uint32_t split_brain_enable = base::GetEnv("FMS_SPLIT_BRAIN_PREVENTION", 0);
    cfg_.plugin_path = base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
    uint32_t use_remote_fencing = base::GetEnv("FMS_USE_REMOTE_FENCING", 0);
    uint32_t prioritise_partition_size =
        base::GetEnv("FMS_TAKEOVER_PRIORITISE_PARTITION_SIZE", 1);
    uint32_t prioritise_partition_size_mds_wait_time =
        base::GetEnv("FMS_TAKEOVER_PRIORITISE_PARTITION_SIZE_MDS_WAIT_TIME", 4);
    uint32_t relaxed_node_promotion =
        base::GetEnv("FMS_RELAXED_NODE_PROMOTION", 0);

    cfg_.config_file = base::GetEnv("FMS_CONF_FILE", "");
    // if not specified in fmd.conf,
    // takeover requests are valid for 20 seconds
    cfg_.takeover_valid_time =
        base::GetEnv("FMS_TAKEOVER_REQUEST_VALID_TIME", 20);
    // expiration time of takeover request is twice the max wait time
    cfg_.max_takeover_retry = cfg_.takeover_valid_time / 2;

    if (split_brain_enable == 1 && cfg_.plugin_path.empty() == false) {
        cfg_.use_consensus = true;
    } else {
        cfg_.use_consensus = false;
    }

    if (use_remote_fencing == 1) {
        cfg_.use_remote_fencing = true;
    }

    if (prioritise_partition_size == 0) {
        cfg_.prioritise_partition_size = false;
    }

    if (cfg_.use_consensus == true && relaxed_node_promotion == 1) {
        cfg_.relaxed_node_promotion = true;
    }

    cfg_.prioritise_partition_size_mds_wait_time =
        prioritise_partition_size_mds_wait_time;
}
