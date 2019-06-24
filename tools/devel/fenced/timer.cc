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

#include "timer.h"

#include <syslog.h>
#include <cerrno>
#include <cstring>
#include <chrono>

Timer::Timer() {
  timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
}

int Timer::start(uint64_t interval_usec, Type type) {
  struct itimerspec spec;

  if (type == Type::kPeriodic) {
    spec.it_interval.tv_sec = interval_usec / kMicrosPerSec;
    spec.it_interval.tv_nsec =  (interval_usec % kMicrosPerSec) * (kNanosPerSec / kMicrosPerSec);
    spec.it_value.tv_sec = interval_usec / kMicrosPerSec;
    spec.it_value.tv_nsec = (interval_usec % kMicrosPerSec) * (kNanosPerSec / kMicrosPerSec);
  } else {
    spec.it_interval.tv_sec = 0;
    spec.it_interval.tv_nsec = 0;
    spec.it_value.tv_sec = interval_usec / kMicrosPerSec;
    spec.it_value.tv_nsec =  (interval_usec % kMicrosPerSec) * (kNanosPerSec / kMicrosPerSec);
  }

  if (timerfd_settime(timer_fd_, 0, &spec, NULL) < 0) {
    syslog(LOG_ERR, "timerfd_settime failed: %s", strerror(errno));
  }
  return timer_fd_;
}

void Timer::stop() {
  struct itimerspec spec;

  spec.it_interval.tv_sec = 0;
  spec.it_interval.tv_nsec = 0;
  spec.it_value.tv_sec = 0;
  spec.it_value.tv_nsec = 0;

  if (timerfd_settime(timer_fd_, 0, &spec, NULL) < 0) {
    syslog(LOG_ERR, "timerfd_settime failed: %s", strerror(errno));
  }
}

