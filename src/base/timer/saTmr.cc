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

#include "ais/include/saTmr.h"

#include <pthread.h>
#include <time.h>
#include <cinttypes>
#include <cstdint>
#include <new>

#include "base/handle/external_mutex.h"
#include "base/timer/timer.h"
#include "base/osaf_poll.h"
#include "base/time.h"
#include "base/macros.h"

extern "C" {

static SaAisErrorT osafTmrInitialize(SaTmrHandleT* timerHandle,
                                     const SaTmrCallbacksT* timerCallbacks,
                                     SaVersionT* version);

static SaAisErrorT osafTmrSelectionObjectGet(
    SaTmrHandleT timerHandle, SaSelectionObjectT* selectionObject);

static SaAisErrorT osafTmrDispatch(SaTmrHandleT timerHandle,
                                   SaDispatchFlagsT dispatchFlags);

static SaAisErrorT osafTmrFinalize(SaTmrHandleT timerHandle);

static SaAisErrorT osafTmrTimerStart(
    SaTmrHandleT timerHandle, const SaTmrTimerAttributesT* timerAttributes,
    const void* timerData, SaTmrTimerIdT* timerId, SaTimeT* callTime);

static SaAisErrorT osafTmrTimerReschedule(
    SaTmrHandleT timerHandle, SaTmrTimerIdT timerId,
    const SaTmrTimerAttributesT* timerAttributes, SaTimeT* callTime);

static SaAisErrorT osafTmrTimerCancel(SaTmrHandleT timerHandle,
                                      SaTmrTimerIdT timerId, void** timerDataP);

static SaAisErrorT osafTmrPeriodicTimerSkip(SaTmrHandleT timerHandle,
                                            SaTmrTimerIdT timerId);

static SaAisErrorT osafTmrTimerRemainingTimeGet(SaTmrHandleT timerHandle,
                                                SaTmrTimerIdT timerId,
                                                SaTimeT* remainingTime);

static SaAisErrorT osafTmrTimerAttributesGet(
    SaTmrHandleT timerHandle, SaTmrTimerIdT timerId,
    SaTmrTimerAttributesT* timerAttributes);

static SaAisErrorT osafTmrTimeGet(SaTmrHandleT timerHandle,
                                  SaTimeT* currentTime);

static SaAisErrorT osafTmrClockTickGet(SaTmrHandleT timerHandle,
                                       SaTimeT* clockTick);

}  // extern "C"

namespace base {

namespace timer {

class SaTmrTimer : public Timer {
 public:
  SaTmrTimer(const TimerHandle::Iterator& iter, void* argument)
      : Timer(iter), argument_(argument), type_{}, initial_expiration_{0} {}
  void* argument() const { return argument_; }
  uint64_t initial_expiration() const { return initial_expiration_; }
  SaTmrTimeTypeT type() const { return type_; }
  void SetTypeAndInitialExpiry(SaTmrTimeTypeT type, int64_t initial_expiry) {
#ifdef ENABLE_DEBUG
    osafassert(type == SA_TIME_ABSOLUTE || type == SA_TIME_DURATION);
#endif
    type_ = type;
    initial_expiration_ = initial_expiry;
  }

 private:
  void* argument_;
  SaTmrTimeTypeT type_;
  int64_t initial_expiration_;

  DELETE_COPY_AND_MOVE_OPERATORS(SaTmrTimer);
};

class SaTmrHandle : public TimerHandle {
 public:
  explicit SaTmrHandle(const SaTmrCallbacksT* timerCallbacks)
      : TimerHandle{},
        timer_expired_callback_{timerCallbacks != nullptr
                                    ? timerCallbacks->saTmrTimerExpiredCallback
                                    : nullptr},
        all_timers_{} {}
  ~SaTmrHandle();
  SaTmrTimer* AllocateTimer(void* argument) {
    SaTmrTimer* timer = new (std::nothrow) SaTmrTimer(end(), argument);
    if (timer != nullptr) {
      try {
        all_timers_.Add(timer);
      } catch (std::bad_alloc&) {
        delete timer;
        timer = nullptr;
      }
    }
    return timer;
  }

  SaTmrTimer* RemoveTimer(SaTmrTimerIdT timerId) {
    handle::Object* timer =
        all_timers_.Remove(static_cast<handle::Object::Id>(timerId));
    return static_cast<SaTmrTimer*>(timer);
  }

