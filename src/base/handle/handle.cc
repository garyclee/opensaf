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

#include "base/handle/handle.h"

#include <new>

#include "base/ncsgl_defs.h"

namespace base {

namespace handle {

Handle::Handle()
    : Object{},
      threads_inside_dispatch_call_{},
      finalizing_{false},
      delete_when_unused_{false} {}

Handle::~Handle() { osafassert(threads_inside_dispatch_call_.empty()); }

bool Handle::EnterDispatchCall() {
#ifdef ENABLE_DEBUG
  osafassert(!finalizing_);
#endif
  bool success = true;
  try {
    threads_inside_dispatch_call_.insert(pthread_self());
  } catch (std::bad_alloc&) {
    success = false;
  }
  return success;
}

void Handle::LeaveDispatchCall() {
  auto iter = threads_inside_dispatch_call_.find(pthread_self());
#ifdef ENABLE_DEBUG
  osafassert(iter != threads_inside_dispatch_call_.end());
#endif
  threads_inside_dispatch_call_.erase(iter);
}

void Handle::Finalize() {
  osafassert(!finalizing_);
  finalizing_ = true;
  Cleanup();
}

}  // namespace handle

}  // namespace base
