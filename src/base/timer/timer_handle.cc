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

#include "base/timer/timer_handle.h"

#include <unistd.h>
#include <cerrno>

#include "base/osaf_utility.h"
#include "base/timer/timer.h"

namespace base {

namespace timer {

TimerHandle::~TimerHandle() {
  if (fd_ >= 0) {
    int result = close(fd_);
    if (result < 0 && errno != EINTR) osaf_abort(fd_);
  }
}

void TimerHandle::EnqueueTimer(Timer* timer, uint64_t expiration_time) {
  uint64_t old_expiration = kInfiniteExpirationTime;
  if (!timer_queue_.empty()) {
    old_expiration = timer_queue_.begin()->expiration_time();
  }
  timer->set_iterator(timer_queue_.emplace(timer, expiration_time));
  uint64_t new_expiration = timer_queue_.begin()->expiration_time();
  if (new_expiration != old_expiration) {
    SetTimerfdExpirationTime(new_expiration);
  }
}

void TimerHandle::Stop(Timer* timer) {
  Iterator iter{timer->iterator()};
  if (iter != timer_queue_.end()) {
#ifdef ENABLE_DEBUG
    osafassert(!timer_queue_.empty());
#endif
    uint64_t old_expiration = timer_queue_.begin()->expiration_time();
    timer_queue_.erase(iter);
    timer->set_iterator(timer_queue_.end());
    uint64_t new_expiration = kInfiniteExpirationTime;
    if (!timer_queue_.empty()) {
      new_expiration = timer_queue_.begin()->expiration_time();
    }
    if (new_expiration != old_expiration) {
      SetTimerfdExpirationTime(new_expiration);
    }
  }
}

uint64_t TimerHandle::RemainingTime(Timer* timer) const {
#ifdef ENABLE_DEBUG
  osafassert(is_running(timer));
#endif
  uint64_t expiration_time = timer->iterator()->expiration_time();
  uint64_t current = GetTime();
  return expiration_time >= current ? expiration_time - current : 0;
}

void TimerHandle::SetTimerfdExpirationTime(uint64_t expiration_time) {
  struct itimerspec new_value {
    base::kZeroSeconds, base::NanosToTimespec(expiration_time)
  };
  if (timerfd_settime(fd_, TFD_TIMER_ABSTIME, &new_value, nullptr) != 0) {
    osaf_abort(expiration_time);
  }
}

// Invoked as a result of calling Handle::Finalize()
void TimerHandle::Cleanup() {
  for (const auto& e : timer_queue_) {
    e.timer()->set_iterator(timer_queue_.end());
  }
  timer_queue_.clear();
  struct itimerspec new_value {
    base::kZeroSeconds, base::ReadMonotonicClock()
  };
  int result = timerfd_settime(fd_, TFD_TIMER_ABSTIME, &new_value, nullptr);
  if (result != 0) osaf_abort(fd_);
}

Timer* TimerHandle::GetNextExpiredTimerInstance() {
  Iterator start = timer_queue_.begin();
  if (start == timer_queue_.end()) {
    if (!finalizing()) {
      SetTimerfdExpirationTime(kInfiniteExpirationTime);
    }
    return nullptr;
  }
  Timer* t = start->timer();
#ifdef ENABLE_DEBUG
  osafassert(is_running(t));
#endif
  uint64_t expiration_time = start->expiration_time();
  uint64_t current = GetTime();
  if (current >= expiration_time) {
    uint64_t expirations = 1;
    timer_queue_.erase(t->iterator());
    if (t->period_duration() == 0) {
      t->set_iterator(timer_queue_.end());
    } else {
      expirations += (current - expiration_time) / t->period_duration();
      expiration_time += expirations * t->period_duration();
      t->set_iterator(timer_queue_.emplace(t, expiration_time));
    }
    t->set_expiration_count(t->expiration_count() + expirations);
    return t;
  }
  SetTimerfdExpirationTime(expiration_time);
  return nullptr;
}

void TimerHandle::Start(Timer* timer, uint64_t expiration_time,
                        uint64_t period_duration) {
#ifdef ENABLE_DEBUG
  osafassert(!is_running(timer));
#endif
  EnqueueTimer(timer, expiration_time);
  timer->set_period_duration(period_duration);
  timer->set_expiration_count(0);
}

SaAisErrorT TimerHandle::Reschedule(Timer* timer, uint64_t expiration_time,
                                    uint64_t period_duration,
                                    uint64_t current_time) {
  SaAisErrorT result = SA_AIS_OK;
#ifdef ENABLE_DEBUG
  osafassert(is_running(timer));
#endif
  if ((period_duration == 0 && timer->period_duration() == 0) ||
      (period_duration != 0 && timer->period_duration() != 0)) {
    if (period_duration != 0 ||
        timer->iterator()->expiration_time() > current_time) {
      timer_queue_.erase(timer->iterator());
      EnqueueTimer(timer, expiration_time);
      timer->set_period_duration(period_duration);
    } else {
      result = SA_AIS_ERR_NOT_EXIST;
    }
  } else {
    result = SA_AIS_ERR_INVALID_PARAM;
  }
  return result;
}

SaAisErrorT TimerHandle::Skip(Timer* timer) {
  SaAisErrorT result = SA_AIS_OK;
#ifdef ENABLE_DEBUG
  osafassert(is_running(timer));
#endif
  if (timer->period_duration() != 0) {
    uint64_t expiration_time = timer->iterator()->expiration_time();
    timer_queue_.erase(timer->iterator());
    uint64_t current = GetTime();
    if (current >= expiration_time) {
      uint64_t expirations = 1;
      expirations += (current - expiration_time) / timer->period_duration();
      expiration_time += expirations * timer->period_duration();
      timer->set_expiration_count(timer->expiration_count() + expirations);
    }
    timer->set_iterator(timer_queue_.emplace(
        timer, expiration_time + timer->period_duration()));
  } else {
    result = SA_AIS_ERR_NOT_EXIST;
  }
  return result;
}

bool TimerHandle::is_running(Timer* timer) const {
  return timer->iterator() != timer_queue_.end();
}

}  // namespace timer

}  // namespace base
