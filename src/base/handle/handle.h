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

#ifndef BASE_HANDLE_HANDLE_H_
#define BASE_HANDLE_HANDLE_H_

#include <pthread.h>
#include <set>

#include "base/handle/object_db.h"

namespace base {

namespace handle {

// A class providing support for implementing the Dispatch() and Finalize()
// functionality of AIS handles.
class Handle : public Object {
 public:
  Handle();
  // Indicate that the calling thread will enter a dispatch call on the @a
  // handle. This protects the handle from being finalized until the
  // LeaveDispatchCall() method is called for the handle by the same
  // thread. Nested dispatch calls are supported.
  //
  // Calls to EnterDispatchCall() can be nested - i.e. you can call
  // EnterDispatchCall() multiple times but you must ensure to call
  // LeaveDispatchCall() the same number of times.
  bool EnterDispatchCall();
  // Indicate that the calling thread will will leave the dispatch call on the
  // @a handle. The handle should be deleted by the caller of this method if the
  // Finalize() method has been called for the handle and the callerwas the last
  // user of the handle. If the Finalize() method has been called but the caller
  // was not the last user, it should instead call NotifyAll() on the handle's
  // condition variable.
  void LeaveDispatchCall();
  // The Finalize() method will set the finalizing flag and call the Cleanup()
  // method to free resources allocated to the handle. The calling thread will
  // then be blocked until all current users (if any) of the handle have called
  // the LeaveDispatchCall() method, and then finally the handle object will be
  // deleted. If the thread calling the Finalize() method is itself a user of
  // the handle, then the Finalize() method will return immediately after
  // calling the Cleanup() method. In this case, the handle object will be
  // deleted when the last user of the handle calls the set LeaveDispatchCall()
  // method.
  void Finalize();
  // Returns true if the handle object is being finalized. This method shall
  // only be used by threads that have already called the EnterDispatchCall()
  // method but not yet called the LeaveDispatchCall() method for the
  // handle. When the finalizing() method returns true, the calling thread shall
  // be aware that the Cleanup() method has already been called, which means
  // that resources associated with the handle have already been freed. In this
  // situation you should stop using the handle immediately by calling the
  // LeaveDispatchCall() method.
  bool finalizing() const { return finalizing_; }
  // Returns true if there are no threads inside a dispatch call on this handle.
  bool empty() const { return threads_inside_dispatch_call_.empty(); }
  // Returns true if this handle shall be deleted by the last thread after it
  // has called LeaveDispatchCall().
  bool delete_when_unused() const { return delete_when_unused_; }
  // Set the flag indicating that this handle shall be deleted by the last
  // thread after it has called LeaveDispatchCall().
  void set_delete_when_unused(bool value) { delete_when_unused_ = value; }
  // Returns true if the calling thread is inside a dispatch call on this
  // handle, i.e. it has called EnterDispatchCall() but not yet made a call to
  // LeaveDispatchCall(). Nested calls to EnterDispatchCall() are supported.
  bool is_this_thread_inside_dispatch_call() const {
    return threads_inside_dispatch_call_.find(pthread_self()) !=
           threads_inside_dispatch_call_.end();
  }

 protected:
  // Free all resources associated with the handle. This method should work both
  // in the case when the Cleanup() method has been called on the object, as
  // well as in the case when the Cleanup() method has not been called.
  virtual ~Handle();
  // Free most resources allocated to the handle, except the file desccriptor
  // (if any) associated with the handle. The file descriptor should be made
  // readable so that threads blocked in poll() on the file handle will wake up.
  //
  // Note: This method is only intended to be invoked by the base class
  // (base::Handle) to let subclasses free their resources.
  virtual void Cleanup() = 0;

 private:
  std::multiset<pthread_t> threads_inside_dispatch_call_;
  bool finalizing_;
  bool delete_when_unused_;
};

}  // namespace handle

}  // namespace base

#endif  // BASE_HANDLE_HANDLE_H_
