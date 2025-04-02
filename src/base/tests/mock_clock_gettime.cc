/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2015 The OpenSAF Foundation
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

#include "base/tests/mock_clock_gettime.h"
#include <dlfcn.h>
#include <cerrno>
#include "base/osaf_time.h"

#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME 7
#endif

timespec realtime_clock;
timespec monotonic_clock;
timespec boottime_clock;
MockClockGettime mock_clock_gettime;
typedef int (*ClockGettimeFn)(clockid_t, struct timespec*);
static ClockGettimeFn real_clock_gettime = nullptr;

int clock_gettime(clockid_t clock_id, struct timespec* tp) {
  if (real_clock_gettime == nullptr) {
    real_clock_gettime =
        reinterpret_cast<ClockGettimeFn>(dlsym(RTLD_NEXT, "clock_gettime"));
  }
  if (mock_clock_gettime.disable_mock && real_clock_gettime != nullptr) {
    return real_clock_gettime(clock_id, tp);
  }
  struct timespec* clock_source = nullptr;
  if (clock_id == CLOCK_REALTIME) {
    clock_source = &realtime_clock;
  } else if (clock_id == CLOCK_MONOTONIC) {
    clock_source = &monotonic_clock;
  } else if (clock_id == CLOCK_BOOTTIME) {
    clock_source = &boottime_clock;
  } else {
    errno = EINVAL;
    return -1;
  }
  if (clock_source != nullptr) *tp = *clock_source;
  osaf_timespec_add(&realtime_clock, &mock_clock_gettime.execution_time,
                    &realtime_clock);
  osaf_timespec_add(&monotonic_clock, &mock_clock_gettime.execution_time,
                    &monotonic_clock);
  osaf_timespec_add(&boottime_clock, &mock_clock_gettime.execution_time,
                    &boottime_clock);
  if (mock_clock_gettime.return_value < 0)
    errno = mock_clock_gettime.errno_value;
  return mock_clock_gettime.return_value;
}
