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

#ifndef BASE_LOGTRACE_BUFFER_H_
#define BASE_LOGTRACE_BUFFER_H_

#include <vector>
#include <string>
#include <list>
#include "base/buffer.h"
#include "base/conf.h"
#include "base/macros.h"
#include "base/logtrace_client.h"
#include "base/log_writer.h"

class LogTraceClient;
// A class implements a buffer which is written in circular fashion
// This buffer is attached to its owner which is a LogTraceClient

class LogTraceBuffer {
 public:
  // define common constants
  // Max buffer size
  static const size_t kBufferSize_10K = 10240;
  // Identified string, added at beginning of logtrace. This string
  // can be used as searching key for the log trace records in coredump file
  constexpr static const char* kLogTraceString = "1qaz2wsx";
  LogTraceBuffer(LogTraceClient* owner, size_t buffer_size);
  ~LogTraceBuffer();
  void WriteToBuffer(std::string trace);
  bool FlushBuffer();
  void RequestFlush();
  void SetFlush(const bool flush) { flush_required_ = flush; }
 private:
  LogTraceClient* owner_;
  const size_t buffer_size_;
  size_t index_;
  int64_t tid_;
  std::vector<std::string> vector_;
  LogWriter* log_writer_;
  bool flush_required_;
  DELETE_COPY_AND_MOVE_OPERATORS(LogTraceBuffer);
};


#endif  // BASE_LOGTRACE_BUFFER_H_