  static SaTmrHandle* AllocateHandle(const SaTmrCallbacksT* timerCallbacks) {
    SaTmrHandle* handle = new (std::nothrow) SaTmrHandle(timerCallbacks);
    if (handle != nullptr && !all_handles_->Add(handle)) {
      delete handle;
      handle = nullptr;
    }
    return handle;
  }
  static SaTmrHandle* RemoveHandle(SaTmrHandleT handle) {
    handle::Object* hdl =
        all_handles_->Remove(static_cast<handle::Object::Id>(handle));
    return static_cast<SaTmrHandle*>(hdl);
  }

  bool Dispatch();
  bool CallbackInstalled() const { return timer_expired_callback_ != nullptr; }
  void GetAttributes(SaTmrTimer* timer, SaTmrTimerAttributesT* attributes) {
#ifdef ENABLE_DEBUG
    osafassert(is_running(timer));
#endif
    attributes->type = timer->type();
    attributes->initialExpirationTime = timer->initial_expiration();
    attributes->timerPeriodDuration = timer->period_duration();
  }
  void Start(SaTmrTimer* timer, uint64_t expiration_time,
             uint64_t period_duration, SaTmrTimeTypeT type,
             uint64_t initial_expiry) {
    TimerHandle::Start(timer, expiration_time, period_duration);
    timer->SetTypeAndInitialExpiry(type, initial_expiry);
  }
  SaAisErrorT Restart(SaTmrTimer* timer, uint64_t expiration_time,
                      uint64_t period_duration, uint64_t current_time,
                      SaTmrTimeTypeT type, uint64_t initial_expiry) {
    SaAisErrorT result = TimerHandle::Reschedule(timer, expiration_time,
                                                 period_duration, current_time);
    if (result == SA_AIS_OK) {
      timer->SetTypeAndInitialExpiry(type, initial_expiry);
    }
    return result;
  }
  SaTmrTimer* Lookup(SaTmrTimerIdT timerId) {
    return static_cast<SaTmrTimer*>(
        all_timers_.Lookup(static_cast<handle::Object::Id>(timerId)));
  }
  static bool InitStatic() {
    int result = pthread_once(&once_control_, PthreadOnceInitRoutine);
    if (result != 0) osaf_abort(result);
    return mutex_store_ != nullptr && all_handles_ != nullptr;
  }
  static Mutex& mutex(SaTmrHandleT handle) {
    return mutex_store_->mutex(static_cast<handle::Object::Id>(handle));
  }
  Mutex& mutex() { return mutex_store_->mutex(id()); }
  ConditionVariable& condition_variable() {
    return mutex_store_->condition_variable(id());
  }
  static SaTmrHandle* LookupHandle(SaTmrHandleT timerHandle) {
    handle::Object* hdl = all_handles_->Lookup(timerHandle);
    return static_cast<SaTmrHandle*>(hdl);
  }

 private:
  static void PthreadOnceInitRoutine();
  static void DeleteTimer(handle::Object* object);
  const SaTmrTimerExpiredCallbackT timer_expired_callback_;
  handle::ObjectDb all_timers_;
  static pthread_once_t once_control_;
  static handle::ExternalMutex* mutex_store_;
  static handle::ObjectDb* all_handles_;

  DELETE_COPY_AND_MOVE_OPERATORS(SaTmrHandle);
};

pthread_once_t SaTmrHandle::once_control_ = PTHREAD_ONCE_INIT;
handle::ExternalMutex* SaTmrHandle::mutex_store_ = nullptr;
handle::ObjectDb* SaTmrHandle::all_handles_ = nullptr;

SaTmrHandle::~SaTmrHandle() { all_timers_.Clear(DeleteTimer); }

void SaTmrHandle::PthreadOnceInitRoutine() {
  assert(mutex_store_ == nullptr && all_handles_ == nullptr);
  if (handle::ObjectDb::InitStatic()) {
    mutex_store_ = new (std::nothrow) handle::ExternalMutex();
    all_handles_ = new (std::nothrow) handle::ObjectDb();
    if (mutex_store_ == nullptr || all_handles_ == nullptr) {
      delete mutex_store_;
      delete all_handles_;
      mutex_store_ = nullptr;
      all_handles_ = nullptr;
    }
  }
}

void SaTmrHandle::DeleteTimer(handle::Object* object) {
  SaTmrTimer* timer = static_cast<SaTmrTimer*>(object);
  delete timer;
}

bool SaTmrHandle::Dispatch() {
  void* argument;
  uint64_t expiration_count;
  handle::Object::Id timer_id;
  SaTmrTimer* timer = static_cast<SaTmrTimer*>(GetNextExpiredTimerInstance());
  if (timer == nullptr) return false;
  argument = timer->argument();
  expiration_count = timer->expiration_count();
  timer_id = timer->id();
  if (timer->period_duration() == 0) {
#ifdef ENABLE_DEBUG
    osafassert(!is_running(timer));
#endif
    all_timers_.Remove(timer_id);
    delete timer;
  }
  const SaTmrTimerExpiredCallbackT callback = timer_expired_callback_;
  mutex().Unlock();
  callback(static_cast<SaTmrTimerIdT>(timer_id), argument, expiration_count);
  mutex().Lock();
  return true;
}

}  // namespace timer

}  // namespace base

