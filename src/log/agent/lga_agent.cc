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

#include "log/agent/lga_agent.h"
#include <string.h>
#include <algorithm>
#include "base/ncs_hdl_pub.h"
#include "base/saf_error.h"
#include "log/agent/lga_mds.h"
#include "log/agent/lga_state.h"
#include "log/agent/lga_util.h"
#include "base/osaf_extended_name.h"
#include "log/agent/lga_client.h"
#include "log/agent/lga_stream.h"

//------------------------------------------------------------------------------
// ScopeData
//------------------------------------------------------------------------------

//>
// Introduce this class is to avoid using @goto, and do following stuffs
// when the object runs out of its scope:
// 1) RestoreRefCounter() on @LogClient and @LogStreamInfo objectstate
// 2) recovery_unlock2()
//
// The data, @LogStreamInfo object, @LogClient object and recovery lock
// will be restored appropriately if the data run out of scope.
//
// Usage example:
// LogClient* client = nullptr;
// LogStreamInfo* stream = nullptr;
// ScopeData::LogClientData client_data{client, &cUpdated, kIncOne};
// ScopeData::LogStreamInfoData stream_data{stream, &sUpdated, kDecOne};
// ScopeData data{&client_data, &stream_data, &is_locked};
//<
class ScopeData {
 public:
  struct LogClientData {
    // Alias of @LogClient object.
    LogClient*& client;
    // True if ref counter needs to be restored
    bool* is_updated;
    // The increased value if @is_updated set. The aim of using enum instead
    // of `int` type is to force the caller pass either value (1) or value (-1).
    RefCounter::Degree value;
    // Who is the caller of `RestoreRefCounter`
    std::string caller;
  };

  struct LogStreamInfoData {
    // Alias of @LogStreamInfo object.
    LogStreamInfo*& stream;
    // True if ref counter needs to be restored
    bool* is_updated;
    // The increased value if @is_updated set. The aim of using enum instead
    // of `int` type is to force the caller pass either value (1) or value (-1).
    RefCounter::Degree value;
    // Who is the caller of `RestoreRefCounter`
    std::string caller;
  };

  explicit ScopeData(LogClientData*, bool* lock = nullptr);
  explicit ScopeData(LogClientData*, LogStreamInfoData*, bool* lock = nullptr);
  ~ScopeData();

 private:
  // Data information passed by @LogClient.
  LogClientData* client_data_;

  // Data information passed by @LogStreamInfoData.
  // Do nothing if the pointer is nullptr.
  LogStreamInfoData* stream_data_;

  // Do unlock the recovery mutex if true.
  bool* is_locked_;

  DELETE_COPY_AND_MOVE_OPERATORS(ScopeData);
};

// Use this constructor if require restoring @LogClientInfo object
// and/or unlock recovery mutex.
ScopeData::ScopeData(LogClientData* client, bool* lock)
    : client_data_{client}, stream_data_{nullptr}, is_locked_{lock} {}

// Use this constructor if require restoring @LogClientInfo,
// @LogStreamInfo and/or unlock recovery mutex.
ScopeData::ScopeData(LogClientData* client, LogStreamInfoData* stream,
                     bool* lock)
    : client_data_{client}, stream_data_{stream}, is_locked_{lock} {}

ScopeData::~ScopeData() {
  // Do release the recovery lock if previous is locked.
  if (is_locked_ != nullptr) {
    recovery2_unlock(is_locked_);
  }

  LogAgent::instance().EnterCriticalSection();
  LogClient* client = client_data_->client;
  bool* is_updated = client_data_->is_updated;
  RefCounter::Degree client_degree = client_data_->value;
  const char* caller = client_data_->caller.c_str();
  if (client != nullptr) {
    // Do restore the reference counter if the client exists.
    client->RestoreRefCounter(caller, client_degree, *is_updated);
  }

  if (stream_data_ != nullptr) {
    LogStreamInfo* stream = stream_data_->stream;
    bool* stream_is_updated = stream_data_->is_updated;
    RefCounter::Degree stream_degree = stream_data_->value;
    const char* caller = stream_data_->caller.c_str();
    if (stream != nullptr) {
      // Do restore the reference counter if the stream exists.
      stream->RestoreRefCounter(caller, stream_degree, *stream_is_updated);
    }
  }
  LogAgent::instance().LeaveCriticalSection();
}

//------------------------------------------------------------------------------
// LogAgent
//------------------------------------------------------------------------------
// Singleton represents LOG agent.
LogAgent LogAgent::me_;

LogAgent::LogAgent() {
  client_list_.clear();
  // There is high risk of calling one @LogClient method
  // in the body of other @LogClient methods, such case would cause deadlock
  // even they are in the same thread context.
  // To avoid such risk, use RECURSIVE MUTEX for @LogClient
  pthread_mutexattr_t attr;
  int result = pthread_mutexattr_init(&attr);
  assert(result == 0 && "Failed to init mutex attribute");

  result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  assert(result == 0 && "Failed to set mutex type");

  result = pthread_mutex_init(&mutex_, nullptr);
  assert(result == 0 && "Failed to init `mutex_`");

  pthread_mutexattr_destroy(&attr);

  // Initialize @get_delete_obj_sync_mutex_
  result = pthread_mutex_init(&get_delete_obj_sync_mutex_, nullptr);
  assert(result == 0 && "Failed to init `get_delete_obj_sync_mutex_`");
}

void LogAgent::PopulateOpenParams(
    const char* dn, uint32_t client_id,
    SaLogFileCreateAttributesT_2* logFileCreateAttributes,
    SaLogStreamOpenFlagsT logStreamOpenFlags,
    lgsv_stream_open_req_t* open_param) {
  TRACE_ENTER();
  assert(open_param != nullptr);
  assert(dn != nullptr);

  open_param->client_id = client_id;
  osaf_extended_name_lend(dn, &open_param->lstr_name);

  if (logFileCreateAttributes == nullptr || is_well_known_stream(dn) == true) {
    open_param->logFileFmt = nullptr;
    open_param->logFileFmtLength = 0;
    open_param->maxLogFileSize = 0;
    open_param->maxLogRecordSize = 0;
    open_param->haProperty = SA_FALSE;
    open_param->logFileFullAction = SA_LOG_FULL_ACTION_BEGIN;
    open_param->maxFilesRotated = 0;
    open_param->lstr_open_flags = 0;
  } else {
    // Server will assign a def fmt string if needed (logFileFmt = nullptr)
    open_param->logFileFmt = logFileCreateAttributes->logFileFmt;
    open_param->maxLogFileSize = logFileCreateAttributes->maxLogFileSize;
    open_param->maxLogRecordSize = logFileCreateAttributes->maxLogRecordSize;
    open_param->haProperty = logFileCreateAttributes->haProperty;
    open_param->logFileFullAction = logFileCreateAttributes->logFileFullAction;
    open_param->maxFilesRotated = logFileCreateAttributes->maxFilesRotated;
    open_param->lstr_open_flags = logStreamOpenFlags;
  }
}

