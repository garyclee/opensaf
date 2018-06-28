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

#ifndef BASE_TIMER_TIMER_HANDLE_H_
#define BASE_TIMER_TIMER_HANDLE_H_

#include <pthread.h>
#include <saAis.h>
#include <sys/timerfd.h>
#include <cstdint>
#include <set>
#include <functional>

#include "base/handle/handle.h"
#include "base/ncsgl_defs.h"
#include "base/time.h"

namespace base {

namespace timer {

class Timer;

// Top-level handle for the timer functionality.
class TimerHandle : public handle::Handle {
 public:
  // Specifies an infinite expiration time, i.e. no expiration.
  constexpr static uint64_t kInfiniteExpirationTime = 0;
  // An entry in the timer queue, with an absolute expiration time and a pointer
  // to a timer.
  class QueueEntry {
   public:
    // Construct a timer queue entry with the specified @a timer and absolute @a
    // expiration time.
    QueueEntry(Timer* timer, uint64_t expiration_time)
        : timer_{timer}, expiration_time_{expiration_time} {
#ifdef ENABLE_DEBUG
      osafassert(timer != nullptr);
      osafassert(expiration_time != 0);
#endif
    }
    // Less than operator - used by std::multiset
    bool operator<(const QueueEntry& e) const {
      return expiration_time_ < e.expiration_time_;
    }
    // Returns a pointer to the timer for this queue entry.
    Timer* timer() const { return timer_; }
    // Returns the absolute expiration time of this queue entry.
    uint64_t expiration_time() const { return expiration_time_; }

   private:
    Timer* timer_;
    uint64_t expiration_time_;
  };
  using Iterator = std::multiset<QueueEntry>::iterator;
  TimerHandle()
      : fd_(timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)), timer_queue_{} {}
  virtual ~TimerHandle();
  // Returns the current time.
  static uint64_t GetTime() {
    return base::TimespecToNanos(base::ReadMonotonicClock());
  }
  SaAisErrorT Skip(Timer* timer);
  int fd() const { return fd_; }
  uint64_t RemainingTime(Timer* timer) const;
  bool is_running(Timer* timer) const;
  void Stop(Timer* timer);

 protected:
  void Start(Timer* timer, uint64_t expiration_time, uint64_t period_duration);
  SaAisErrorT Reschedule(Timer* timer, uint64_t expiration_time,
                         uint64_t period_duration, uint64_t current_time);
  Timer* GetNextExpiredTimerInstance();
  Iterator end() const { return timer_queue_.end(); }

 private:
  void SetTimerfdExpirationTime(uint64_t expiration_time);
  void EnqueueTimer(Timer* timer, uint64_t expiration_time);
  void Cleanup();

  const int fd_;
  std::multiset<QueueEntry> timer_queue_;
};

}  // namespace timer

}  // namespace base

#endif  // BASE_TIMER_TIMER_HANDLE_H_