static SaAisErrorT osafTmrInitialize(SaTmrHandleT* timerHandle,
                                     const SaTmrCallbacksT* timerCallbacks,
                                     SaVersionT* version) {
  version->minorVersion = static_cast<SaUint8T>(1);
  if (version->releaseCode != static_cast<SaUint8T>('A') ||
      version->majorVersion != static_cast<SaUint8T>(1)) {
    version->releaseCode = static_cast<SaUint8T>('A');
    version->majorVersion = static_cast<SaUint8T>(1);
    return SA_AIS_ERR_VERSION;
  }
  if (base::timer::SaTmrHandle::InitStatic()) {
    base::timer::SaTmrHandle* handle =
        base::timer::SaTmrHandle::AllocateHandle(timerCallbacks);
    if (handle != nullptr) {
      if (handle->fd() >= 0) {
        *timerHandle = handle->id();
        return SA_AIS_OK;
      } else {
        delete handle;
        return SA_AIS_ERR_NO_RESOURCES;
      }
    } else {
      return SA_AIS_ERR_NO_MEMORY;
    }
  } else {
    return SA_AIS_ERR_NO_MEMORY;
  }
}

static SaAisErrorT osafTmrSelectionObjectGet(
    SaTmrHandleT timerHandle, SaSelectionObjectT* selectionObject) {
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  *selectionObject = handle->fd();
  return SA_AIS_OK;
}

static SaAisErrorT osafTmrDispatch(SaTmrHandleT timerHandle,
                                   SaDispatchFlagsT dispatchFlags) {
  base::Mutex& mutex = base::timer::SaTmrHandle::mutex(timerHandle);
  base::Lock lock(mutex);
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  if (!handle->EnterDispatchCall()) {
    return SA_AIS_ERR_NO_MEMORY;
  }
  SaAisErrorT result = SA_AIS_OK;
  switch (dispatchFlags) {
    case SA_DISPATCH_ONE:
      handle->Dispatch();
      break;
    case SA_DISPATCH_ALL:
      while (handle->Dispatch()) {
      }
      break;
    case SA_DISPATCH_BLOCKING:
      do {
        mutex.Unlock();
        osaf_poll_one_fd(handle->fd(), -1);
        mutex.Lock();
        while (handle->Dispatch()) {
        }
      } while (!handle->finalizing());
      break;
    default:
      result = SA_AIS_ERR_INVALID_PARAM;
      break;
  }
  handle->LeaveDispatchCall();
  if (handle->finalizing() && handle->empty()) {
    if (handle->delete_when_unused()) {
      delete handle;
    } else {
      handle->condition_variable().NotifyAll();
    }
  }
  return result;
}

static SaAisErrorT osafTmrFinalize(SaTmrHandleT timerHandle) {
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::RemoveHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  handle->Finalize();
  if (!handle->is_this_thread_inside_dispatch_call()) {
    handle->condition_variable().Wait<std::function<bool()>>(
        &lock, [handle] { return handle->empty(); });
    delete handle;
  } else {
    handle->set_delete_when_unused(true);
  }
  return SA_AIS_OK;
}

