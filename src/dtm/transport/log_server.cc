/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2016 The OpenSAF Foundation
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

#include "dtm/transport/log_server.h"
#include <signal.h>
#include <syslog.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include "base/osaf_poll.h"
#include "base/string_parse.h"
#include "base/time.h"
#include "dtm/common/osaflog_protocol.h"
#include "osaf/configmake.h"

const Osaflog::ClientAddressConstantPrefix LogServer::address_header_{};

LogServer::LogServer(int term_fd)
    : term_fd_{term_fd},
      max_backups_{9},
      max_file_size_{5 * 1024 * 1024},
      log_socket_{Osaflog::kServerSocketPath, base::UnixSocket::kNonblocking,
                  0777},
      log_streams_{},
      current_stream_{new LogStream{kMdsLogStreamName, 1, 5 * 1024 * 1024}},
      no_of_log_streams_{1} {
  log_streams_[kMdsLogStreamName] = current_stream_;
  }

LogServer::~LogServer() {
  for (const auto& s : log_streams_) delete s.second;
  log_streams_.clear();
}

void LogServer::Run() {
  struct pollfd pfd[2] = {{term_fd_, POLLIN, 0}, {log_socket_.fd(), POLLIN, 0}};
  do {
    for (int i = 0; i < 256; ++i) {
      char* buffer = current_stream_->current_buffer_position();
      struct sockaddr_un src_addr;
      socklen_t addrlen = sizeof(src_addr);
      ssize_t result = log_socket_.RecvFrom(buffer, LogWriter::kMaxMessageSize,
                                            &src_addr, &addrlen);
      if (result < 0) break;
      if (result == 0 || buffer[0] != '?') {
        while (result != 0 && buffer[result - 1] == '\n') --result;
        if (static_cast<size_t>(result) != LogWriter::kMaxMessageSize) {
          buffer[result++] = '\n';
        } else {
          buffer[result - 1] = '\n';
        }
        size_t msg_id_size;
        const char* msg_id = Osaflog::GetField(buffer, result, 5, &msg_id_size);
        if (msg_id == nullptr) continue;
        LogStream* stream = GetStream(msg_id, msg_id_size);
        if (stream == nullptr) continue;
        if (stream != current_stream_) {
          memcpy(stream->current_buffer_position(), buffer, result);
          current_stream_ = stream;
        }
        current_stream_->Write(result);
      } else {
        ExecuteCommand(buffer, result, src_addr, addrlen);
      }
    }

    CloseIdleStreams();

    struct timespec current = base::ReadMonotonicClock();
    struct timespec last_flush = current;
    bool empty = true;
    for (const auto& s : log_streams_) {
      LogStream* stream = s.second;
      struct timespec flush = stream->last_flush();
      if ((current - flush) >= base::kFifteenSeconds) {
        stream->Flush();
      } else {
        if (flush < last_flush) last_flush = flush;
      }
      if (!stream->empty()) empty = false;
    }
    struct timespec timeout = (last_flush + base::kFifteenSeconds) - current;
    struct timespec* poll_timeout = &timeout;
    if (empty && log_streams_.size() > 1) {
      uint64_t max_idle = max_idle_time_.tv_sec;
      poll_timeout = (max_idle) ? &max_idle_time_ : nullptr;
    }
    pfd[1].fd = log_socket_.fd();
    osaf_ppoll(pfd, 2, poll_timeout, nullptr);
  } while ((pfd[0].revents & POLLIN) == 0);
}

void LogServer::CloseIdleStreams() {
  if (max_idle_time_.tv_sec == 0 || log_streams_.size() == 1) return;

  struct timespec current = base::ReadMonotonicClock();
  auto it = log_streams_.begin();
  while (it != log_streams_.end()) {
    LogStream* stream = it->second;
    struct timespec last_write = stream->last_write();
    std::string name = stream->name();
    if ((current - last_write) >= max_idle_time_
        && name != kMdsLogStreamName) {
      syslog(LOG_NOTICE, "Deleted the idle stream: %s", name.c_str());
      if (current_stream_ == stream) {
        current_stream_ = log_streams_.begin()->second;
      }
      it = log_streams_.erase(it);
      delete stream;
      no_of_log_streams_--;
    } else {
      it++;
    }
  }
}

