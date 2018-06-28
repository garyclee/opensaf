/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2009, 2018 - All Rights Reserved.
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
 * Author(s): Emerson Network Power
 *
 */

#include "base/ncssysf_tmr.h"

#include <saAis.h>
#include <sched.h>
#include <new>
#include <unordered_set>

#include "base/condition_variable.h"
#include "base/mutex.h"
#include "base/ncssysf_def.h"
#include "base/ncssysf_tsk.h"
#include "base/osaf_poll.h"
#include "base/osaf_utility.h"
#include "timer/timer.h"

namespace base {

class NcsTimer : public timer::Timer {
 public:
  explicit NcsTimer(const timer::TimerHandle::Iterator& iter)
      : timer::Timer(iter), callback_(nullptr), argument_(nullptr) {}
  void SetCallback(TMR_CALLBACK callback, void* argument) {
    callback_ = callback;
    argument_ = argument;
  }
  TMR_CALLBACK callback() const { return callback_; }
  void* argument() const { return argument_; }

 private:
  TMR_CALLBACK callback_;
  void* argument_;
};

class NcsTmrHandle : public timer::TimerHandle {
 public:
  NcsTmrHandle()
      : timer::TimerHandle{}, mutex_{}, condition_variable_{}, all_timers_{} {}
  ~NcsTmrHandle();
  NcsTimer* AllocateTimer();
  void FreeTimer(NcsTimer* timer);
  bool Dispatch();
  void Stop(NcsTimer* timer);
  void Start(NcsTimer* timer, uint64_t expiration_time,
             uint64_t period_duration);
  SaAisErrorT Restart(NcsTimer* timer, uint64_t expiration_time,
                      uint64_t period_duration, uint64_t current_time);
  void Unlock() { mutex_.Unlock(); }
  void Lock() { mutex_.Lock(); }
  void NotifyAll() { condition_variable_.NotifyAll(); }

  Mutex mutex_;
  ConditionVariable condition_variable_;

 private:
  std::unordered_set<NcsTimer*> all_timers_;
};

NcsTmrHandle::~NcsTmrHandle() {
  for (const auto& timer : all_timers_) delete timer;
}

void NcsTmrHandle::Start(NcsTimer* timer, uint64_t expiration_time,
                         uint64_t period_duration) {
  base::Lock lock(mutex_);
  TimerHandle::Start(timer, expiration_time, period_duration);
}

SaAisErrorT NcsTmrHandle::Restart(NcsTimer* timer, uint64_t expiration_time,
                                  uint64_t period_duration,
                                  uint64_t current_time) {
  base::Lock lock(mutex_);
  return TimerHandle::Reschedule(timer, expiration_time, period_duration,
                                 current_time);
}

NcsTimer* NcsTmrHandle::AllocateTimer() {
  base::Lock lock(mutex_);
  NcsTimer* timer = new NcsTimer(end());
  all_timers_.insert(timer);
  return timer;
}

void NcsTmrHandle::Stop(NcsTimer* timer) {
  base::Lock lock(mutex_);
  TimerHandle::Stop(timer);
}

void NcsTmrHandle::FreeTimer(NcsTimer* timer) {
  base::Lock lock(mutex_);
  TimerHandle::Stop(timer);
  all_timers_.erase(timer);
  delete timer;
}

bool NcsTmrHandle::Dispatch() {
  NcsTimer* timer = static_cast<NcsTimer*>(GetNextExpiredTimerInstance());
  if (timer == nullptr) return false;
  TMR_CALLBACK callback = timer->callback();
  void* argument = timer->argument();
  mutex_.Unlock();
  callback(argument);
  mutex_.Lock();
  return true;
}

}  // namespace base

namespace {

base::NcsTmrHandle* sysf_tmr_instance = nullptr;
void* p_tsk_hdl;

void* ncs_tmr_wait(void* arg) {
  base::NcsTmrHandle* handle = static_cast<base::NcsTmrHandle*>(arg);
  handle->Lock();
  osafassert(handle->empty());
  if (!handle->EnterDispatchCall()) osaf_abort(0);
  handle->NotifyAll();
  do {
    handle->Unlock();
    osaf_poll_one_fd(handle->fd(), -1);
    handle->Lock();
    while (handle->Dispatch()) {
    }
  } while (!handle->finalizing());
  handle->LeaveDispatchCall();
  osafassert(handle->empty());
  handle->NotifyAll();
  handle->Unlock();
  return reinterpret_cast<void*>(NCSCC_RC_SUCCESS);
}

}  // namespace