static SaAisErrorT osafTmrTimerStart(
    SaTmrHandleT timerHandle, const SaTmrTimerAttributesT* timerAttributes,
    const void* timerData, SaTmrTimerIdT* timerId, SaTimeT* callTime) {
  if (timerAttributes == nullptr || timerId == nullptr || callTime == nullptr) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  if (timerAttributes->initialExpirationTime < 0 ||
      timerAttributes->timerPeriodDuration < 0) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  uint64_t current_time = base::timer::SaTmrHandle::GetTime();
  uint64_t expiration_time = timerAttributes->initialExpirationTime;
  switch (timerAttributes->type) {
    case SA_TIME_ABSOLUTE:
      if (expiration_time <= current_time) return SA_AIS_ERR_INVALID_PARAM;
      break;
    case SA_TIME_DURATION:
      if (expiration_time == 0 ||
          (SA_TIME_MAX - current_time) < expiration_time)
        return SA_AIS_ERR_INVALID_PARAM;
      expiration_time += current_time;
      break;
    default:
      return SA_AIS_ERR_INVALID_PARAM;
  }
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  if (!handle->CallbackInstalled()) return SA_AIS_ERR_INIT;
  base::timer::SaTmrTimer* timer =
      handle->AllocateTimer(const_cast<void*>(timerData));
  if (timer == nullptr) return SA_AIS_ERR_NO_MEMORY;
  handle->Start(timer, expiration_time, timerAttributes->timerPeriodDuration,
                timerAttributes->type, timerAttributes->initialExpirationTime);
  *timerId = timer->id();
  *callTime = current_time;
  return SA_AIS_OK;
}

static SaAisErrorT osafTmrTimerReschedule(
    SaTmrHandleT timerHandle, SaTmrTimerIdT timerId,
    const SaTmrTimerAttributesT* timerAttributes, SaTimeT* callTime) {
  if (timerAttributes == nullptr || callTime == nullptr) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  if (timerAttributes->initialExpirationTime < 0 ||
      timerAttributes->timerPeriodDuration < 0) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  uint64_t current_time = base::timer::SaTmrHandle::GetTime();
  uint64_t expiration_time = timerAttributes->initialExpirationTime;
  switch (timerAttributes->type) {
    case SA_TIME_ABSOLUTE:
      if (expiration_time <= current_time) return SA_AIS_ERR_INVALID_PARAM;
      break;
    case SA_TIME_DURATION:
      if (expiration_time == 0 ||
          (SA_TIME_MAX - current_time) < expiration_time)
        return SA_AIS_ERR_INVALID_PARAM;
      expiration_time += current_time;
      break;
    default:
      return SA_AIS_ERR_INVALID_PARAM;
  }
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  base::timer::SaTmrTimer* timer = handle->Lookup(timerId);
  if (timer == nullptr) return SA_AIS_ERR_NOT_EXIST;
  SaAisErrorT result = handle->Restart(timer, expiration_time,
                                       timerAttributes->timerPeriodDuration,
                                       current_time, timerAttributes->type,
                                       timerAttributes->initialExpirationTime);
  if (result == SA_AIS_OK) {
    *callTime = current_time;
  }
  return result;
}

static SaAisErrorT osafTmrTimerCancel(SaTmrHandleT timerHandle,
                                      SaTmrTimerIdT timerId,
                                      void** timerDataP) {
  if (timerDataP == nullptr) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  base::timer::SaTmrTimer* timer = handle->RemoveTimer(timerId);
  if (timer == nullptr) return SA_AIS_ERR_NOT_EXIST;
#ifdef ENABLE_DEBUG
  osafassert(handle->is_running(timer));
#endif
  *timerDataP = timer->argument();
  handle->Stop(timer);
  delete timer;
  return SA_AIS_OK;
}

static SaAisErrorT osafTmrPeriodicTimerSkip(SaTmrHandleT timerHandle,
                                            SaTmrTimerIdT timerId) {
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  base::timer::SaTmrTimer* timer = handle->Lookup(timerId);
  if (timer == nullptr) return SA_AIS_ERR_NOT_EXIST;
  return handle->Skip(timer);
}

static SaAisErrorT osafTmrTimerRemainingTimeGet(SaTmrHandleT timerHandle,
                                                SaTmrTimerIdT timerId,
                                                SaTimeT* remainingTime) {
  if (remainingTime == nullptr) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  base::timer::SaTmrTimer* timer = handle->Lookup(timerId);
  if (timer == nullptr) return SA_AIS_ERR_NOT_EXIST;
  *remainingTime = handle->RemainingTime(timer);
  return SA_AIS_OK;
}

