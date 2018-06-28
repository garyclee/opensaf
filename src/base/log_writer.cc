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

#include "base/log_writer.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include "base/getenv.h"
#include "osaf/configmake.h"

LogWriter::LogWriter(const std::string& log_name, size_t max_backups,
                                                  size_t max_file_size)
    : log_file_{base::GetEnv<std::string>("pkglogdir", PKGLOGDIR) + "/" +
                log_name},
      fd_{-1},
      current_file_size_{0},
      current_buffer_size_{0},
      max_backups_{max_backups},
      max_file_size_{max_file_size},
      buffer_{new char[kBufferSize]} {}

LogWriter::~LogWriter() {
  Flush();
  Close();
  delete[] buffer_;
}

std::string LogWriter::log_file(size_t backup) const {
  std::string file_name = log_file_;
  if (backup != 0) {
    file_name += std::string(".") + std::to_string(backup);
  }
  return file_name;
}

void LogWriter::Open() {
  if (fd_ < 0) {
    int fd;
    do {
      fd = open(log_file(0).c_str(), O_WRONLY | O_CLOEXEC | O_CREAT,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    } while (fd == -1 && errno == EINTR);
    if (fd >= 0) {
      off_t seek_result = lseek(fd, 0, SEEK_END);
      if (seek_result >= 0) current_file_size_ = seek_result;
      fd_ = fd;
    }
  }
}

void LogWriter::Close() {
  int fd = fd_;
  if (fd >= 0) {
    close(fd);
    fd_ = -1;
    current_file_size_ = 0;
  }
}

void LogWriter::RotateLog() {
  Close();
  unlink(log_file(max_backups_).c_str());
  for (size_t i = max_backups_; i != 0; --i) {
    std::string backup_name = log_file(i);
    std::string previous_backup = log_file(i - 1);
    if (rename(previous_backup.c_str(), backup_name.c_str()) != 0) {
      unlink(previous_backup.c_str());
    }
  }
}

void LogWriter::Write(const char* bytes, size_t size) {
  size_t bytes_written = 0;
  size_t bytes_chunk = 0;
  while (bytes_written < size) {
    if ((size - bytes_written) > (kBufferSize - current_buffer_size_)) {
      bytes_chunk = kBufferSize - current_buffer_size_;
    } else {
      bytes_chunk = size - bytes_written;
    }
    memcpy(current_buffer_position(), bytes + bytes_written, bytes_chunk);
    bytes_written += bytes_chunk;
    current_buffer_size_ += bytes_chunk;
    if (current_buffer_size_ >= kBufferSize ||
        current_buffer_size_ >= max_file_size_) Flush();
  }
}

void LogWriter::Write(size_t size) {
  current_buffer_size_ += size;
  if (current_buffer_size_ > kBufferSize - kMaxMessageSize ||
      current_buffer_size_ >= max_file_size_) Flush();
}

void LogWriter::Flush() {
  size_t size = current_buffer_size_;
  current_buffer_size_ = 0;
  if (size == 0) return;
  if (fd_ < 0) Open();
  if (fd_ < 0) return;
  if (current_file_size_ >= max_file_size_) {
    RotateLog();
    if (fd_ < 0) Open();
    if (fd_ < 0) return;
  }
  size_t bytes_written = 0;
  while (bytes_written < size) {
    ssize_t result = write(fd_, buffer_ + bytes_written, size - bytes_written);
    if (result < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
      break;
    }
    bytes_written += result;
  }
  current_file_size_ += bytes_written;
}