LogServer::LogStream* LogServer::GetStream(const char* msg_id,
                                           size_t msg_id_size) {
  if (msg_id_size == current_stream_->log_name_size() &&
      memcmp(msg_id, current_stream_->log_name_data(), msg_id_size) == 0) {
    return current_stream_;
  }
  std::string log_name{msg_id, msg_id_size};
  auto iter = log_streams_.find(log_name);
  if (iter != log_streams_.end()) return iter->second;
  if (no_of_log_streams_ >= kMaxNoOfStreams) return nullptr;
  if (!ValidateLogName(msg_id, msg_id_size)) return nullptr;
  LogStream* stream = new LogStream{log_name, max_backups_, max_file_size_};
  auto result = log_streams_.insert(
      std::map<std::string, LogStream*>::value_type{log_name, stream});
  if (!result.second) osaf_abort(msg_id_size);
  ++no_of_log_streams_;
  return stream;
}

bool LogServer::ReadConfig(const char *transport_config_file) {
  FILE *transport_conf_file;
  char line[256];
  int i, n, comment_line, tag_len = 0;

  /* Open transportd.conf config file. */
  transport_conf_file = fopen(transport_config_file, "r");
  if (transport_conf_file == nullptr) {
    syslog(LOG_ERR, "Not able to read transportd.conf: %s", strerror(errno));
    return false;
  }


  /* Read file. */
  while (fgets(line, 256, transport_conf_file)) {
    /* If blank line, skip it - and set tag back to 0. */
    if (strcmp(line, "\n") == 0) {
      continue;
    }

    /* If a comment line, skip it. */
    n = strlen(line);
    comment_line = 0;
    for (i = 0; i < n; i++) {
      if ((line[i] == ' ') || (line[i] == '\t')) {
        continue;
      } else if (line[i] == '#') {
        comment_line = 1;
        break;
      } else {
        break;
      }
    }
    if (comment_line) continue;
    line[n - 1] = 0;

    if (strncmp(line, "TRANSPORT_MAX_FILE_SIZE=",
                  strlen("TRANSPORT_MAX_FILE_SIZE=")) == 0) {
      tag_len = strlen("TRANSPORT_MAX_FILE_SIZE=");
      bool success;
      uint64_t max_file_size = base::StrToUint64(&line[tag_len], &success);
      if (success && max_file_size <= SIZE_MAX) {
        max_file_size_ = max_file_size;
      } else {
        syslog(LOG_ERR, "TRANSPORT_MAX_FILE_SIZE has illegal value '%s'",
               &line[tag_len]);
      }
    }

    if (strncmp(line, "TRANSPORT_MAX_BACKUPS=",
                  strlen("TRANSPORT_MAX_BACKUPS=")) == 0) {
      tag_len = strlen("TRANSPORT_MAX_BACKUPS=");
      bool success;
      uint64_t max_backups = base::StrToUint64(&line[tag_len], &success);
      if (success && max_backups <= SIZE_MAX) {
         max_backups_ = max_backups;
      } else {
        syslog(LOG_ERR, "TRANSPORT_MAX_BACKUPS "
               "has illegal value '%s'", &line[tag_len]);
      }
    }

    if (strncmp(line, "TRANSPORT_MAX_IDLE_TIME=",
                strlen("TRANSPORT_MAX_IDLE_TIME=")) == 0) {
      tag_len = strlen("TRANSPORT_MAX_IDLE_TIME=");
      bool success;
      uint64_t max_idle_time = base::StrToUint64(&line[tag_len], &success);
      if (success && max_idle_time <= Osaflog::kOneDayInMinute) {
        max_idle_time_.tv_sec = max_idle_time;
      } else {
        syslog(LOG_ERR, "TRANSPORT_MAX_IDLE_TIME "
               "has illegal value '%s'", &line[tag_len]);
      }
    }
  }

  /* Close file. */
  fclose(transport_conf_file);

  return true;
}

bool LogServer::ValidateLogName(const char* msg_id, size_t msg_id_size) {
  if (msg_id_size < 1 || msg_id_size > LogStream::kMaxLogNameSize) return false;
  if (msg_id[0] == '.' || msg_id[0] == '-') return false;
  size_t no_of_dots = 0;
  for (size_t i = 0; i != msg_id_size; ++i) {
    char c = msg_id[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || (c == '.' || c == '_' || c == '-')))
      return false;
    if (c == '.') ++no_of_dots;
  }
  return no_of_dots < 2;
}

void LogServer::ExecuteCommand(const char* request, size_t size,
                               const struct sockaddr_un& addr,
                               socklen_t addrlen) {
  if (ValidateAddress(addr, addrlen)) {
    size_t command_size;
    size_t argument_size;
    const char* command = Osaflog::GetField(request, size, 0, &command_size);
    const char* argument = Osaflog::GetField(request, size, 1, &argument_size);
    std::string reply = ExecuteCommand(command != nullptr
                                       ? std::string{command, command_size}
                                       : std::string{},
                                       argument != nullptr
                                       ? std::string{argument, argument_size}
                                       : std::string{});
    log_socket_.SendTo(reply.data(), reply.size(), &addr, addrlen);
  }
}

