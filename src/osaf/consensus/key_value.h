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
#ifndef OSAF_CONSENSUS_KEY_VALUE_H_
#define OSAF_CONSENSUS_KEY_VALUE_H_

#include <functional>
#include <string>
#include <thread>
#include "saAis.h"

typedef std::function<void(const std::string& key, const std::string& new_value,
                           const uint32_t user_defined)>
    ConsensusCallback;

class KeyValue {
 public:
  // Retrieve value of key
  static SaAisErrorT Get(const std::string& key, std::string& value);

  // Set key to value
  static SaAisErrorT Set(const std::string& key, const std::string& value,
                         const unsigned int timeout);

  // Set key to value only if prev value matches
  static SaAisErrorT Set(const std::string& key, const std::string& value,
                         const std::string& prev_value,
                         const unsigned int timeout);

  // Create key, and set to value. Fails if key already exists.
  static SaAisErrorT Create(const std::string& key, const std::string& value,
                            const unsigned int timeout);

  // Erase key
  static SaAisErrorT Erase(const std::string& key);

  // Obtain lock, default timeout is 0 seconds (indefinite). If lock
  // is called when already locked, the timeout is extended
  static SaAisErrorT Lock(const std::string& owner,
                          const unsigned int timeout = 0);

  // Release lock
  static SaAisErrorT Unlock(const std::string& owner);

  // An empty string is returned if there is an error
  static SaAisErrorT LockOwner(std::string& owner);

  // starts a thread to watch key and call callback if values changes
  static void Watch(const std::string& key, ConsensusCallback callback,
                    const uint32_t user_defined);

  // starts a thread to watch the lock and call callback if is modified
  static void WatchLock(ConsensusCallback callback,
                        const uint32_t user_defined);

  // internal use
  static int Execute(const std::string& command, std::string& output);
};

#endif  // OSAF_CONSENSUS_KEY_VALUE_H_