bool sysfTmrCreate(void) {
  if (sysf_tmr_instance != nullptr) return false;
  base::NcsTmrHandle* handle = new (std::nothrow) base::NcsTmrHandle();
  if (handle == nullptr) osaf_abort(0);
  if (handle->fd() < 0) osaf_abort(0);
  sysf_tmr_instance = handle;

  int policy = SCHED_RR; /*root defaults */
  int max_prio = sched_get_priority_max(policy);
  int min_prio = sched_get_priority_min(policy);
  int prio_val = ((max_prio - min_prio) * 0.87);

  if (m_NCS_TASK_CREATE((NCS_OS_CB)ncs_tmr_wait, handle,
                        const_cast<char*>("OSAF_TMR"), prio_val, policy,
                        NCS_STACKSIZE_HUGE, &p_tsk_hdl) != NCSCC_RC_SUCCESS) {
    osaf_abort(0);
  }

  if (m_NCS_TASK_START(p_tsk_hdl) != NCSCC_RC_SUCCESS) {
    m_NCS_TASK_RELEASE(p_tsk_hdl);
    osaf_abort(0);
  }

  base::Lock lock(handle->mutex_);
  handle->condition_variable_.Wait<std::function<bool()>>(
      &lock, [handle] { return !handle->empty(); });

  return true;
}

bool sysfTmrDestroy(void) {
  base::NcsTmrHandle* handle = sysf_tmr_instance;
  if (handle == nullptr) return false;
  {
    base::Lock lock(handle->mutex_);
    handle->Finalize();
    osafassert(!handle->is_this_thread_inside_dispatch_call());
    handle->condition_variable_.Wait<std::function<bool()>>(
        &lock, [handle] { return handle->empty(); });
  }
  delete handle;
  sysf_tmr_instance = nullptr;
  uint32_t result = m_NCS_TASK_JOIN(p_tsk_hdl);
  osafassert(result == NCSCC_RC_SUCCESS);
  return true;
}

tmr_t ncs_tmr_alloc(const char*, uint32_t) {
  base::NcsTimer* tmr = sysf_tmr_instance->AllocateTimer();
  return static_cast<tmr_t>(tmr);
}

void ncs_tmr_free(tmr_t tmrID) {
  if (tmrID != nullptr) {
    base::NcsTimer* tmr = static_cast<base::NcsTimer*>(tmrID);
    sysf_tmr_instance->FreeTimer(tmr);
  }
}

tmr_t ncs_tmr_start(tmr_t tid,
                    int64_t tmrDelay, /* timer delay in centiseconds */
                    TMR_CALLBACK tmrCB, void* tmrUarg, const char*, uint32_t) {
#ifdef ENABLE_DEBUG
  osafassert(tmrCB != nullptr);
#endif
  if (tid == nullptr) return nullptr;
  uint64_t expiration_time =
      tmrDelay >= 0 ? (static_cast<uint64_t>(tmrDelay) * 10000000) : 0;
  uint64_t current_time = base::NcsTmrHandle::GetTime();
  expiration_time += current_time;
  expiration_time += 99999999;
  expiration_time /= 100000000;
  expiration_time *= 100000000;
  base::NcsTimer* t = static_cast<base::NcsTimer*>(tid);
  if (!sysf_tmr_instance->is_running(t)) {
    t->SetCallback(tmrCB, tmrUarg);
    sysf_tmr_instance->Start(t, expiration_time, 0);
  } else {
    t->SetCallback(tmrCB, tmrUarg);
    sysf_tmr_instance->Restart(t, expiration_time, 0, current_time);
  }
  return tid;
}

void ncs_tmr_stop(tmr_t tmrID) {
  if (tmrID != nullptr) {
    base::NcsTimer* tmr = static_cast<base::NcsTimer*>(tmrID);
    sysf_tmr_instance->Stop(tmr);
  }
}

uint32_t ncs_tmr_remaining(tmr_t tmrID, int64_t* p_tleft) {
  if (tmrID != nullptr && p_tleft != nullptr) {
    base::NcsTimer* tmr = static_cast<base::NcsTimer*>(tmrID);
    if (sysf_tmr_instance->is_running(tmr)) {
      uint64_t remaining = sysf_tmr_instance->RemainingTime(tmr);
      *p_tleft = remaining / 100000;
      return NCSCC_RC_SUCCESS;
    }
  }
  return NCSCC_RC_FAILURE;
}
