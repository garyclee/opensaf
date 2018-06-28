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

#ifndef DTM_TRANSPORT_LOG_SERVER_H_
#define DTM_TRANSPORT_LOG_SERVER_H_

#include <sys/socket.h>
#include <sys/un.h>
#include <cstddef>
#include <map>
#include <string>
#include "base/macros.h"
#include "base/unix_server_socket.h"
#include "dtm/common/osaflog_protocol.h"
#include "base/log_writer.h"

// This class implements a loop that receives log messages over a UNIX socket
// and sends them to a LogWriter instance.
class LogServer {
 public:
  static constexpr size_t kMaxNoOfStreams = 32;
  static constexpr const char* kTransportdConfigFile =
                                       PKGSYSCONFDIR "/transportd.conf";

  static constexpr const char* kMdsLogStreamName = "mds.log";
  // @a term_fd is a file descriptor that will become readable when the program
  // should exit because it has received the SIGTERM signal.
  explicit LogServer(int term_fd);
  virtual ~LogServer();

  // Run in a loop, receiving log messages over the UNIX socket until the
  // process has received the SIGTERM signal, which is indicated by the caller
  // by making the term_fd (provided in the constructor) readable.
  void Run();
  // To read Transportd.conf
  bool ReadConfig(const char *transport_config_file);

 private:
  class LogStream {
   public:
    static constexpr size_t kMaxLogNameSize = 32;
    LogStream(const std::string& log_name, size_t max_backups,
                                                 size_t max_file_size);

    size_t log_name_size() const { return log_name_.size(); }
    const char* log_name_data() const { return log_name_.data(); }
    char* current_buffer_position() {
      return log_writer_.current_buffer_position();
    }
    bool empty() const { return log_writer_.empty(); }
    // Write @a size bytes of log message data in the memory pointed to by @a
    // buffer to the MDS log file. After the log message has been written, the
    // file will be rotated if necessary. This method performs blocking file
    // I/O.
    void Write(size_t size);
    void Flush();
    struct timespec last_flush() const {
      return last_flush_;
    }

   private:
    const std::string log_name_;
    struct timespec last_flush_;
    LogWriter log_writer_;
  };
  LogStream* GetStream(const char* msg_id, size_t msg_id_size);
  // Validate the log stream name, for security reasons. This method will check
  // that the string, when used as a file name, does not traverse the directory
  // structure (e.g. ../../../etc/passwd would be an illegal log stream
  // name). File names starting with a dot are also disallowed, since they would
  // result in hidden files.
  static bool ValidateLogName(const char* msg_id, size_t msg_id_size);
  void ExecuteCommand(const char* command, size_t size,
                      const struct sockaddr_un& addr, socklen_t addrlen);
  static bool ValidateAddress(const struct sockaddr_un& addr,
                              socklen_t addrlen);
  std::string ExecuteCommand(const std::string& command,
                             const std::string& argument);
  int term_fd_;
  // Configuration for LogServer
  size_t max_backups_;
  size_t max_file_size_;

  base::UnixServerSocket log_socket_;
  std::map<std::string, LogStream*> log_streams_;
  LogStream* current_stream_;
  size_t no_of_log_streams_;
  static const Osaflog::ClientAddressConstantPrefix address_header_;

  DELETE_COPY_AND_MOVE_OPERATORS(LogServer);
};

#endif  // DTM_TRANSPORT_LOG_SERVER_H_
