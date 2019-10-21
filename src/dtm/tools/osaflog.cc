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
 */

#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <list>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "base/logtrace_buffer.h"
#include "base/log_writer.h"
#include "base/string_parse.h"
#include "base/time.h"
#include "base/unix_server_socket.h"
#include "dtm/common/osaflog_protocol.h"
#include "osaf/configmake.h"

namespace {

void PrintUsage(const char* program_name);
bool SendCommand(const std::string& command);
bool MaxTraceFileSize(uint64_t max_file_size);
bool SetMaxIdleTime(uint64_t max_idle);
bool NoOfBackupFiles(uint64_t number_of_backups);
bool Flush();
base::UnixServerSocket* CreateSocket();
uint64_t Random64Bits(uint64_t seed);
bool PrettyPrint(const std::string& log_stream);
bool Delete(const std::string& log_stream);
std::list<int> OpenLogFiles(const std::string& log_stream);
std::string PathName(const std::string& log_stream, int suffix);
uint64_t GetInode(int fd);
bool PrettyPrint(FILE* stream);
bool PrettyPrint(const char* line, size_t size);
int ExtractTrace(const std::string& core_file, const std::string& trace_file);
char buf[65 * 1024];

}  // namespace

int main(int argc, char** argv) {
  struct option long_options[] = {{"max-file-size", required_argument, 0, 'm'},
                                  {"max-backups", required_argument, 0, 'b'},
                                  {"flush", no_argument, 0, 'f'},
                                  {"print", no_argument, nullptr, 'p'},
                                  {"delete", no_argument, nullptr, 'd'},
                                  {"extract-trace", required_argument, 0, 'e'},
                                  {"max-idle", required_argument, 0, 'i'},
                                  {0, 0, 0, 0}};

  uint64_t max_file_size = 0;
  uint64_t max_backups = 0;
  uint64_t max_idle = 0;
  int option = 0;

  int long_index = 0;
  bool flush_result =  true;
  bool print_result =  true;
  bool delete_result =  true;
  bool max_file_size_result = true;
  bool number_of_backups_result = true;
  bool max_idle_result = true;
  bool flush_set = false;
  bool pretty_print_set = false;
  bool delete_set = false;
  bool max_file_size_set = false;
  bool max_backups_set = false;
  bool max_idle_set = false;
  bool thread_trace = false;
  std::string input_core = "";
  std::string output_trace = "";

  if (argc == 1) {
    PrintUsage(argv[0]);
    exit(EXIT_FAILURE);
  }

  while ((option = getopt_long(argc, argv, "m:b:p:f:e:",
                               long_options, &long_index)) != -1) {
    switch (option) {
      case 'p':
        pretty_print_set = true;
        flush_set = true;
        break;
      case 'd':
        delete_set = true;
        break;
      case 'f':
        flush_set = true;
        break;
      case 'm':
        max_file_size = base::StrToUint64(optarg,
                                          &max_file_size_set);
        if (!max_file_size_set || max_file_size > SIZE_MAX) {
          fprintf(stderr, "Illegal max-file-size argument\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 'b':
        max_backups = base::StrToUint64(optarg, &max_backups_set);
        if (!max_backups_set || max_backups > SIZE_MAX) {
          fprintf(stderr, "Illegal max-backups argument\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 'i':
        max_idle = base::StrToUint64(optarg, &max_idle_set);
        if (!max_idle_set || max_idle > Osaflog::kOneDayInMinute) {
          fprintf(stderr, "Illegal max-idle argument."
                  " Valid value is in the range [0-24*60]\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 'e':
        if (argv[optind] == nullptr || optarg == nullptr) {
          fprintf(stderr, "Coredump file or output trace file is "
                  "not specified in arguments\n");
          exit(EXIT_FAILURE);
        }
        input_core = std::string(optarg);
        output_trace = std::string(argv[optind]);
        struct stat statbuf;
        if (stat(input_core.c_str(), &statbuf) != 0) {
          fprintf(stderr, "Core dump file does not exist\n");
          exit(EXIT_FAILURE);
        }

        if (stat(output_trace.c_str(), &statbuf) == 0) {
          fprintf(stderr, "Output trace file already exists\n");
          exit(EXIT_FAILURE);
        }
        thread_trace = true;
        break;
      default: PrintUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (thread_trace) exit(ExtractTrace(input_core, output_trace));

  if (argc > optind && !pretty_print_set && !delete_set) {
    pretty_print_set = true;
    flush_set = true;
  }

  if ((argc <= optind && (pretty_print_set || delete_set)) ||
      (pretty_print_set && delete_set)) {
     PrintUsage(argv[0]);
     exit(EXIT_FAILURE);
  }

  if (flush_set == true) {
     flush_result = Flush();
  }
  if (pretty_print_set == true) {
    while (print_result && optind < argc) {
      print_result = PrettyPrint(argv[optind++]);
    }
  }
  if (delete_set == true) {
    while (delete_result && optind < argc) {
      delete_result = Delete(argv[optind++]);
    }
  }
  if (max_backups_set == true) {
     number_of_backups_result = NoOfBackupFiles(max_backups);
  }
  if (max_file_size_set == true) {
     max_file_size_result = MaxTraceFileSize(max_file_size);
  }
  if (max_idle_set == true) {
    max_idle_result = SetMaxIdleTime(max_idle);
  }
  if (flush_result && print_result && max_file_size_result &&
      delete_result && number_of_backups_result && max_idle_result)
     exit(EXIT_SUCCESS);
  exit(EXIT_FAILURE);
}

namespace {

void PrintUsage(const char* program_name) {
  fprintf(stderr,
          "Usage: %s [OPTION] [LOGSTREAM...]\n"
          "\n"
          "print the messages stored on disk for the specified\n"
          "LOGSTREAM. When a LOGSTREAM argument is specified, the option\n"
          "--flush is implied.\n"
          "\n"
          "Options:\n"
          "\n"
          "--flush               Flush all buffered messages in the log\n"
          "                      server to disk even when no LOGSTREAM\n"
          "                      is specified.\n"
          "--print               print the messages stored on disk for the\n"
          "                      specified LOGSTREAM(s). This option is the\n"
          "                      default when no option is specified.\n"
          "--delete              Delete the specified LOGSTREAM(s) by\n"
          "                      removing allocated resources in the log\n"
          "                      server. Does not delete log files from disk.\n"
          "--max-file-size=SIZE  Set the maximum size of the log file to\n"
          "                      SIZE bytes. The log file will be rotated\n"
          "                      when it exceeds this size. Suffixes k, M and\n"
          "                      G can be used for kilo-, mega- and\n"
          "                      gigabytes.\n"
          "--max-backups=NUM     Set the maximum number of backup files to\n"
          "                      retain during log rotation to NUM.\n"
          "--extract-trace <corefile> <tracefile>\n"
          "                      If a process produces a core dump file has\n"
          "                      THREAD_TRACE_BUFFER enabled, this option\n"
          "                      reads the <corefile> to extract the trace\n"
          "                      strings in all threads and writes them to\n"
          "                      the <tracefile> file.\n"
          "--max-idle=NUM        Set the maximum number of idle time to NUM\n"
          "                      minutes. If a stream has not been used for\n"
          "                      the given time, the stream will be closed.\n"
          "                      Given zero (default) to max-idle to disable\n"
          "                      this functionality.\n",
          program_name);
}

bool SendCommand(const std::string& command) {
  std::string request(std::string("?") + command);
  std::string expected_reply(std::string("!") + command);
  auto sock = std::unique_ptr<base::UnixServerSocket>(CreateSocket());

  if (!sock) {
    fprintf(stderr, "Failed to create UNIX domain socket\n");
    return false;
  }

  struct sockaddr_un osaftransportd_addr;
  socklen_t addrlen = base::UnixSocket::SetAddress(Osaflog::kServerSocketPath,
                                                   &osaftransportd_addr);

  ssize_t result = sock->SendTo(request.data(), request.size(),
                                                &osaftransportd_addr, addrlen);
  if (result < 0) {
    perror("Failed to send message to osaftransportd");
    return false;
  } else if (static_cast<size_t>(result) != request.size()) {
    fprintf(stderr, "Failed to send message to osaftransportd\n");
    return false;
  }

  struct timespec end_time = base::ReadMonotonicClock() + base::kTenSeconds;
  for (;;) {
    struct pollfd fds {
      sock->fd(), POLLIN, 0
    };
    struct timespec current_time = base::ReadMonotonicClock();
    result = 0;
    if (current_time >= end_time) {
      fprintf(stderr, "Timeout\n");
      return false;
    }
    struct timespec timeout = end_time - current_time;
    result = ppoll(&fds, 1, &timeout, NULL);
    if (result < 0) {
      perror("Failed to wait for reply from osaftransportd");
      return false;
    } else if (result == 0) {
      fprintf(stderr, "Timeout\n");
      return false;
    }
    struct sockaddr_un sender_addr;
    socklen_t sender_addrlen = sizeof(sender_addr);
    result = sock->RecvFrom(buf, sizeof(buf), &sender_addr, &sender_addrlen);
    if (result < 0) break;
    if (sender_addrlen == addrlen &&
        memcmp(&osaftransportd_addr, &sender_addr, addrlen) == 0)
      break;
  }
  if (result < 0) {
    perror("Failed to receive reply from osaftransportd");
    return false;
  } else if (static_cast<size_t>(result) != expected_reply.size() ||
             memcmp(buf, expected_reply.data(), result) != 0) {
    fprintf(stderr, "ERROR: osaftransportd replied '%s'\n",
            std::string{buf, static_cast<size_t>(result)}.c_str());
    return false;
  }
  return true;
}

bool MaxTraceFileSize(uint64_t max_file_size) {
  return SendCommand(std::string("max-file-size ") +
                     std::to_string(max_file_size));
}

bool NoOfBackupFiles(uint64_t max_backups) {
  return SendCommand(std::string("max-backups ") + std::to_string(max_backups));
}

bool SetMaxIdleTime(uint64_t max_idle) {
  return SendCommand(std::string("max-idle-time ") +
                     std::to_string(max_idle));
}

bool Flush() {
  return SendCommand(std::string{"flush"});
}

base::UnixServerSocket* CreateSocket() {
  base::UnixServerSocket* sock = nullptr;
  Osaflog::ClientAddress addr{};
  addr.pid = getpid();
  for (uint64_t i = 1; i <= 1000; ++i) {
    addr.random = Random64Bits(i);
    sock = new base::UnixServerSocket(addr.sockaddr(), addr.sockaddr_length(),
                                      base::UnixSocket::kNonblocking);
    if (sock->fd() >= 0 || errno != EADDRINUSE) break;
    delete sock;
    sock = nullptr;
    sched_yield();
  }
  if (sock != nullptr && sock->fd() < 0) {
    delete sock;
    sock = nullptr;
  }
  return sock;
}

uint64_t Random64Bits(uint64_t seed) {
  std::mt19937_64 generator{base::TimespecToNanos(base::ReadRealtimeClock()) *
                            seed};
  return generator();
}

bool PrettyPrint(const std::string& log_stream) {
  std::list<int> fds = OpenLogFiles(log_stream);
  uint64_t last_inode = ~UINT64_C(0);
  bool result = true;
  for (const auto& fd : fds) {
    uint64_t inode = GetInode(fd);
    if (inode == last_inode) {
      close(fd);
      continue;
    }
    last_inode = inode;
    FILE* stream = fdopen(fd, "r");
    if (stream != nullptr) {
      if (!PrettyPrint(stream)) result = false;
      fclose(stream);
    } else {
      result = false;
      close(fd);
    }
  }
  return result;
}

bool Delete(const std::string& log_stream) {
  return SendCommand(std::string("delete ") +
                     log_stream);
}

std::list<int> OpenLogFiles(const std::string& log_stream) {
  std::list<int> result{};
  bool last_open_failed = false;
  for (int suffix = 0; suffix < 100; ++suffix) {
    std::string path_name = PathName(log_stream, suffix);
    std::string next_path_name = PathName(log_stream, suffix + 1);
    int fd;
    do {
      fd = open(path_name.c_str(), O_RDONLY);
    } while (fd < 0 && errno == EINTR);
    if (fd >= 0) {
      result.push_front(fd);
      last_open_failed = false;
    } else {
      if (errno != ENOENT) perror("Could not open log file");
      if (last_open_failed || errno != ENOENT) break;
      last_open_failed = true;
    }
  }
  return result;
}

std::string PathName(const std::string& log_stream, int suffix) {
  std::string path_name{PKGLOGDIR "/"};
  path_name += log_stream;
  if (suffix != 0) path_name += std::string(".") + std::to_string(suffix);
  return path_name;
}

uint64_t GetInode(int fd) {
  struct stat buf;
  int result = fstat(fd, &buf);
  return result == 0 ? buf.st_ino : ~UINT64_C(0);
}

bool PrettyPrint(FILE* stream) {
  bool result = true;
  for (;;) {
    char* s = fgets(buf, sizeof(buf), stream);
    if (s == nullptr) break;
    if (!PrettyPrint(s, strlen(buf))) result = false;
  }
  if (!feof(stream)) {
    fprintf(stderr, "Error reading from file\n");
    result = false;
  }
  return result;
}

bool PrettyPrint(const char* line, size_t size) {
  size_t date_size = 0;
  const char* date_field = Osaflog::GetField(line, size, 1, &date_size);
  size_t msg_size = 0;
  const char* msg_field = Osaflog::GetField(line, size, 8, &msg_size);
  char pretty_date[33];
  if (date_field == nullptr || date_size < 19 ||
      date_size >= sizeof(pretty_date) || date_field[10] != 'T' ||
      msg_field == nullptr || msg_size == 0)
    return false;
  memcpy(pretty_date, date_field, date_size);
  memset(pretty_date + date_size, '0', sizeof(pretty_date) - 1 - date_size);
  pretty_date[10] = ' ';
  pretty_date[19] = '.';
  pretty_date[23] = '\0';
  printf("%s %s", pretty_date, msg_field);
  return true;
}

int SearchStringInFile(std::ifstream& stream, const char* str) {
  int rc = 0;
  int len = strlen(str);
  int i;
  while (!stream.eof() && rc == 0) {
    for (i = 0; i < len; i++) {
      char c;
      if (!stream.get(c) || str[i] != c) break;
    }
    if (i == len) rc = 1;
  }
  return rc;
}

int ExtractTrace(const std::string& core_file,
    const std::string& trace_file) {
  std::vector<std::string> v_str{};
  std::ifstream corefile_stream {core_file, std::ifstream::binary};
  // extract
  if (!corefile_stream.fail()) {
    while (!corefile_stream.eof()) {
      int s = SearchStringInFile(corefile_stream,
          LogTraceBuffer::kLogTraceString);
      if (s == 1) {
        std::string str;
        std::getline(corefile_stream, str, '\n');
        str += "\n";
        v_str.push_back(std::string(str));
      }
    }
    corefile_stream.close();
  } else {
    fprintf(stderr, "Failed to open coredump file\n");
    return EXIT_FAILURE;
  }
  if (v_str.size() == 0) {
    fprintf(stderr, "No trace string is found in coredump file\n");
    return EXIT_FAILURE;
  }
  // sort by sequenceId
  std::sort(v_str.begin(), v_str.end(),
            [](const std::string a, const std::string b) -> bool {
            int seqa, seqb;
            std::sscanf(a.c_str(),"%*s %*s %*s %*s %*s %*s %*s "
                "sequenceId=\"%d\"]", &seqa);
            std::sscanf(b.c_str(),"%*s %*s %*s %*s %*s %*s %*s "
                "sequenceId=\"%d\"]", &seqb);
            return a < b;
            });
  // write
  LogWriter log_writer{"", 1, LogWriter::kMaxFileSize_10MB};
  log_writer.SetLogFile(trace_file);
  for (auto str : v_str) log_writer.Write(str.c_str(), str.length());
  log_writer.Flush();
  return EXIT_SUCCESS;
}
}  // namespace
