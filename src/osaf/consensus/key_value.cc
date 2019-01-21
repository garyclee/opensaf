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
#include "osaf/consensus/key_value.h"
#include <sys/wait.h>
#include "base/conf.h"
#include "base/getenv.h"
#include "base/logtrace.h"

int KeyValue::Execute(const std::string& command, std::string& output) {
  TRACE_ENTER();
  constexpr size_t buf_size = 128;
  std::array<char, buf_size> buffer;
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return 1;
  }
  output.clear();
  while (feof(pipe) == 0) {
    if (fgets(buffer.data(), buf_size, pipe) != nullptr) {
      output += buffer.data();
    }
  }
  int exit_code = pclose(pipe);
  exit_code = WEXITSTATUS(exit_code);
  if (output.empty() == false && isspace(output.back()) != 0) {
    // remove newline at end of output
    output.pop_back();
  }
  TRACE("Executed '%s', returning %d", command.c_str(), exit_code);
  return exit_code;
}

SaAisErrorT KeyValue::Get(const std::string& key, std::string& value) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " get \"" + key + "\"");
  int rc = KeyValue::Execute(command, value);
  TRACE("Read '%s'", value.c_str());

  if (rc == 0) {
    return SA_AIS_OK;
  } else if (rc == 1) {
    return SA_AIS_ERR_INVALID_PARAM;
  } else {
    return SA_AIS_ERR_FAILED_OPERATION;
  }
}

SaAisErrorT KeyValue::Set(const std::string& key, const std::string& value,
                          const unsigned int timeout) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " set \"" + key + "\" \"" + value +
                            "\" " + std::to_string(timeout));
  std::string output;
  int rc = KeyValue::Execute(command, output);

  if (rc == 0) {
    return SA_AIS_OK;
  } else {
    return SA_AIS_ERR_FAILED_OPERATION;
  }
}

SaAisErrorT KeyValue::Set(const std::string& key, const std::string& value,
                          const std::string& prev_value,
                          const unsigned int timeout) {
  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " set_if_prev \"" + key + "\" \"" +
                            value + "\" \"" + prev_value +
                            "\" " + std::to_string(timeout));
  std::string output;
  int rc = KeyValue::Execute(command, output);

  if (rc == 0) {
    return SA_AIS_OK;
  } else {
    return SA_AIS_ERR_FAILED_OPERATION;
  }
}

SaAisErrorT KeyValue::Create(const std::string& key, const std::string& value,
                             const unsigned int timeout) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " create \"" + key + "\" \"" +
                            value + "\" " + std::to_string(timeout));
  std::string output;
  int rc = KeyValue::Execute(command, output);

  if (rc == 0) {
    return SA_AIS_OK;
  } else if (rc == 1) {
    return SA_AIS_ERR_EXIST;
  } else if (rc == 2) {
    return SA_AIS_ERR_INVALID_PARAM;
  } else {
    return SA_AIS_ERR_FAILED_OPERATION;
  }
}

SaAisErrorT KeyValue::Erase(const std::string& key) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " erase \"" + key + "\"");
  std::string output;
  int rc = KeyValue::Execute(command, output);

  if (rc == 0) {
    return SA_AIS_OK;
  } else {
    return SA_AIS_ERR_FAILED_OPERATION;
  }
}

SaAisErrorT KeyValue::Lock(const std::string& owner,
                           const unsigned int timeout) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " lock \"" + owner + "\" " +
                            std::to_string(timeout));
  std::string output;
  int rc = KeyValue::Execute(command, output);

  if (rc == 0) {
    return SA_AIS_OK;
  } else if (rc == 1) {
    // already locked
    LOG_NO("Locked failed: %s", output.c_str());
    return SA_AIS_ERR_EXIST;
  } else {
    LOG_NO("Locked failed: %s", output.c_str());
    return SA_AIS_ERR_TRY_AGAIN;
  }
}

SaAisErrorT KeyValue::Unlock(const std::string& owner) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " unlock \"" + owner + "\"");
  std::string output;
  int rc = Execute(command, output);

  if (rc == 0) {
    return SA_AIS_OK;
  } else if (rc == 1) {
    LOG_NO("Unlock failed: %s", output.c_str());
    LOG_ER("Lock is owned by another node");
    return SA_AIS_ERR_INVALID_PARAM;
  } else {
    return SA_AIS_ERR_TRY_AGAIN;
  }
}

SaAisErrorT KeyValue::LockOwner(std::string& owner) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " lock_owner");
  std::string output;
  int rc = KeyValue::Execute(command, output);

  if (rc == 0) {
    TRACE("Lock owner is %s", output.c_str());
    owner = output;
    return SA_AIS_OK;
  }

  // put output in owner, for debugging purposes
  owner = output;
  return SA_AIS_ERR_FAILED_OPERATION;
}

namespace {

static constexpr std::chrono::milliseconds kSleepInterval =
    std::chrono::milliseconds(100);  // in ms
static constexpr uint32_t kMaxRetry = 100;

void WatchKeyFunction(const std::string& key, const ConsensusCallback& callback,
                      const uint32_t user_defined) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " watch \"" + key + "\"");
  std::string value;
  uint32_t retries = 0;
  int rc;

  rc = KeyValue::Execute(command, value);
  while (rc != 0 && rc < 126 && retries < kMaxRetry) {
    ++retries;
    std::this_thread::sleep_for(kSleepInterval);
    rc = KeyValue::Execute(command, value);
  }

  if (rc == 0) {
    TRACE("Read '%s'", value.c_str());
    callback(key, value, user_defined);
  } else {
    LOG_ER("Failed to watch %s", key.c_str());
    osafassert(false);
  }
}

void WatchLockFunction(const ConsensusCallback& callback,
                       const uint32_t user_defined) {
  TRACE_ENTER();

  const std::string kv_store_cmd =
      base::GetEnv("FMS_KEYVALUE_STORE_PLUGIN_CMD", "");
  const std::string command(kv_store_cmd + " watch_lock");
  std::string value;
  uint32_t retries = 0;
  int rc;

  rc = KeyValue::Execute(command, value);
  while (rc != 0 && rc < 126 && retries < kMaxRetry) {
    ++retries;
    std::this_thread::sleep_for(kSleepInterval);
    rc = KeyValue::Execute(command, value);
  }

  if (rc == 0) {
    TRACE("Read '%s'", value.c_str());
    callback("WatchLockFunction", value, user_defined);
  } else {
    LOG_ER("Failed to watch lock");
    osafassert(false);
  }
}

}  // namespace

void KeyValue::Watch(const std::string& key, const ConsensusCallback callback,
                     const uint32_t user_defined) {
  std::thread t(WatchKeyFunction, key, callback, user_defined);
  t.detach();
  return;
}

void KeyValue::WatchLock(const ConsensusCallback callback,
                         const uint32_t user_defined) {
  std::thread t(WatchLockFunction, callback, user_defined);
  t.detach();
  return;
}
