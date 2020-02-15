/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2018 - All Rights Reserved.
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

#include "base/logtrace_client.h"
#include <limits.h>
#include <unistd.h>
#include <utility>
#include <string>
#include "base/getenv.h"
#include "base/ncsgl_defs.h"
#include "base/osaf_utility.h"
#include "dtm/common/osaflog_protocol.h"


LogTraceClient::LogTraceClient(const char *msg_id, WriteMode mode)
    : sequence_id_{1}, buffer_{} {
  log_mutex_ = nullptr;
  ext_buffer_mutex_ = nullptr;
  log_socket_ = nullptr;
  Init(msg_id, mode);
}

LogTraceClient::~LogTraceClient() {
  if (log_mutex_) delete log_mutex_;
  if (ext_buffer_mutex_) delete ext_buffer_mutex_;
  if (log_socket_) delete log_socket_;
}

bool LogTraceClient::Init(const char *msg_id, WriteMode mode) {
  char app_name[NAME_MAX];
  char pid_path[PATH_MAX];
  uint32_t process_id;
  char *token, *saveptr;
  char *pid_name = nullptr;

  memset(app_name, 0, NAME_MAX);
  memset(pid_path, 0, PATH_MAX);
  process_id = getpid();

  snprintf(pid_path, sizeof(pid_path), "/proc/%" PRIu32 "/cmdline",
      process_id);
  FILE *f = fopen(pid_path, "r");
  if (f != nullptr) {
    size_t size;
    size = fread(pid_path, sizeof(char), PATH_MAX, f);
    if (size > 0) {
      if ('\n' == pid_path[size - 1]) pid_path[size - 1] = '\0';
    }
    fclose(f);
  } else {
    pid_path[0] = '\0';
  }
  token = strtok_r(pid_path, "/", &saveptr);
  while (token != nullptr) {
    pid_name = token;
    token = strtok_r(nullptr, "/", &saveptr);
  }
  if (snprintf(app_name, sizeof(app_name), "%s", pid_name) < 0) {
    app_name[0] = '\0';
  }
  base::Conf::InitFullyQualifiedDomainName();
  const std::string &fqdn = base::Conf::FullyQualifiedDomainName();

  fqdn_ = base::LogMessage::HostName(fqdn);
  app_name_ = base::LogMessage::AppName(app_name);
  proc_id_ = base::LogMessage::ProcId{std::to_string(process_id)};
  msg_id_ = base::LogMessage::MsgId{msg_id};

  if (mode == kRemoteBlocking || mode == kRemoteNonblocking) {
    log_socket_ = new base::UnixClientSocket{Osaflog::kServerSocketPath,
      static_cast<base::UnixSocket::Mode>(mode)};
  }
  log_mutex_ = new base::Mutex{};
  ext_buffer_mutex_ = new base::Mutex{};
  return true;
}

const char* LogTraceClient::Log(LogTraceClient* tracelog,
    base::LogMessage::Severity severity, const char *fmt, va_list ap) {
  if (tracelog != nullptr) return tracelog->Log(severity, fmt, ap);
  return nullptr;
}

const char* LogTraceClient::Log(base::LogMessage::Severity severity,
    const char *fmt, va_list ap) {
  if (log_socket_ != nullptr && log_mutex_ != nullptr) {
    return LogInternal(severity, base::ReadRealtimeClock(), fmt, ap);
  }
  return nullptr;
}

const char* LogTraceClient::LogInternal(base::LogMessage::Severity severity,
    timespec time_spec, const char *fmt, va_list ap) {
  base::Lock lock(*log_mutex_);
  CreateLogEntryInternal(severity, time_spec, fmt, ap);
  log_socket_->Send(buffer_.data(), buffer_.size());
  return buffer_.data();
}

const char* LogTraceClient::CreateLogEntry(base::LogMessage::Severity severity,
    timespec time_spec, const char *fmt, va_list ap) {
  base::Lock lock(*log_mutex_);
  return CreateLogEntryInternal(severity, time_spec, fmt, ap);
}

const char* LogTraceClient::CreateLogEntryInternal(
    base::LogMessage::Severity severity, timespec time_spec,
    const char *fmt, va_list ap) {
  uint32_t id = sequence_id_;
  sequence_id_ = id < kMaxSequenceId ? id + 1 : 1;
  buffer_.clear();
  base::LogMessage::Write(
      base::LogMessage::Facility::kLocal1, severity, time_spec,
      fqdn_, app_name_, proc_id_, msg_id_,
      {{base::LogMessage::SdName{"meta"},
        {base::LogMessage::Parameter{base::LogMessage::SdName{"sequenceId"},
                                     std::to_string(id)}}}},
      fmt, ap, &buffer_);
  return buffer_.data();
}

void LogTraceClient::AddExternalBuffer(int64_t tid, LogTraceBuffer* buffer) {
  base::Lock lock(*ext_buffer_mutex_);
  ext_buffer_map_.insert(std::pair<int64_t, LogTraceBuffer*>(tid, buffer));
}

void LogTraceClient::RemoveExternalBuffer(int64_t tid) {
  base::Lock lock(*ext_buffer_mutex_);
  ext_buffer_map_.erase(tid);
}

void LogTraceClient::RequestFlushExternalBuffer() {
  base::Lock lock(*ext_buffer_mutex_);
  for (auto &buff : ext_buffer_map_) {
    buff.second->SetFlush(true);
  }
}

bool LogTraceClient::FlushExternalBuffer() {
  base::Lock lock(*ext_buffer_mutex_);
  for (auto &buff : ext_buffer_map_) {
    buff.second->FlushBuffer();
  }
  return true;
}

