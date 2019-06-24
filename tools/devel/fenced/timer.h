/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2019 - All Rights Reserved.
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

#ifndef TIMER_H_
#define TIMER_H_

#include <sys/timerfd.h>
#include <unistd.h>
#include <cstdint>

#include "cpp_macros.h"

enum {
  // Number of nanoseconds per second.
  kNanosPerSec = 1000000000,

  // Number of microseconds per second.
  kMicrosPerSec = 1000000,

  // Number of milliseconds per second.
  kMillisPerSec = 1000
};

class Timer {
 public:
  enum class Type {kOneShot, kPeriodic};

  Timer();
  ~Timer() {close(timer_fd_);}
  int start(uint64_t interval_usec, Type type);
  void stop();
  int get_fd() const {return timer_fd_;}
 protected:
 private:
  int timer_fd_;
  DELETE_COPY_AND_MOVE_OPERATORS(Timer);
};

#endif  // TIMER_H_