static SaAisErrorT osafTmrTimerAttributesGet(
    SaTmrHandleT timerHandle, SaTmrTimerIdT timerId,
    SaTmrTimerAttributesT* timerAttributes) {
  if (timerAttributes == nullptr) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  base::Lock lock(base::timer::SaTmrHandle::mutex(timerHandle));
  base::timer::SaTmrHandle* handle =
      base::timer::SaTmrHandle::LookupHandle(timerHandle);
  if (handle == nullptr) return SA_AIS_ERR_BAD_HANDLE;
  base::timer::SaTmrTimer* timer = handle->Lookup(timerId);
  if (timer == nullptr) return SA_AIS_ERR_NOT_EXIST;
  handle->GetAttributes(timer, timerAttributes);
  return SA_AIS_OK;
}

static SaAisErrorT osafTmrTimeGet(SaTmrHandleT timerHandle,
                                  SaTimeT* currentTime) {
  if (currentTime == nullptr) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  *currentTime = base::timer::SaTmrHandle::GetTime();
  return SA_AIS_OK;
}

static SaAisErrorT osafTmrClockTickGet(SaTmrHandleT timerHandle,
                                       SaTimeT* clockTick) {
  if (clockTick == nullptr) {
    return SA_AIS_ERR_INVALID_PARAM;
  }
  struct timespec ts;
  if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) {
    return SA_AIS_ERR_LIBRARY;
  }
  *clockTick = base::TimespecToNanos(ts);
  return SA_AIS_OK;
}

SaAisErrorT saTmrInitialize(SaTmrHandleT* timerHandle,
                            const SaTmrCallbacksT* timerCallbacks,
                            SaVersionT* version)
    __attribute__((weak, alias("osafTmrInitialize")));

SaAisErrorT saTmrSelectionObjectGet(SaTmrHandleT timerHandle,
                                    SaSelectionObjectT* selectionObject)
    __attribute__((weak, alias("osafTmrSelectionObjectGet")));

SaAisErrorT saTmrDispatch(SaTmrHandleT timerHandle,
                          SaDispatchFlagsT dispatchFlags)
    __attribute__((weak, alias("osafTmrDispatch")));

SaAisErrorT saTmrFinalize(SaTmrHandleT timerHandle)
    __attribute__((weak, alias("osafTmrFinalize")));

SaAisErrorT saTmrTimerStart(SaTmrHandleT timerHandle,
                            const SaTmrTimerAttributesT* timerAttributes,
                            const void* timerData, SaTmrTimerIdT* timerId,
                            SaTimeT* callTime)
    __attribute__((weak, alias("osafTmrTimerStart")));

SaAisErrorT saTmrTimerReschedule(SaTmrHandleT timerHandle,
                                 SaTmrTimerIdT timerId,
                                 const SaTmrTimerAttributesT* timerAttributes,
                                 SaTimeT* callTime)
    __attribute__((weak, alias("osafTmrTimerReschedule")));

SaAisErrorT saTmrTimerCancel(SaTmrHandleT timerHandle, SaTmrTimerIdT timerId,
                             void** timerDataP)
    __attribute__((weak, alias("osafTmrTimerCancel")));

SaAisErrorT saTmrPeriodicTimerSkip(SaTmrHandleT timerHandle,
                                   SaTmrTimerIdT timerId)
    __attribute__((weak, alias("osafTmrPeriodicTimerSkip")));

SaAisErrorT saTmrTimerRemainingTimeGet(SaTmrHandleT timerHandle,
                                       SaTmrTimerIdT timerId,
                                       SaTimeT* remainingTime)
    __attribute__((weak, alias("osafTmrTimerRemainingTimeGet")));

SaAisErrorT saTmrTimerAttributesGet(SaTmrHandleT timerHandle,
                                    SaTmrTimerIdT timerId,
                                    SaTmrTimerAttributesT* timerAttributes)
    __attribute__((weak, alias("osafTmrTimerAttributesGet")));

SaAisErrorT saTmrTimeGet(SaTmrHandleT timerHandle, SaTimeT* currentTime)
    __attribute__((weak, alias("osafTmrTimeGet")));

SaAisErrorT saTmrClockTickGet(SaTmrHandleT timerHandle, SaTimeT* clockTick)
    __attribute__((weak, alias("osafTmrClockTickGet")));
