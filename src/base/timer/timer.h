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

#ifndef BASE_TIMER_TIMER_H_
#define BASE_TIMER_TIMER_H_

#include <cstdint>

#include "base/handle/object_db.h"
#include "base/timer/timer_handle.h"

namespace base {

namespace timer {

class Timer : public handle::Object {
 public:
  explicit Timer(const TimerHandle::Iterator& i)
      : handle::Object{},
        period_duration_{},
        expiration_count_{},
        iterator_{i} {}
  uint64_t period_duration() const { return period_duration_; }
  void set_iterator(TimerHandle::Iterator&& i) { iterator_ = i; }
  void set_period_duration(uint64_t t) { period_duration_ = t; }
  const TimerHandle::Iterator& iterator() const { return iterator_; }
  uint64_t expiration_count() const { return expiration_count_; }
  void set_expiration_count(uint64_t n) { expiration_count_ = n; }

 protected:
  ~Timer() {}

 private:
  uint64_t period_duration_;
  uint64_t expiration_count_;
  TimerHandle::Iterator iterator_;
};

}  // namespace timer

}  // namespace base

#endif  // BASE_TIMER_TIMER_H_
