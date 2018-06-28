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

#include "base/logtrace_buffer.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <thread>

LogTraceBuffer::LogTraceBuffer(LogTraceClient* owner, size_t buffer_size) :
  owner_(owner),
  buffer_size_(buffer_size),
  index_(0),
  flush_required_(false) {
  std::string log_file_name;
  tid_ = syscall(SYS_gettid);
  vector_.resize(buffer_size_);
  if (owner_) {
    owner_->AddExternalBuffer(tid_, this);
    log_file_name = std::string(owner_->app_name()) + "_" + owner_->proc_id()
        + "_" + std::to_string(tid_);
  } else {
    log_file_name = std::string(getpid() + "_" + std::to_string(tid_));
  }
  log_writer_ = new LogWriter{log_file_name, 10, LogWriter::kMaxFileSize_10MB};
}

LogTraceBuffer::~LogTraceBuffer() {
  if (owner_) {
    owner_->RemoveExternalBuffer(tid_);
  }
  delete log_writer_;
}

void LogTraceBuffer::WriteToBuffer(std::string trace) {
  vector_[index_] = kLogTraceString + trace;
  // add break line char
  if (trace.at(trace.length() - 1) != '\n')
    vector_[index_] += "\n";

  if (index_++ == buffer_size_) index_ = 0;
  if (flush_required_) FlushBuffer();
}
void LogTraceBuffer::RequestFlush() {
  flush_required_ = true;
  if (owner_) owner_->RequestFlushExternalBuffer();
}

bool LogTraceBuffer::FlushBuffer() {
  size_t i;
  // flushing the right half first
  for (i = index_ ; i < buffer_size_ ; i++) {
    if (vector_[i].length() > 0) {
      log_writer_->Write(vector_[i].c_str(), vector_[i].length());
      vector_[i] = "";
    }
  }
  // flushing the left half second
  for (i = 0 ; i < index_ ; i++) {
    if (vector_[i].length() > 0) {
      log_writer_->Write(vector_[i].c_str(), vector_[i].length());
      vector_[i] = "";
    }
  }
  log_writer_->Flush();
  flush_required_ = false;
  return true;
}

