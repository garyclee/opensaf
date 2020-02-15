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

#ifndef BASE_LOGTRACE_CLIENT_H_
#define BASE_LOGTRACE_CLIENT_H_

#include <map>
#include "base/log_message.h"
#include "base/buffer.h"
#include "base/conf.h"
#include "base/logtrace_buffer.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/unix_client_socket.h"

// A class implements trace/log client that communicates to the log server
// running in osaftransportd.
class LogTraceBuffer;

class LogTraceClient {
 public:
  enum WriteMode {
    kRemoteBlocking = base::UnixSocket::Mode::kBlocking,
    kRemoteNonblocking = base::UnixSocket::Mode::kNonblocking,
    kLocalBuffer
  };
  LogTraceClient(const char *msg_id, WriteMode mode);
  ~LogTraceClient();

  static const char* Log(LogTraceClient* tracelog,
      base::LogMessage::Severity severity, const char *fmt, va_list ap);
  const char* Log(base::LogMessage::Severity severity, const char *fmt,
                  va_list ap);
  const char* CreateLogEntry(base::LogMessage::Severity severity,
      timespec time_spec, const char *fmt, va_list ap);
  void AddExternalBuffer(int64_t tid, LogTraceBuffer* buffer);
  void RemoveExternalBuffer(int64_t tid);
  void RequestFlushExternalBuffer();

  const char* app_name() const { return app_name_.data(); }
  const char* proc_id() const { return proc_id_.data(); }

  bool FlushExternalBuffer();

 private:
  bool Init(const char *msg_id, WriteMode mode);

  const char* LogInternal(base::LogMessage::Severity severity,
      timespec time_spec, const char *fmt, va_list ap);
  const char* CreateLogEntryInternal(base::LogMessage::Severity severity,
      timespec time_spec, const char *fmt, va_list ap);
  static constexpr const uint32_t kMaxSequenceId = uint32_t{0x7fffffff};
  base::LogMessage::HostName fqdn_{""};
  base::LogMessage::AppName app_name_{""};
  base::LogMessage::ProcId proc_id_{""};
  base::LogMessage::MsgId msg_id_{""};
  uint32_t sequence_id_;
  base::UnixClientSocket* log_socket_;
  base::Buffer<512> buffer_;
  base::Mutex* log_mutex_;

  std::map<int64_t, LogTraceBuffer*> ext_buffer_map_;
  base::Mutex* ext_buffer_mutex_;

  DELETE_COPY_AND_MOVE_OPERATORS(LogTraceClient);
};

#endif  // BASE_LOGTRACE_CLIENT_H_