bool LogServer::ValidateAddress(const struct sockaddr_un& addr,
                                socklen_t addrlen) {
  if (addrlen == sizeof(Osaflog::ClientAddress)) {
    return memcmp(&addr, &address_header_, sizeof(address_header_)) == 0;
  } else {
    return false;
  }
}

std::string LogServer::MaxFileSizeCmd(const std::string& cmd,
                                      const std::string& arg) {
  bool success = false;
  uint64_t max_file_size = base::StrToUint64(arg.c_str(), &success);
  if (success && max_file_size <= SIZE_MAX) max_file_size_ = max_file_size;
  return std::string{"!max-file-size " + std::to_string(max_file_size_)};
}

std::string LogServer::MaxBackupCmd(const std::string& cmd,
                                    const std::string& arg) {
  bool success = false;
  uint64_t max_backups = base::StrToUint64(arg.c_str(), &success);
  if (success && max_backups <= SIZE_MAX) max_backups_ = max_backups;
  return std::string{"!max-backups " + std::to_string(max_backups_)};
}

std::string LogServer::DeleteCmd(const std::string& cmd,
                                 const std::string& arg) {
  if (!ValidateLogName(arg.c_str(), arg.size()) ||
      arg == kMdsLogStreamName) {
    return std::string{"Invalid stream name"};
  }
  auto iter = log_streams_.find(arg);
  if (iter == log_streams_.end()) {
    return std::string{"Stream not found"};
  }
  LogStream* stream = iter->second;
  log_streams_.erase(iter);
  if (current_stream_ == stream) {
    current_stream_ = log_streams_.begin()->second;
  }
  delete stream;
  --no_of_log_streams_;
  return std::string{"!delete " + arg};
}

std::string LogServer::FlushCmd(const std::string& cmd,
                                const std::string& arg) {
  for (const auto& s : log_streams_) {
    LogStream* stream = s.second;
    stream->Flush();
  }
  return std::string{"!flush"};
}

std::string LogServer::MaxIdleCmd(const std::string& cmd,
                                  const std::string& arg) {
  bool success = false;
  uint64_t max_idle_time = base::StrToUint64(arg.c_str(), &success);
  if (success && max_idle_time <= Osaflog::kOneDayInMinute) {
    max_idle_time_.tv_sec = max_idle_time*60;
  }
  return std::string{"!max-idle-time " + std::to_string(max_idle_time)};
}

std::string LogServer::RotateCmd(const std::string& cmd,
                                 const std::string& arg) {
  for (const auto& s : log_streams_) {
    LogStream* stream = s.second;
    if (stream->name() == arg) {
      stream->Rotate();
      break;
    }
  }
  return std::string{"!rotate " + arg};
}

std::string LogServer::ExecuteCommand(const std::string& command,
                                      const std::string& argument) {
  using CmdPtr = std::string (LogServer::*)(const std::string&,
                                            const std::string&);
  std::map<std::string, CmdPtr> cmd_dispatcher = {
    {"?max-file-size", &LogServer::MaxFileSizeCmd},
    {"?max-backups", &LogServer::MaxBackupCmd},
    {"?delete", &LogServer::DeleteCmd},
    {"?flush", &LogServer::FlushCmd},
    {"?max-idle-time", &LogServer::MaxIdleCmd},
    {"?rotate", &LogServer::RotateCmd}
  };

  if (cmd_dispatcher.find(command) != cmd_dispatcher.end()) {
    return (this->*cmd_dispatcher[command])(command, argument);
  }

  return std::string{"!not_supported"};
}

LogServer::LogStream::LogStream(const std::string& log_name,
                                size_t max_backups, size_t max_file_size)
    : log_name_{log_name}, last_flush_{}, log_writer_{log_name, max_backups,
                                                             max_file_size} {
  if (log_name.size() > kMaxLogNameSize) osaf_abort(log_name.size());
  last_write_ = base::ReadMonotonicClock();
}

void LogServer::LogStream::Write(size_t size) {
  if (log_writer_.empty()) last_flush_ = base::ReadMonotonicClock();
  log_writer_.Write(size);
  last_write_ = base::ReadMonotonicClock();
}

void LogServer::LogStream::Flush() {
  log_writer_.Flush();
  last_flush_ = base::ReadMonotonicClock();
}