LogStreamInfo* LogAgent::SearchLogStreamInfoByHandle(SaLogStreamHandleT h) {
  TRACE_ENTER();
  LogStreamInfo* stream;
  stream = static_cast<LogStreamInfo*>(ncshm_take_hdl(NCS_SERVICE_ID_LGA, h));
  ncshm_give_hdl(h);
  return stream;
}

// Search for client in the @client_list_ based on client @handle
LogClient* LogAgent::SearchClientByHandle(uint32_t handle) {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  LogClient* client = nullptr;
  for (const auto& c : client_list_) {
    if (c != nullptr && c->GetHandle() == handle) {
      client = c;
      break;
    }
  }
  return client;
}

// Search for client in the @client_list_ based on client @id
LogClient* LogAgent::SearchClientById(uint32_t id) {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  LogClient* client = nullptr;
  for (const auto& c : client_list_) {
    if (c != nullptr && c->GetClientId() == id) {
      client = c;
      break;
    }
  }
  return client;
}

// Remove one client @client from the list @client_list_
// This method should be called in @saLogFinalize() context
void LogAgent::RemoveLogClient(LogClient** client) {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  assert(*client != nullptr);
  auto it = std::remove(client_list_.begin(), client_list_.end(), *client);
  if (it != client_list_.end()) {
    client_list_.erase(it, client_list_.end());
    delete *client;
    *client = nullptr;
    lga_decrease_user_counter();
    return;
  }
  // We hope it will never come to this line
  assert(0);
}

void LogAgent::RemoveAllLogClients() {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  for (auto& client : client_list_) {
    if (client == nullptr) continue;
    delete client;
  }
}

// Add one client @client to the list @client_list_
// This method should be called in @saLogInitialize() context
void LogAgent::AddLogClient(LogClient* client) {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  assert(client != nullptr);
  client_list_.push_back(client);
}

// Do recover all log clients in list @client_list_
bool LogAgent::RecoverAllLogClients() {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  for (auto& client : client_list_) {
    // We just get SC absence in MDS thread
    // Don't try to recover the client list.
    // as all @recovery_flags_ will be set to false.
    //
    // Since 'I' am not aware of if both SC nodes are absent
    // until MDS thread inform me about that.
    // then, 'I' do check everytime doing client recovery.
    // and will cancel the recovery job immediately if get that info.
    if (is_no_log_server() == true) return false;
    if (client == nullptr) continue;
    // DO NOT delete the client if the recovery is failed.
    // That failed recovered client will be deleted from the database
    // by LOG client thread.
    if (client->RecoverMe() == false) continue;
  }
  return true;
}

void LogAgent::NoLogServer() {
  TRACE_ENTER();
  ScopeLock scopeLock(mutex_);
  // When SC is absent, surely, no active SC.
  // Then reset LOG server destination address.
  atomic_data_.lgs_mds_dest = 0;
  atomic_data_.log_server_state = LogServerState::kNoLogServer;
  // Inform SC absence to all its clients.
  // Each client takes responsible to inform to its own log streams.
  for (auto& client : client_list_) {
    if (client == nullptr) continue;
    client->NoLogServer();
  }
}

