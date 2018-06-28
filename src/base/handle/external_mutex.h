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

#ifndef BASE_HANDLE_EXTERNAL_MUTEX_H_
#define BASE_HANDLE_EXTERNAL_MUTEX_H_

#include <cstdint>

#include "base/condition_variable.h"
#include "base/macros.h"
#include "base/mutex.h"

namespace base {

namespace handle {

// A fixed set of mutexes and condition variables, that can be used to protect a
// variable (and possibly larger) set of objects. By using a mutex which is not
// stored inside of the object itself, we can safely delete objects safely even
// if they are used concurrently by multiple threads.
//
// To use an Object, first lock the mutex corresponding to that Object, then
// look up the object in the ObjectDb.  If the lookup fails, it means that the
// Object has already been deleted.
class ExternalMutex {
 public:
  ExternalMutex() {}

  // Returns a reference to the mutex corresponding to the Object with id @a
  // object_id. Note that a mutex may be shared by multiple Objects.
  base::Mutex& mutex(uint64_t object_id) {
    return mutex_[object_id & (kNumberOfMutexes - 1)];
  }
  // Returns a reference to the condition variable corresponding to the Object
  // with id @a object_id. Note that a condition variable may be shared by
  // multiple Objects. You must therefore always use NotifyAll() instead of
  // NotifyOne().
  base::ConditionVariable& condition_variable(uint64_t object_id) {
    return condition_variable_[object_id & (kNumberOfMutexes - 1)];
  }

 private:
  static constexpr uint64_t kNumberOfMutexes = 32;  // Must be a power of two
  base::Mutex mutex_[kNumberOfMutexes];
  base::ConditionVariable condition_variable_[kNumberOfMutexes];

  DELETE_COPY_AND_MOVE_OPERATORS(ExternalMutex);
};

}  // namespace handle

}  // namespace base

#endif  // BASE_HANDLE_EXTERNAL_MUTEX_H_