SaAisErrorT LogAgent::saLogInitialize(SaLogHandleT* logHandle,
                                      const SaLogCallbacksT* callbacks,
                                      SaVersionT* version) {
  LogClient* client;
  lgsv_msg_t i_msg, *o_msg = nullptr;
  SaAisErrorT ais_rc = SA_AIS_OK;
  int rc;
  uint32_t client_id = 0;
  SaVersionT client_ver;

  TRACE_ENTER();

  // Verify parameters (log handle and that version is given)
  if ((logHandle == nullptr) || (version == nullptr)) {
    TRACE("version or handle FAILED");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    return ais_rc;
  }

  // Validate the version
  // Store client version that will be sent to log server. In case minor
  // version has not set, it is set to 1
  // Update the [out] version to highest supported version in log agent
  if (is_log_version_valid(version)) {
    client_ver = *version;
    if (version->minorVersion < LOG_MINOR_VERSION_0) {
      client_ver.minorVersion = LOG_MINOR_VERSION_0;
    }
    version->majorVersion = LOG_MAJOR_VERSION;
    version->minorVersion = LOG_MINOR_VERSION;
  } else {
    TRACE("Version FAILED, required: %c.%u.%u, supported: %c.%u.%u\n",
          version->releaseCode, version->majorVersion, version->minorVersion,
          LOG_RELEASE_CODE, LOG_MAJOR_VERSION, LOG_MINOR_VERSION);
    version->releaseCode = LOG_RELEASE_CODE;
    version->majorVersion = LOG_MAJOR_VERSION;
    version->minorVersion = LOG_MINOR_VERSION;
    ais_rc = SA_AIS_ERR_VERSION;
    return ais_rc;
  }

  // No LOG server. No service is provided.
  if (no_active_log_server() == true) {
    // We have a server but it is temporary unavailable. Client may try again
    TRACE("%s LGS no active", __func__);
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  //>
  // Do initialize. It's ok to initiate in recovery state 1 since we have a
  // server and is not conflicting with auto recovery
  //<

  // Initiate the client in the agent and if first client also start MDS
  if ((rc = lga_startup()) != NCSCC_RC_SUCCESS) {
    TRACE("lga_startup FAILED: %u", rc);
    ais_rc = SA_AIS_ERR_LIBRARY;
    return ais_rc;
  }

  // Increase client counter
  lga_increase_user_counter();

  // Populate the message to be sent to the LGS
  memset(&i_msg, 0, sizeof(lgsv_msg_t));
  i_msg.type = LGSV_LGA_API_MSG;
  i_msg.info.api_info.type = LGSV_INITIALIZE_REQ;
  i_msg.info.api_info.param.init.version = client_ver;

  // Send a message to LGS to obtain a client_id/server ref id which is cluster
  // wide unique.
  rc = lga_mds_msg_sync_send(&i_msg, &o_msg, LGS_WAIT_TIME,
                             MDS_SEND_PRIORITY_HIGH);
  if (rc != NCSCC_RC_SUCCESS) {
    lga_msg_destroy(o_msg);
    lga_decrease_user_counter();
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  // Make sure the LGS return status was SA_AIS_OK
  // If the returned status was SA_AIS_ERR_VERSION, the [out] version is
  // updated to highest supported version in log server. But now the log
  // server has not returned the highest version in the response. Here is
  // temporary updating [out] version to A.02.03.
  // NOTE: This may be fixed to update the highest version when log server
  // return the response with version.
  ais_rc = o_msg->info.api_resp_info.rc;
  if (SA_AIS_OK != ais_rc) {
    TRACE("%s LGS error response %s", __func__, saf_error(ais_rc));
    if (ais_rc == SA_AIS_ERR_VERSION) {
      version->releaseCode = LOG_RELEASE_CODE_1;
      version->majorVersion = LOG_RELEASE_CODE_1;
      version->minorVersion = LOG_RELEASE_CODE_1;
    }
    lga_msg_destroy(o_msg);
    lga_decrease_user_counter();
    return ais_rc;
  }

  // Store the cient ID returned by the LGS to pass into the next routine
  client_id = o_msg->info.api_resp_info.param.init_rsp.client_id;

  // Create @LogClient instance and adding to list of clients
  client = new LogClient(callbacks, client_id, client_ver);
  if (client == nullptr) {
    lga_msg_destroy(o_msg);
    lga_decrease_user_counter();
    ais_rc = SA_AIS_ERR_NO_MEMORY;
    return ais_rc;
  }

  // Pass the handle value to the application
  *logHandle = client->GetHandle();
  lga_msg_destroy(o_msg);
  AddLogClient(client);

  TRACE_LEAVE2("client_id = %d", client_id);
  return ais_rc;
}

SaAisErrorT LogAgent::saLogSelectionObjectGet(
    SaLogHandleT logHandle, SaSelectionObjectT* selectionObject) {
  SaAisErrorT ais_rc = SA_AIS_OK;
  LogClient* client = nullptr;
  bool updated = false;

  TRACE_ENTER();

  // Use scope data to avoid missing data restore when running out of scope
  // such as Restore the reference counter after fetching & updating.
  // or unlock recovery mutex.
  ScopeData::LogClientData client_data{client, &updated,
        RefCounter::Degree::kIncOne, __func__};
  ScopeData data{&client_data};

  if (selectionObject == nullptr) {
    TRACE("selectionObject = nullptr");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    return ais_rc;
  }

  if (true) {
    ScopeLock critical_section(get_delete_obj_sync_mutex_);

    // Get log client from the global list
    client = SearchClientByHandle(logHandle);
    if (client == nullptr) {
      TRACE("No log client with such handle");
      ais_rc = SA_AIS_ERR_BAD_HANDLE;
      return ais_rc;
    }

    if (client->FetchAndIncreaseRefCounter(__func__, &updated) == -1) {
      // @client is being deleted. Don't touch @this client
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }
  }  // end critical section

  // Check CLM membership of node
  if (client->is_stale_client() == true) {
    TRACE("Node is not CLM member or stale client");
    ais_rc = SA_AIS_ERR_UNAVAILABLE;
    return ais_rc;
  }

  // Pass the sel fd to the application
  *selectionObject = client->GetSelectionObject();
  return ais_rc;
}

SaAisErrorT LogAgent::saLogDispatch(SaLogHandleT logHandle,
                                    SaDispatchFlagsT dispatchFlags) {
  LogClient* client = nullptr;
  bool updated = false;
  SaAisErrorT ais_rc;

  TRACE_ENTER();

  // Use scope data to avoid missing data restore when running out of scope
  // such as Restore the reference counter fetching & updating.
  // or unlock recovery mutex.
  ScopeData::LogClientData client_data{client, &updated,
        RefCounter::Degree::kIncOne, __func__};
  ScopeData data{&client_data};

  if (is_dispatch_flag_valid(dispatchFlags) == false) {
    TRACE("Invalid dispatchFlags");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    return ais_rc;
  }

  if (true) {
    ScopeLock critical_section(get_delete_obj_sync_mutex_);

    // Get log client from the global list
    client = SearchClientByHandle(logHandle);
    if (client == nullptr) {
      TRACE("No log client with such handle");
      ais_rc = SA_AIS_ERR_BAD_HANDLE;
      return ais_rc;
    }

    if (client->FetchAndIncreaseRefCounter(__func__, &updated) == -1) {
      // @client is being deleted. DO NOT touch this @client
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }
  }  // end critical section

  // Check CLM membership of node
  if (client->is_stale_client() == true) {
    TRACE("Node is not CLM member or stale client");
    ais_rc = SA_AIS_ERR_UNAVAILABLE;
    return ais_rc;
  }

  if ((ais_rc = client->Dispatch(dispatchFlags)) != SA_AIS_OK) {
    TRACE("LGA_DISPATCH_FAILURE");
  }

  return ais_rc;
}

SaAisErrorT LogAgent::SendFinalizeMsg(uint32_t client_id) {
  uint32_t mds_rc;
  lgsv_msg_t msg, *o_msg = nullptr;
  SaAisErrorT ais_rc = SA_AIS_OK;

  TRACE_ENTER();

  memset(&msg, 0, sizeof(lgsv_msg_t));
  msg.type = LGSV_LGA_API_MSG;
  msg.info.api_info.type = LGSV_FINALIZE_REQ;
  msg.info.api_info.param.finalize.client_id = client_id;

  mds_rc = lga_mds_msg_sync_send(&msg, &o_msg, LGS_WAIT_TIME,
                                 MDS_SEND_PRIORITY_MEDIUM);
  switch (mds_rc) {
    case NCSCC_RC_SUCCESS:
      break;

    case NCSCC_RC_REQ_TIMOUT:
      TRACE("lga_mds_msg_sync_send FAILED: %s", saf_error(ais_rc));
      ais_rc = SA_AIS_ERR_TIMEOUT;
      lga_msg_destroy(o_msg);
      return ais_rc;

    default:
      TRACE("lga_mds_msg_sync_send FAILED: %s", saf_error(ais_rc));
      lga_msg_destroy(o_msg);
      ais_rc = SA_AIS_ERR_NO_RESOURCES;
      return ais_rc;
  }

  if (o_msg != nullptr) {
    ais_rc = o_msg->info.api_resp_info.rc;
    lga_msg_destroy(o_msg);
  } else {
    ais_rc = SA_AIS_ERR_NO_RESOURCES;
  }

  return ais_rc;
}

SaAisErrorT LogAgent::saLogFinalize(SaLogHandleT logHandle) {
  LogClient* client = nullptr;
  bool updated = false;
  bool is_locked = false;
  SaAisErrorT ais_rc = SA_AIS_OK;

  TRACE_ENTER();

  // Use scope data to avoid missing data restore when running out of scope
  // such as Restore the reference counter after fetching & updating.
  // or unlock recovery mutex.
  ScopeData::LogClientData client_data{client, &updated,
        RefCounter::Degree::kDecOne, __func__};
  ScopeData data{&client_data, &is_locked};

  if (true) {
    ScopeLock critical_section(get_delete_obj_sync_mutex_);

    // Get log client from the global list
    client = SearchClientByHandle(logHandle);
    if (client == nullptr) {
      TRACE("No log client with such handle");
      ais_rc = SA_AIS_ERR_BAD_HANDLE;
      return ais_rc;
    }

    if (client->FetchAndDecreaseRefCounter(__func__, &updated) != 0) {
      // DO NOT delete this @client as it is being used by somewhere (>0)
      // Or it is being deleted by other thread (=-1)
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }
  }  // end critical section

  if (client->HaveLogStreamInUse() == true) {
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  // No LOG server. No service is provided.
  if (no_active_log_server() == true) {
    // We have a server but it is temporary unavailable. Client may try again
    TRACE("%s lgs_state = LGS no active", __func__);
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  // Avoid the recovery thread block operation on done-recovery client
  if (client->is_recovery_done() == false) {
    // Synchronize b/w client/recovery thread
    recovery2_lock(&is_locked);
    if (is_lga_recovery_state(RecoveryState::kRecovery2)) {
      // Auto recovery is ongoing. We have to wait for it to finish.
      // The client may try again
      TRACE("%s lga_state = LGA auto recovery ongoing (2)", __func__);
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }

    if (is_lga_recovery_state(RecoveryState::kRecovery1)) {
      // We are in recovery state 1. Client may or may not have been
      // initialized. If initialized a finalize request must be sent
      // to the server else the client is finalized in the agent only
      TRACE("%s lga_state = Recovery state (1)", __func__);
      if (client->is_client_initialized() == false) {
        TRACE("\t Client is not initialized. Remove it from database");
        ScopeLock critical_section(get_delete_obj_sync_mutex_);
        RemoveLogClient(&client);
        return ais_rc;
      }
      TRACE("\t Client is initialized");
    }
  }

  // Populate & send the finalize message and make sure the finalize
  // from the server end returned before deleting the local records.
  ais_rc = SendFinalizeMsg(client->GetClientId());
  if (ais_rc == SA_AIS_OK) {
    TRACE("%s delete_one_client", __func__);
    if (true) {
      ScopeLock critical_section(get_delete_obj_sync_mutex_);
      RemoveLogClient(&client);
    }
  }

  TRACE_LEAVE2("ais_rc = %s", saf_error(ais_rc));
  return ais_rc;
}

SaAisErrorT LogAgent::ValidateOpenParams(
    const char* logStreamName,
    const SaLogFileCreateAttributesT_2* logFileCreateAttributes,
    SaLogStreamOpenFlagsT logStreamOpenFlags,
    SaLogStreamHandleT* logStreamHandle, SaLogHeaderTypeT* header_type) {
  SaAisErrorT ais_rc = SA_AIS_OK;

  TRACE_ENTER();

  osafassert(header_type != nullptr);

  if (nullptr == logStreamHandle) {
    TRACE("SA_AIS_ERR_INVALID_PARAM => nullptr pointer check");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    return ais_rc;
  }

  // Check log stream input parameters.
  if (is_well_known_stream(logStreamName) == true) {
    // The well known log streams
    // SA_AIS_ERR_INVALID_PARAM, bullet 3 in SAI-AIS-LOG-A.02.01
    // Section 3.6.1, Return Values.
    if ((nullptr != logFileCreateAttributes) ||
        (logStreamOpenFlags == SA_LOG_STREAM_CREATE)) {
      TRACE("SA_AIS_ERR_INVALID_PARAM, logStreamOpenFlags");
      return SA_AIS_ERR_INVALID_PARAM;
    }
    if (strcmp(logStreamName, SA_LOG_STREAM_SYSTEM) == 0) {
      *header_type = SA_LOG_GENERIC_HEADER;
    } else {
      *header_type = SA_LOG_NTF_HEADER;
    }
  } else {
    // Application log stream
    // SA_AIS_ERR_INVALID_PARAM, bullet 1 in SAI-AIS-LOG-A.02.01
    // Section 3.6.1, Return Values
    if (logStreamOpenFlags > 1) {
      TRACE("logStreamOpenFlags invalid  when create");
      return SA_AIS_ERR_BAD_FLAGS;
    }

    if ((logStreamOpenFlags == SA_LOG_STREAM_CREATE) &&
        (logFileCreateAttributes == nullptr)) {
      TRACE("logFileCreateAttributes == nullptr, when create");
      return SA_AIS_ERR_INVALID_PARAM;
    }

    // SA_AIS_ERR_INVALID_PARAM, bullet 2 in SAI-AIS-LOG-A.02.01
    // Section 3.6.1, Return Values
    if ((logStreamOpenFlags != SA_LOG_STREAM_CREATE) &&
        (logFileCreateAttributes != nullptr)) {
      TRACE("logFileCreateAttributes defined when create");
      return SA_AIS_ERR_INVALID_PARAM;
    }

    // SA_AIS_ERR_INVALID_PARAM, bullet 5 in SAI-AIS-LOG-A.02.01
    // Section 3.6.1, Return Values
    if (strncmp(logStreamName, "safLgStr=", 9) &&
        strncmp(logStreamName, "safLgStrCfg=", 12)) {
      TRACE("\"safLgStr=\" is missing in logStreamName");
      return SA_AIS_ERR_INVALID_PARAM;
    }

    // SA_AIS_ERR_BAD_FLAGS
    if (logStreamOpenFlags > SA_LOG_STREAM_CREATE) {
      TRACE("logStreamOpenFlags");
      return SA_AIS_ERR_BAD_FLAGS;
    }

    // Check logFileCreateAttributes
    if (logFileCreateAttributes != nullptr) {
      if (logFileCreateAttributes->logFileName == nullptr) {
        TRACE("logFileCreateAttributes");
        return SA_AIS_ERR_INVALID_PARAM;
      }

      if (logFileCreateAttributes->maxLogFileSize == 0) {
        TRACE("maxLogFileSize");
        return SA_AIS_ERR_INVALID_PARAM;
      }

      if ((logFileCreateAttributes->logFileFullAction <
           SA_LOG_FILE_FULL_ACTION_WRAP) ||
          (logFileCreateAttributes->logFileFullAction >
           SA_LOG_FILE_FULL_ACTION_ROTATE)) {
        TRACE("logFileFullAction");
        return SA_AIS_ERR_INVALID_PARAM;
      }

      if (logFileCreateAttributes->haProperty > SA_TRUE) {
        TRACE("haProperty");
        return SA_AIS_ERR_INVALID_PARAM;
      }

      if (logFileCreateAttributes->maxLogRecordSize >
          logFileCreateAttributes->maxLogFileSize) {
        TRACE("maxLogRecordSize is greater than the maxLogFileSize");
        return SA_AIS_ERR_INVALID_PARAM;
      }

      // Verify that the fixedLogRecordSize is in valid range
      if ((logFileCreateAttributes->maxLogRecordSize != 0) &&
          ((logFileCreateAttributes->maxLogRecordSize <
            SA_LOG_MIN_RECORD_SIZE) ||
           (is_logrecord_size_valid(
               logFileCreateAttributes->maxLogRecordSize)))) {
        TRACE("maxLogRecordSize is invalid");
        return SA_AIS_ERR_INVALID_PARAM;
      }

      // Validate maxFilesRotated just in case of
      // SA_LOG_FILE_FULL_ACTION_ROTATE type
      if ((logFileCreateAttributes->logFileFullAction ==
           SA_LOG_FILE_FULL_ACTION_ROTATE) &&
          ((logFileCreateAttributes->maxFilesRotated < 1) ||
           (logFileCreateAttributes->maxFilesRotated > 127))) {
        TRACE("Invalid maxFilesRotated. Valid range = [1-127]");
        return SA_AIS_ERR_INVALID_PARAM;
      }
    }

    *header_type = SA_LOG_GENERIC_HEADER;
  }

  // Check implementation specific string length
  if (nullptr != logFileCreateAttributes) {
    size_t len;
    len = strlen(logFileCreateAttributes->logFileName);
    if ((len == 0) || (len > kMaxLogFileNameLenth)) {
      TRACE("logFileName is too long (max = %d)", kMaxLogFileNameLenth);
      return SA_AIS_ERR_INVALID_PARAM;
    }
    if (logFileCreateAttributes->logFilePathName != nullptr) {
      len = strlen(logFileCreateAttributes->logFilePathName);
      if ((len == 0) || (len > PATH_MAX)) {
        TRACE("logFilePathName");
        return SA_AIS_ERR_INVALID_PARAM;
      }
    }
  }

  return ais_rc;
}

SaAisErrorT LogAgent::saLogStreamOpen_2(
    SaLogHandleT logHandle, const SaNameT* logStreamName,
    const SaLogFileCreateAttributesT_2* logFileCreateAttributes,
    SaLogStreamOpenFlagsT logStreamOpenFlags, SaTimeT timeOut,
    SaLogStreamHandleT* logStreamHandle) {
  LogStreamInfo* stream;
  LogClient* client = nullptr;
  lgsv_msg_t msg, *o_msg = nullptr;
  lgsv_stream_open_req_t* open_param;
  SaAisErrorT ais_rc;
  uint32_t ncs_rc;
  SaTimeT timeout;
  uint32_t log_stream_id;
  SaLogHeaderTypeT log_header_type = SA_LOG_NTF_HEADER;
  bool is_locked = false;
  SaConstStringT streamName;
  bool updated = false;

  TRACE_ENTER();

  // Use scope data to avoid missing data restore when running out of scope
  // such as Restore the reference counter after fetching & updating.
  // or unlock recovery mutex.
  ScopeData::LogClientData client_data{client, &updated,
        RefCounter::Degree::kIncOne, __func__};
  ScopeData data{&client_data, &is_locked};

  if (lga_is_extended_name_valid(logStreamName) == false) {
    TRACE("logStreamName is invalid");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    return ais_rc;
  }

  streamName = osaf_extended_name_borrow(logStreamName);
  ais_rc =
      ValidateOpenParams(streamName, logFileCreateAttributes,
                         logStreamOpenFlags, logStreamHandle, &log_header_type);
  if (ais_rc != SA_AIS_OK) return ais_rc;

  if (true) {
    ScopeLock critical_section(get_delete_obj_sync_mutex_);

    // Get log client from the global list
    client = SearchClientByHandle(logHandle);
    if (client == nullptr) {
      TRACE("No log client with such handle");
      ais_rc = SA_AIS_ERR_BAD_HANDLE;
      return ais_rc;
    }

    if (client->FetchAndIncreaseRefCounter(__func__, &updated) == -1) {
      // @client is being deleted. DO NOT touch this @client
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }
  }  // end critical section

  // Check CLM membership of node
  if (client->is_stale_client() == true) {
    TRACE("Node is not CLM member or stale client");
    ais_rc = SA_AIS_ERR_UNAVAILABLE;
    return ais_rc;
  }

  // No LOG server. No service is provided.
  if (no_active_log_server() == true) {
    // We have a server but it is temporary unavailable. Client may try again
    TRACE("%s LGS no active", __func__);
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  // Avoid the recovery thread block operation on done-recovery client
  if (client->is_recovery_done() == false) {
    // Synchronize b/w client/recovery thread
    recovery2_lock(&is_locked);
    if (is_lga_recovery_state(RecoveryState::kRecovery2)) {
      // Auto recovery is ongoing. We have to wait for it to finish.
      // The client may try again
      TRACE("%s LGA auto recovery ongoing (2)", __func__);
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }

    if (is_lga_recovery_state(RecoveryState::kRecovery1)) {
      // We are in recovery 1 state. Recover client and execute the request
      client->RecoverMe();
    }

    // Remove the client from the database if recovery failed
    if (client->is_recovery_done() == false) {
      ScopeLock critical_section(get_delete_obj_sync_mutex_);
      // Make sure that client is just removed from the list
      // if no other users use it
      if (client->FetchRefCounter() == 1) {
        RemoveLogClient(&client);
        ais_rc = SA_AIS_ERR_BAD_HANDLE;
      } else {
        ais_rc = SA_AIS_ERR_TRY_AGAIN;
      }
      return ais_rc;
    }
  }

  // Populate a sync MDS message to obtain a log stream id
  // and an instance open id.
  memset(&msg, 0, sizeof(lgsv_msg_t));
  msg.type = LGSV_LGA_API_MSG;
  msg.info.api_info.type = LGSV_STREAM_OPEN_REQ;
  open_param = &msg.info.api_info.param.lstr_open_sync;

  // Make it safe for free
  open_param->logFileName = nullptr;
  open_param->logFilePathName = nullptr;

  PopulateOpenParams(
      streamName, client->GetClientId(),
      const_cast<SaLogFileCreateAttributesT_2*>(logFileCreateAttributes),
      logStreamOpenFlags, open_param);
  if (logFileCreateAttributes != nullptr) {
    // Construct the logFileName
    size_t length = strlen(logFileCreateAttributes->logFileName);
    open_param->logFileName = static_cast<char*>(malloc(length + 1));
    if (open_param->logFileName == nullptr) {
      ais_rc = SA_AIS_ERR_NO_MEMORY;
      return ais_rc;
    }
    strncpy(open_param->logFileName, logFileCreateAttributes->logFileName,
            length + 1);

    // Construct the logFilePathName
    // A nullptr pointer refers to impl defined directory
    size_t len = (logFileCreateAttributes->logFilePathName == nullptr)
                     ? (2)
                     : (strlen(logFileCreateAttributes->logFilePathName) + 1);

    open_param->logFilePathName = static_cast<char*>(malloc(len));
    if (open_param->logFilePathName == nullptr) {
      free(open_param->logFileName);
      ais_rc = SA_AIS_ERR_NO_MEMORY;
      return ais_rc;
    }

    if (logFileCreateAttributes->logFilePathName == nullptr) {
      strncpy(open_param->logFilePathName, ".", len);
    } else {
      strncpy(open_param->logFilePathName,
              logFileCreateAttributes->logFilePathName, len);
    }
  }

  // Normalize the timeOut value
  timeout = (timeOut / kNanoSecondToLeapTm);
  if (timeout < kSafMinAcceptTime) {
    TRACE("Timeout");
    free(open_param->logFileName);
    free(open_param->logFilePathName);
    ais_rc = SA_AIS_ERR_TIMEOUT;
    return ais_rc;
  }

  // Send a sync MDS message to obtain a log stream id
  ncs_rc = lga_mds_msg_sync_send(&msg, &o_msg, timeout, MDS_SEND_PRIORITY_HIGH);
  if (ncs_rc != NCSCC_RC_SUCCESS) {
    free(open_param->logFileName);
    free(open_param->logFilePathName);
    lga_msg_destroy(o_msg);
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  ais_rc = o_msg->info.api_resp_info.rc;
  if (SA_AIS_OK != ais_rc) {
    TRACE("Bad return status!!! rc = %d", ais_rc);
    free(open_param->logFileName);
    free(open_param->logFilePathName);
    lga_msg_destroy(o_msg);
    return ais_rc;
  }

  // Retrieve the log stream id and log stream open id params
  // and pass them into the subroutine.
  log_stream_id = o_msg->info.api_resp_info.param.lstr_open_rsp.lstr_id;

  // Create Log stream info and add that to @client
  stream = new LogStreamInfo(streamName, log_stream_id);
  if (stream == nullptr) {
    free(open_param->logFileName);
    free(open_param->logFilePathName);
    lga_msg_destroy(o_msg);
    ais_rc = SA_AIS_ERR_NO_MEMORY;
    return ais_rc;
  }

  // Add the stream to @client
  client->AddLogStreamInfo(stream, logStreamOpenFlags, log_header_type);
  *logStreamHandle = stream->GetHandle();

  lga_msg_destroy(o_msg);
  free(open_param->logFileName);
  free(open_param->logFilePathName);

  return ais_rc;
}

SaAisErrorT LogAgent::saLogStreamOpenAsync_2(
    SaLogHandleT logHandle, const SaNameT* logStreamName,
    const SaLogFileCreateAttributesT_2* logFileCreateAttributes,
    SaLogStreamOpenFlagsT logstreamOpenFlags, SaInvocationT invocation) {
  TRACE_ENTER();
  return SA_AIS_ERR_NOT_SUPPORTED;
}

SaAisErrorT LogAgent::HandleLogRecord(const SaLogRecordT* logRecord,
                                      char* logSvcUsrName,
                                      lgsv_write_log_async_req_t* write_param,
                                      SaTimeT* const logTimeStamp) {
  SaAisErrorT ais_rc = SA_AIS_OK;

  TRACE_ENTER();

  if (nullptr == logRecord) {
    TRACE("SA_AIS_ERR_INVALID_PARAM => nullptr pointer check");
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    return ais_rc;
  }

  if (logRecord->logHdrType == SA_LOG_GENERIC_HEADER) {
    switch (logRecord->logHeader.genericHdr.logSeverity) {
      case SA_LOG_SEV_EMERGENCY:
      case SA_LOG_SEV_ALERT:
      case SA_LOG_SEV_CRITICAL:
      case SA_LOG_SEV_ERROR:
      case SA_LOG_SEV_WARNING:
      case SA_LOG_SEV_NOTICE:
      case SA_LOG_SEV_INFO:
        break;
      default:
        TRACE("Invalid severity: %x",
              logRecord->logHeader.genericHdr.logSeverity);
        ais_rc = SA_AIS_ERR_INVALID_PARAM;
        return ais_rc;
    }
  }

  if (logRecord->logBuffer != nullptr) {
    if ((logRecord->logBuffer->logBuf == nullptr) &&
        (logRecord->logBuffer->logBufSize != 0)) {
      TRACE("logBuf == nullptr && logBufSize != 0");
      ais_rc = SA_AIS_ERR_INVALID_PARAM;
      return ais_rc;
    }
  }

  // Set timeStamp data if not provided by application user
  if (logRecord->logTimeStamp == static_cast<SaTimeT>(SA_TIME_UNKNOWN)) {
    *logTimeStamp = SetLogTime();
    write_param->logTimeStamp = logTimeStamp;
  } else {
    write_param->logTimeStamp = const_cast<SaTimeT*>(&logRecord->logTimeStamp);
  }

  // SA_AIS_ERR_INVALID_PARAM, bullet 2 in SAI-AIS-LOG-A.02.01
  // Section 3.6.3, Return Values
  if (logRecord->logHdrType == SA_LOG_GENERIC_HEADER) {
    if (logRecord->logHeader.genericHdr.logSvcUsrName == nullptr) {
      TRACE("logSvcUsrName == nullptr");
      char* logSvcUsrChars = getenv("SA_AMF_COMPONENT_NAME");
      if (logSvcUsrChars == nullptr) {
        ais_rc = SA_AIS_ERR_INVALID_PARAM;
        return ais_rc;
      }
      if (strlen(logSvcUsrChars) > kOsafMaxDnLength) {
        TRACE("SA_AMF_COMPONENT_NAME is too long");
        ais_rc = SA_AIS_ERR_INVALID_PARAM;
        return ais_rc;
      }
      strncpy(logSvcUsrName, logSvcUsrChars, strlen(logSvcUsrChars) + 1);
      osaf_extended_name_lend(logSvcUsrName, write_param->logSvcUsrName);
    } else {
      if (lga_is_extended_name_valid(
              logRecord->logHeader.genericHdr.logSvcUsrName) == false) {
        TRACE("Invalid logSvcUsrName");
        ais_rc = SA_AIS_ERR_INVALID_PARAM;
        return ais_rc;
      }
      osaf_extended_name_lend(
          osaf_extended_name_borrow(
              logRecord->logHeader.genericHdr.logSvcUsrName),
          write_param->logSvcUsrName);
    }
  }

  if (logRecord->logHdrType == SA_LOG_NTF_HEADER) {
    if (lga_is_extended_name_valid(
            logRecord->logHeader.ntfHdr.notificationObject) == false) {
      TRACE("Invalid notificationObject");
      ais_rc = SA_AIS_ERR_INVALID_PARAM;
      return ais_rc;
    }

    if (lga_is_extended_name_valid(
            logRecord->logHeader.ntfHdr.notifyingObject) == false) {
      TRACE("Invalid notifyingObject");
      ais_rc = SA_AIS_ERR_INVALID_PARAM;
      return ais_rc;
    }
  }

  return ais_rc;
}

SaAisErrorT LogAgent::saLogWriteLog(SaLogStreamHandleT logStreamHandle,
                                    SaTimeT timeOut,
                                    const SaLogRecordT* logRecord) {
  TRACE_ENTER();
  return SA_AIS_ERR_NOT_SUPPORTED;
}

SaAisErrorT LogAgent::saLogWriteLogAsync(SaLogStreamHandleT logStreamHandle,
                                         SaInvocationT invocation,
                                         SaLogAckFlagsT ackFlags,
                                         const SaLogRecordT* logRecord) {
  LogStreamInfo* stream = nullptr;
  LogClient* client = nullptr;
  lgsv_msg_t msg;
  SaAisErrorT ais_rc = SA_AIS_OK;
  lgsv_write_log_async_req_t* write_param;
  char logSvcUsrName[kOsafMaxDnLength + 1] = {0};
  bool is_locked = false;
  SaNameT tmpSvcUsrName;
  bool cUpdated = false, sUpdated = false;

  TRACE_ENTER();

  // Use scope data to avoid missing data restore when running out of scope
  // such as Restore the reference counter after fetching & updating.
  // or unlock recovery mutex.
  ScopeData::LogClientData client_data{client, &cUpdated,
        RefCounter::Degree::kIncOne, __func__};
  ScopeData::LogStreamInfoData stream_data{stream, &sUpdated,
        RefCounter::Degree::kIncOne, __func__};
  ScopeData data{&client_data, &stream_data, &is_locked};

  if (true) {
    ScopeLock critical_section(get_delete_obj_sync_mutex_);

    // Retrieve the @LogStreamInfo based on @logStreamHandle
    stream = SearchLogStreamInfoByHandle(logStreamHandle);
    if (stream == nullptr) {
      TRACE("%s: ncshm_take_hdl logStreamHandle FAILED!", __func__);
      ais_rc = SA_AIS_ERR_BAD_HANDLE;
      return ais_rc;
    }

    // Retrieve @LogClient obj based on @client_handle
    client = SearchClientByHandle(stream->GetMyClientHandle());
    if (client == nullptr) {
      TRACE("%s: No such client with this logHandle!", __func__);
      ais_rc = SA_AIS_ERR_LIBRARY;
      return ais_rc;
    }

    if (client->FetchAndIncreaseRefCounter(__func__, &cUpdated) == -1) {
      // @client is being deleted. DO NOT touch this @client
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }

    if (stream->FetchAndIncreaseRefCounter(__func__, &sUpdated) == -1) {
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }
  }  // end critical section

  memset(&(msg), 0, sizeof(lgsv_msg_t));
  write_param = &msg.info.api_info.param.write_log_async;
  write_param->logSvcUsrName = &tmpSvcUsrName;

  if ((ackFlags != 0) && (ackFlags != SA_LOG_RECORD_WRITE_ACK)) {
    TRACE("SA_AIS_ERR_BAD_FLAGS=> ackFlags");
    ais_rc = SA_AIS_ERR_BAD_FLAGS;
    return ais_rc;
  }

  // Validate the log record and if generic header add
  // logSvcUsrName from environment variable SA_AMF_COMPONENT_NAME
  SaTimeT logTimeStamp;
  ais_rc =
      HandleLogRecord(logRecord, logSvcUsrName, write_param, &logTimeStamp);
  if (ais_rc != SA_AIS_OK) {
    TRACE("%s: Validate Log record Fail", __func__);
    return ais_rc;
  }

  if (logRecord->logBuffer != nullptr &&
      logRecord->logBuffer->logBuf != nullptr) {
    SaSizeT size = logRecord->logBuffer->logBufSize;
    if (is_well_known_stream(stream->GetStreamName().c_str()) == true) {
      bool sizeOver =
          size >
          strlen(reinterpret_cast<char*>(logRecord->logBuffer->logBuf)) + 1;
      // Prevent log client accidently assign too big number to logBufSize.
      if (sizeOver == true) {
        TRACE("logBufSize > strlen(logBuf) + 1");
        ais_rc = SA_AIS_ERR_INVALID_PARAM;
        return ais_rc;
      }
    }

    // Prevent sending too big data to server side
    if (is_logrecord_size_valid(size) == true) {
      TRACE("logBuf data is too big (max: %d)", SA_LOG_MAX_RECORD_SIZE);
      ais_rc = SA_AIS_ERR_INVALID_PARAM;
      return ais_rc;
    }
  }

  // SA_AIS_ERR_INVALID_PARAM, bullet 1 in SAI-AIS-LOG-A.02.01
  // Section 3.6.3, Return Values
  if (stream->GetHeaderType() != logRecord->logHdrType) {
    TRACE("%s: log_header_type != logRecord->logHdrType", __func__);
    ais_rc = SA_AIS_ERR_INVALID_PARAM;
    return ais_rc;
  }

  // Check CLM membership of node
  if (client->is_stale_client() == true) {
    TRACE("Node is not CLM member or stale client");
    ais_rc = SA_AIS_ERR_UNAVAILABLE;
    return ais_rc;
  }

  if ((client->GetCallback()->saLogWriteLogCallback == nullptr) &&
      (ackFlags == SA_LOG_RECORD_WRITE_ACK)) {
    TRACE("%s: Write Callback not registered", __func__);
    ais_rc = SA_AIS_ERR_INIT;
    return ais_rc;
  }

  // No LOG server, no service provided.
  if (no_active_log_server() == true) {
    // We have a server but it is temporarily unavailable. Client may try again
    TRACE("%s: LGS no active", __func__);
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  // Avoid the recovery thread block operation on done-recovery client
  if (client->is_recovery_done() == false) {
    // Synchronize b/w client/recovery thread
    recovery2_lock(&is_locked);
    if (is_lga_recovery_state(RecoveryState::kRecovery2)) {
      // Auto recovery is ongoing. We have to wait for it to finish.
      // The client may try again
      TRACE("\t RecoveryState::kRecovery2");
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }

    if (is_lga_recovery_state(RecoveryState::kRecovery1)) {
      TRACE("\t RecoveryState::kRecovery1");
      client->RecoverMe();
    }

    // Remove the client from the database if recovery failed
    if (client->is_recovery_done() == false) {
      ScopeLock critical_section(get_delete_obj_sync_mutex_);
      // Make sure that client is just removed from the list
      // if no other users use it
      if (client->FetchRefCounter() == 1) {
        RemoveLogClient(&client);
        // To avoid ScopeData restore ref counter for already deleted stream
        stream = nullptr;
        ais_rc = SA_AIS_ERR_BAD_HANDLE;
      } else {
        ais_rc = SA_AIS_ERR_TRY_AGAIN;
      }
      return ais_rc;
    }
  }

  // Populate the mds message to send across to the LGS
  // whence it will possibly get published
  msg.type = LGSV_LGA_API_MSG;
  msg.info.api_info.type = LGSV_WRITE_LOG_ASYNC_REQ;

  write_param->invocation = invocation;
  write_param->ack_flags = ackFlags;
  write_param->client_id = client->GetClientId();
  write_param->lstr_id = stream->GetStreamId();
  write_param->logRecord = const_cast<SaLogRecordT*>(logRecord);
  // Send the message out to the LGS
  if (NCSCC_RC_SUCCESS !=
      lga_mds_msg_async_send(&msg, MDS_SEND_PRIORITY_MEDIUM)) {
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
  }

  return ais_rc;
}

SaAisErrorT LogAgent::saLogStreamClose(SaLogStreamHandleT logStreamHandle) {
  LogStreamInfo* stream = nullptr;
  LogClient* client = nullptr;
  lgsv_msg_t msg, *o_msg = nullptr;
  SaAisErrorT ais_rc = SA_AIS_OK;
  uint32_t mds_rc;
  bool is_locked = false;
  bool sUpdated = false, cUpdated = false;

  TRACE_ENTER();

  // Use scope data to avoid missing data restore when running out of scope
  // such as Restore the reference counter after fetching & updating.
  // or unlock recovery mutex.
  ScopeData::LogClientData client_data{client, &cUpdated,
        RefCounter::Degree::kIncOne, __func__};
  ScopeData::LogStreamInfoData stream_data{stream, &sUpdated,
        RefCounter::Degree::kDecOne, __func__};
  ScopeData data{&client_data, &stream_data, &is_locked};

  if (true) {
    ScopeLock critical_section(get_delete_obj_sync_mutex_);

    stream = SearchLogStreamInfoByHandle(logStreamHandle);
    if (stream == nullptr) {
      TRACE("ncshm_take_hdl logStreamHandle ");
      ais_rc = SA_AIS_ERR_BAD_HANDLE;
      return ais_rc;
    }

    if (stream->FetchAndDecreaseRefCounter(__func__, &sUpdated) != 0) {
      // @stream is being used somewhere (>0), DO NOT delete this @stream.
      // or @stream is being deleted on other thread (=-1)
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }

    // Retrieve @LogClient obj based on @client_handle
    client = SearchClientByHandle(stream->GetMyClientHandle());
    if (client == nullptr) {
      TRACE("%s: No such client with this logHandle!", __func__);
      ais_rc = SA_AIS_ERR_LIBRARY;
      return ais_rc;
    }

    if (client->FetchAndIncreaseRefCounter(__func__, &cUpdated) == -1) {
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }
  }  // end critical section

  if (no_active_log_server_but_not_headless() == true) {
    // We have a server but it is temporarily unavailable. Client may try again
    TRACE("%s LGS no active", __func__);
    ais_rc = SA_AIS_ERR_TRY_AGAIN;
    return ais_rc;
  }

  // Check CLM membership of node
  if (client->is_stale_client() == true) {
    TRACE("Node is not CLM member or stale client");
    ais_rc = SA_AIS_ERR_UNAVAILABLE;
    return ais_rc;
  }

  if (is_lga_recovery_state(RecoveryState::kNoLogServer)) {
    // No server is available. Remove the stream from client database.
    // Server side will manage to release resources of this stream when up.
    TRACE("%s No server", __func__);
    ScopeLock critical_section(get_delete_obj_sync_mutex_);
    client->RemoveLogStreamInfo(&stream);
    ais_rc = SA_AIS_OK;
    return ais_rc;
  }

  // Avoid the recovery thread block operation on done-recovery client
  if (client->is_recovery_done() == false) {
    // Synchronize b/w client/recovery thread
    recovery2_lock(&is_locked);
    if (is_lga_recovery_state(RecoveryState::kRecovery2)) {
      // Auto recovery is ongoing. We have to wait for it to finish.
      // The client may try again
      TRACE("%s LGA auto recovery ongoing (2)", __func__);
      ais_rc = SA_AIS_ERR_TRY_AGAIN;
      return ais_rc;
    }

    if (is_lga_recovery_state(RecoveryState::kRecovery1)) {
      // Recover client and execute the request
      client->RecoverMe();
    }

    // Remove the client from the database if recovery failed
    if (client->is_recovery_done() == false) {
      ScopeLock critical_section(get_delete_obj_sync_mutex_);
      // Make sure that client is just removed from the list
      // if no other users use it
      if (client->FetchRefCounter() == 1) {
        RemoveLogClient(&client);
        // To avoid ScopeData restore ref counter for already deleted stream
        stream = nullptr;
        ais_rc = SA_AIS_ERR_BAD_HANDLE;
      } else {
        ais_rc = SA_AIS_ERR_TRY_AGAIN;
      }
      return ais_rc;
    }
  }

  // Populate a MDS message to send to the LGS for a channel close operation.
  memset(&msg, 0, sizeof(lgsv_msg_t));
  msg.type = LGSV_LGA_API_MSG;
  msg.info.api_info.type = LGSV_STREAM_CLOSE_REQ;
  msg.info.api_info.param.lstr_close.client_id = client->GetClientId();
  msg.info.api_info.param.lstr_close.lstr_id = stream->GetStreamId();
  mds_rc = lga_mds_msg_sync_send(&msg, &o_msg, LGS_WAIT_TIME,
                                 MDS_SEND_PRIORITY_MEDIUM);
  switch (mds_rc) {
    case NCSCC_RC_SUCCESS:
      break;

    case NCSCC_RC_REQ_TIMOUT:
      TRACE("lga_mds_msg_sync_send FAILED: %s", saf_error(ais_rc));
      ais_rc = SA_AIS_ERR_TIMEOUT;
      lga_msg_destroy(o_msg);
      return ais_rc;

    default:
      TRACE("lga_mds_msg_sync_send FAILED: %s", saf_error(ais_rc));
      ais_rc = SA_AIS_ERR_NO_RESOURCES;
      lga_msg_destroy(o_msg);
      return ais_rc;
  }

  if (o_msg != nullptr) {
    ais_rc = o_msg->info.api_resp_info.rc;
    lga_msg_destroy(o_msg);
  } else {
    ais_rc = SA_AIS_ERR_NO_RESOURCES;
  }

  if (ais_rc == SA_AIS_OK) {
    ScopeLock critical_section(get_delete_obj_sync_mutex_);
    client->RemoveLogStreamInfo(&stream);
  }

  return ais_rc;
}

SaAisErrorT LogAgent::saLogLimitGet(SaLogHandleT logHandle,
                                    SaLogLimitIdT limitId,
                                    SaLimitValueT* limitValue) {
  TRACE_ENTER();
  return SA_AIS_ERR_NOT_SUPPORTED;
}
