/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
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

#include <saTmr.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include "base/time.h"
#include "base/osaf_poll.h"
#include "gtest/gtest.h"

class SaTmrTest : public ::testing::Test {
 protected:
  SaTmrTest() {}

  virtual ~SaTmrTest() {
    // Cleanup work that doesn't throw exceptions here.
  }

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
    expirations_ = 0;
    dispatch_flags_ = SA_DISPATCH_ALL;
  }

  virtual void TearDown() {}

  static void CallbackFunction(SaTmrTimerIdT timerId, const void* timerData,
                               SaUint64T expirationCount);
  static void FinalizingTimerExpiredCallback(SaTmrTimerIdT timerId,
                                             const void* timerData,
                                             SaUint64T expirationCount);
  static void VerifyNullCallbackCase(SaTmrHandleT handle);
  static void NinetyNineTimersCallbackFunction(SaTmrTimerIdT timerId,
                                               const void* timerData,
                                               SaUint64T expirationCount);

  static const SaTmrCallbacksT null_callback_;
  static const SaTmrCallbacksT callback_;
  static uint64_t expirations_;
  static SaTmrTimerIdT callback_timer_id_;
  static const void* callback_timer_data_;
  static SaUint64T callback_expiration_count_;
  static SaDispatchFlagsT dispatch_flags_;
};

const SaTmrCallbacksT SaTmrTest::null_callback_ = {nullptr};
const SaTmrCallbacksT SaTmrTest::callback_ = {&CallbackFunction};
uint64_t SaTmrTest::expirations_ = 0;
SaTmrTimerIdT SaTmrTest::callback_timer_id_{};
const void* SaTmrTest::callback_timer_data_ = nullptr;
SaUint64T SaTmrTest::callback_expiration_count_ = 0;
SaDispatchFlagsT SaTmrTest::dispatch_flags_ = SA_DISPATCH_BLOCKING;

void SaTmrTest::CallbackFunction(SaTmrTimerIdT timerId, const void* timerData,
                                 SaUint64T expirationCount) {
  ++expirations_;
  callback_timer_id_ = timerId;
  callback_timer_data_ = timerData;
  callback_expiration_count_ = expirationCount;
}

void SaTmrTest::NinetyNineTimersCallbackFunction(SaTmrTimerIdT timerId,
                                                 const void* timerData,
                                                 SaUint64T expirationCount) {
  CallbackFunction(timerId, timerData, expirationCount);
  EXPECT_EQ(reinterpret_cast<uint64_t>(timerData), expirations_);
}

void SaTmrTest::FinalizingTimerExpiredCallback(SaTmrTimerIdT timerId,
                                               const void* timerData,
                                               SaUint64T expirationCount) {
  SaTmrHandleT handle = reinterpret_cast<SaTmrHandleT>(timerData);
  CallbackFunction(timerId, timerData, expirationCount);
  if (expirations_ == 1) {
    const SaTmrTimerAttributesT attributes{SA_TIME_DURATION,
                                           SA_TIME_ONE_MICROSECOND, 0};
    SaTmrTimerIdT timer_id{};
    SaTimeT call_time{};
    SaAisErrorT start_result =
        saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                        &timer_id, &call_time);
    EXPECT_EQ(start_result, SA_AIS_OK);
    SaAisErrorT dispatch_result = saTmrDispatch(handle, dispatch_flags_);
    EXPECT_EQ(dispatch_result, SA_AIS_OK);
  } else {
    SaAisErrorT finalize_result = saTmrFinalize(handle);
    EXPECT_EQ(finalize_result, SA_AIS_OK);
  }
}

void SaTmrTest::VerifyNullCallbackCase(SaTmrHandleT handle) {
  SaSelectionObjectT sel_obj = 1234;
  SaAisErrorT result = saTmrSelectionObjectGet(handle, &sel_obj);
  EXPECT_EQ(result, SA_AIS_OK);
  // EXPECT_EQ(sel_obj, static_cast<SaSelectionObjectT>(-1));
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_ONE_SECOND,
                                         0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  result = saTmrTimerStart(handle, &attributes, &expirations_, &timer_id,
                           &call_time);
  EXPECT_EQ(result, SA_AIS_ERR_INIT);
  for (dispatch_flags_ = SA_DISPATCH_ONE;
       dispatch_flags_ <= SA_DISPATCH_BLOCKING;
       dispatch_flags_ = static_cast<SaDispatchFlagsT>(
           static_cast<int>(dispatch_flags_) + 1)) {
    std::thread dispatch_thread{[handle] {
      SaAisErrorT dispatch_result = saTmrDispatch(handle, dispatch_flags_);
      EXPECT_EQ(dispatch_result, SA_AIS_OK);
    }};
    if (dispatch_flags_ == SA_DISPATCH_BLOCKING) {
      // Make sure the other thread is inside saTmrDispatch
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      result = saTmrFinalize(handle);
      EXPECT_EQ(result, SA_AIS_OK);
    }
    dispatch_thread.join();
    EXPECT_EQ(expirations_, 0u);
  }
}

TEST_F(SaTmrTest, InitializeWithUnsupportedReleaseCode) {
  SaTmrHandleT handle{};
  SaVersionT version{'B', 1, 0};
  SaAisErrorT init_result = saTmrInitialize(&handle, nullptr, &version);
  EXPECT_EQ(init_result, SA_AIS_ERR_VERSION);
  EXPECT_EQ(version.releaseCode, static_cast<SaUint8T>('A'));
  EXPECT_EQ(version.majorVersion, static_cast<SaUint8T>(1));
  EXPECT_EQ(version.minorVersion, static_cast<SaUint8T>(1));
}

TEST_F(SaTmrTest, InitializeWithUnsupportedMajorVersion) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 2, 0};
  SaAisErrorT init_result = saTmrInitialize(&handle, nullptr, &version);
  EXPECT_EQ(init_result, SA_AIS_ERR_VERSION);
  EXPECT_EQ(version.releaseCode, static_cast<SaUint8T>('A'));
  EXPECT_EQ(version.majorVersion, static_cast<SaUint8T>(1));
  EXPECT_EQ(version.minorVersion, static_cast<SaUint8T>(1));
}

TEST_F(SaTmrTest, InitializeWithZeroVersion) {
  SaTmrHandleT handle{};
  SaVersionT version{0, 0, 0};
  SaAisErrorT init_result = saTmrInitialize(&handle, nullptr, &version);
  EXPECT_EQ(init_result, SA_AIS_ERR_VERSION);
  EXPECT_EQ(version.releaseCode, static_cast<SaUint8T>('A'));
  EXPECT_EQ(version.majorVersion, static_cast<SaUint8T>(1));
  EXPECT_EQ(version.minorVersion, static_cast<SaUint8T>(1));
}

TEST_F(SaTmrTest, InitializeWithoutCallbacks) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaAisErrorT init_result = saTmrInitialize(&handle, nullptr, &version);
  EXPECT_EQ(init_result, SA_AIS_OK);
  EXPECT_EQ(version.releaseCode, static_cast<SaUint8T>('A'));
  EXPECT_EQ(version.majorVersion, static_cast<SaUint8T>(1));
  EXPECT_EQ(version.minorVersion, static_cast<SaUint8T>(1));
  if (init_result == SA_AIS_OK) VerifyNullCallbackCase(handle);
}

TEST_F(SaTmrTest, InitializeWithNullCallback) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaAisErrorT init_result = saTmrInitialize(&handle, &null_callback_, &version);
  EXPECT_EQ(init_result, SA_AIS_OK);
  if (init_result == SA_AIS_OK) VerifyNullCallbackCase(handle);
}

TEST_F(SaTmrTest, FinalizeInsideTwoLevelsOfTimerExpiredCallback) {
  for (dispatch_flags_ = SA_DISPATCH_ONE;
       dispatch_flags_ <= SA_DISPATCH_BLOCKING;
       dispatch_flags_ = static_cast<SaDispatchFlagsT>(
           static_cast<int>(dispatch_flags_) + 1)) {
    expirations_ = 0;
    SaTmrHandleT handle{};
    SaVersionT version{'A', 1, 0};
    SaTmrCallbacksT callbacks{&FinalizingTimerExpiredCallback};
    SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
    ASSERT_EQ(init_result, SA_AIS_OK);
    const SaTmrTimerAttributesT attributes{SA_TIME_DURATION,
                                           SA_TIME_ONE_MICROSECOND, 0};
    SaTmrTimerIdT timer_id{};
    SaTimeT call_time{};
    SaAisErrorT start_result =
        saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                        &timer_id, &call_time);
    EXPECT_EQ(start_result, SA_AIS_OK);
    std::thread dispatch_thread{[handle] {
      SaAisErrorT dispatch_result = saTmrDispatch(handle, dispatch_flags_);
      EXPECT_EQ(dispatch_result, SA_AIS_OK);
    }};
    dispatch_thread.join();
    EXPECT_EQ(expirations_, 2u);
    EXPECT_NE(callback_timer_id_, timer_id);
    EXPECT_EQ(reinterpret_cast<SaTmrHandleT>(callback_timer_data_), handle);
    EXPECT_EQ(callback_expiration_count_, 1u);
  }
}

TEST_F(SaTmrTest, MultipleExpirationsBeforeCallback) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{
      SA_TIME_DURATION, SA_TIME_ONE_MICROSECOND, SA_TIME_ONE_MICROSECOND};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  SaSelectionObjectT sel_obj = 0;
  SaAisErrorT selection_result = saTmrSelectionObjectGet(handle, &sel_obj);
  EXPECT_EQ(selection_result, SA_AIS_OK);
  int poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 1);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  void* data_ptr = nullptr;
  poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 1);
  SaAisErrorT cancel_result = saTmrTimerCancel(handle, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_OK);
  EXPECT_EQ(data_ptr, reinterpret_cast<void*>(handle));
  poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 0);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 1u);
  EXPECT_EQ(callback_timer_id_, timer_id);
  EXPECT_EQ(reinterpret_cast<SaTmrHandleT>(callback_timer_data_), handle);
  EXPECT_GE(callback_expiration_count_, 1000u);
  EXPECT_LT(callback_expiration_count_, 10000u);
}

TEST_F(SaTmrTest, CheckForNoExpirationWithinOneMillisecond) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{
      SA_TIME_DURATION, 100 * SA_TIME_ONE_MILLISECOND, SA_TIME_ONE_MICROSECOND};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  SaSelectionObjectT sel_obj = 0;
  SaAisErrorT selection_result = saTmrSelectionObjectGet(handle, &sel_obj);
  EXPECT_EQ(selection_result, SA_AIS_OK);
  int poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 0);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  void* data_ptr = nullptr;
  poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 0);
  SaAisErrorT cancel_result = saTmrTimerCancel(handle, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_OK);
  EXPECT_EQ(data_ptr, reinterpret_cast<void*>(handle));
  poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 0);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, TimeRemainingForRelativeTimer) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{
      SA_TIME_DURATION, SA_TIME_ONE_MILLISECOND, 100 * SA_TIME_ONE_MILLISECOND};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond + base::kOneMillisecond);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_EQ(remaining, 0);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  remaining_result = saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_GE(remaining, 50 * SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(remaining, 99 * SA_TIME_ONE_MILLISECOND);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 1u);
}

TEST_F(SaTmrTest, TimeRemainingForAbsoluteTimer) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  SaTimeT current = 0;
  SaAisErrorT get_result = saTmrTimeGet(handle, &current);
  EXPECT_EQ(get_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_ABSOLUTE,
                                         current + SA_TIME_ONE_DAY, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_GE(remaining, SA_TIME_ONE_DAY - 50 * SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(remaining, SA_TIME_ONE_DAY - SA_TIME_ONE_MILLISECOND);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, TimeRemainingForRescheduledTimer) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION,
                                         100 * SA_TIME_ONE_MILLISECOND, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  const SaTmrTimerAttributesT attributes2{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                          0};
  SaTimeT call_time2{};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes2, &call_time2);
  EXPECT_EQ(reschedule_result, SA_AIS_OK);
  EXPECT_GE(call_time2, call_time + SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(call_time2, call_time + 50 * SA_TIME_ONE_MILLISECOND);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_GE(remaining, SA_TIME_ONE_HOUR - 50 * SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(remaining, SA_TIME_ONE_HOUR);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, RescheduleExpiredSingleEventTimer) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION,
                                         1 * SA_TIME_ONE_MICROSECOND, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  const SaTmrTimerAttributesT attributes2{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                          0};
  SaTimeT call_time2{};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes2, &call_time2);
  EXPECT_EQ(reschedule_result, SA_AIS_ERR_NOT_EXIST);
  EXPECT_EQ(expirations_, 0u);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 1u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, RescheduleExpiredPeriodicTimer) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{
      SA_TIME_DURATION, 1 * SA_TIME_ONE_MICROSECOND, SA_TIME_ONE_DAY};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  const SaTmrTimerAttributesT attributes2{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                          SA_TIME_ONE_HOUR};
  SaTimeT call_time2{};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes2, &call_time2);
  EXPECT_EQ(reschedule_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_GE(remaining, SA_TIME_ONE_HOUR - 50 * SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(remaining, SA_TIME_ONE_HOUR);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, ReschedulePeriodicToSingleEvent) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                         SA_TIME_ONE_HOUR};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes2{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                          0};
  SaTimeT call_time2{};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes2, &call_time2);
  EXPECT_EQ(reschedule_result, SA_AIS_ERR_INVALID_PARAM);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, RescheduleSingleEventToPeriodic) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_ONE_HOUR, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes2{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                          SA_TIME_ONE_HOUR};
  SaTimeT call_time2{};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes2, &call_time2);
  EXPECT_EQ(reschedule_result, SA_AIS_ERR_INVALID_PARAM);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, StartDurationAndAbsoluteWithZeroTimeToExpiry) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, 0, SA_TIME_ONE_HOUR};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_ERR_INVALID_PARAM);
  SaTimeT current = 0;
  SaAisErrorT get_result = saTmrTimeGet(handle, &current);
  EXPECT_EQ(get_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes2{SA_TIME_ABSOLUTE, current,
                                          SA_TIME_ONE_HOUR};
  start_result =
      saTmrTimerStart(handle, &attributes2, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_ERR_INVALID_PARAM);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, StartWithMaxTimeToExpiry) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_MAX, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_ERR_INVALID_PARAM);
  SaTimeT current = 0;
  SaAisErrorT get_result = saTmrTimeGet(handle, &current);
  EXPECT_EQ(get_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes2{
      SA_TIME_DURATION, SA_TIME_MAX - current - 50 * SA_TIME_ONE_MILLISECOND,
      0};
  start_result =
      saTmrTimerStart(handle, &attributes2, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes3{SA_TIME_ABSOLUTE, SA_TIME_MAX, 0};
  SaTmrTimerIdT timer_id2{};
  start_result =
      saTmrTimerStart(handle, &attributes3, reinterpret_cast<void*>(handle),
                      &timer_id2, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, RescheduleWithMaxTimeToExpiry) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes0{SA_TIME_DURATION, SA_TIME_ONE_MINUTE,
                                          0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes0, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_MAX, 0};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes, &call_time);
  EXPECT_EQ(reschedule_result, SA_AIS_ERR_INVALID_PARAM);
  SaTimeT current = 0;
  SaAisErrorT get_result = saTmrTimeGet(handle, &current);
  EXPECT_EQ(get_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes2{
      SA_TIME_DURATION, SA_TIME_MAX - current - 50 * SA_TIME_ONE_MILLISECOND,
      0};
  reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes2, &call_time);
  EXPECT_EQ(reschedule_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes3{SA_TIME_ABSOLUTE, SA_TIME_MAX, 0};
  reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes3, &call_time);
  EXPECT_EQ(reschedule_result, SA_AIS_OK);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, NinetyNineTimers) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&NinetyNineTimersCallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes0{SA_TIME_DURATION,
                                          99 * SA_TIME_ONE_MICROSECOND, 0};
  SaTmrTimerIdT timer_id0{};
  SaTimeT call_time0{};
  SaAisErrorT start_result0 =
      saTmrTimerStart(handle, &attributes0, reinterpret_cast<void*>(1),
                      &timer_id0, &call_time0);
  EXPECT_EQ(start_result0, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id0, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_EQ(remaining, 0);
  EXPECT_EQ(expirations_, 0u);
  SaTmrTimerIdT timer_ids[98];
  for (int i = 97; i != -1; --i) {
    const SaTmrTimerAttributesT attributes{
        SA_TIME_ABSOLUTE,
        call_time0 + 10 * SA_TIME_ONE_MILLISECOND + i * SA_TIME_ONE_MICROSECOND,
        0};
    SaTimeT call_time{};
    SaAisErrorT start_result =
        saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(i + 2),
                        timer_ids + i, &call_time);
    EXPECT_EQ(start_result, SA_AIS_OK);
  }
  base::Sleep(base::kOneMillisecond + base::kTenMilliseconds);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ALL);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 99u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, RescheduleDurationToAbsolute) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes0{SA_TIME_DURATION, SA_TIME_ONE_MINUTE,
                                          SA_TIME_ONE_MILLISECOND};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes0, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  SaTmrTimerAttributesT attr_out{};
  SaAisErrorT attributes_result =
      saTmrTimerAttributesGet(handle, timer_id, &attr_out);
  EXPECT_EQ(attributes_result, SA_AIS_OK);
  EXPECT_EQ(attr_out.type, SA_TIME_DURATION);
  EXPECT_EQ(attr_out.initialExpirationTime, SA_TIME_ONE_MINUTE);
  EXPECT_EQ(attr_out.timerPeriodDuration, SA_TIME_ONE_MILLISECOND);
  SaTimeT call_time2{};
  const SaTmrTimerAttributesT attributes{
      SA_TIME_ABSOLUTE, call_time + SA_TIME_ONE_HOUR, SA_TIME_ONE_DAY};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes, &call_time2);
  EXPECT_EQ(reschedule_result, SA_AIS_OK);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  attributes_result = saTmrTimerAttributesGet(handle, timer_id, &attr_out);
  EXPECT_EQ(attributes_result, SA_AIS_OK);
  EXPECT_EQ(attr_out.type, SA_TIME_ABSOLUTE);
  EXPECT_EQ(attr_out.initialExpirationTime, call_time + SA_TIME_ONE_HOUR);
  EXPECT_EQ(attr_out.timerPeriodDuration, SA_TIME_ONE_DAY);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, ClockTickGet) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  SaTimeT clock_tick{};
  SaAisErrorT result = saTmrClockTickGet(handle, &clock_tick);
  ASSERT_EQ(result, SA_AIS_OK);
  ASSERT_GT(clock_tick, 0);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, ClockTickGetNullPtr) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  SaAisErrorT result = saTmrClockTickGet(handle, nullptr);
  ASSERT_EQ(result, SA_AIS_ERR_INVALID_PARAM);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, CancelNullPtr) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes0{SA_TIME_DURATION, SA_TIME_ONE_MINUTE,
                                          SA_TIME_ONE_MILLISECOND};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes0, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  SaAisErrorT cancel_result = saTmrTimerCancel(handle, timer_id, nullptr);
  EXPECT_EQ(cancel_result, SA_AIS_ERR_INVALID_PARAM);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, Skip) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes0{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                          SA_TIME_ONE_MINUTE};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes0, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  SaAisErrorT skip_result = saTmrPeriodicTimerSkip(handle, timer_id);
  EXPECT_EQ(skip_result, SA_AIS_OK);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_GE(remaining, SA_TIME_ONE_HOUR + SA_TIME_ONE_MINUTE -
                           50 * SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(remaining, SA_TIME_ONE_HOUR + SA_TIME_ONE_MINUTE);
  SaTmrTimerAttributesT attr_out{};
  SaAisErrorT attributes_result =
      saTmrTimerAttributesGet(handle, timer_id, &attr_out);
  EXPECT_EQ(attributes_result, SA_AIS_OK);
  EXPECT_EQ(attr_out.type, SA_TIME_DURATION);
  EXPECT_EQ(attr_out.initialExpirationTime, SA_TIME_ONE_HOUR);
  EXPECT_EQ(attr_out.timerPeriodDuration, SA_TIME_ONE_MINUTE);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, SkipSingleEventTimer) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes0{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                          0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes0, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  SaAisErrorT skip_result = saTmrPeriodicTimerSkip(handle, timer_id);
  EXPECT_EQ(skip_result, SA_AIS_ERR_NOT_EXIST);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_GE(remaining, SA_TIME_ONE_HOUR - 50 * SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(remaining, SA_TIME_ONE_HOUR);
  SaTmrTimerAttributesT attr_out{};
  SaAisErrorT attributes_result =
      saTmrTimerAttributesGet(handle, timer_id, &attr_out);
  EXPECT_EQ(attributes_result, SA_AIS_OK);
  EXPECT_EQ(attr_out.type, SA_TIME_DURATION);
  EXPECT_EQ(attr_out.initialExpirationTime, SA_TIME_ONE_HOUR);
  EXPECT_EQ(attr_out.timerPeriodDuration, 0);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, SkipExpiredTimer) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes0{
      SA_TIME_DURATION, SA_TIME_ONE_MICROSECOND, SA_TIME_ONE_MINUTE};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes0, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  SaAisErrorT skip_result = saTmrPeriodicTimerSkip(handle, timer_id);
  EXPECT_EQ(skip_result, SA_AIS_OK);
  SaTimeT remaining = 0;
  SaAisErrorT remaining_result =
      saTmrTimerRemainingTimeGet(handle, timer_id, &remaining);
  EXPECT_EQ(remaining_result, SA_AIS_OK);
  EXPECT_GE(remaining, SA_TIME_ONE_MICROSECOND + 2 * SA_TIME_ONE_MINUTE -
                           SA_TIME_ONE_MILLISECOND -
                           50 * SA_TIME_ONE_MILLISECOND);
  EXPECT_LE(remaining, SA_TIME_ONE_MICROSECOND + 2 * SA_TIME_ONE_MINUTE -
                           SA_TIME_ONE_MILLISECOND);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
  const SaTmrTimerAttributesT attributes{
      SA_TIME_DURATION, SA_TIME_ONE_MICROSECOND, SA_TIME_ONE_DAY};
  SaAisErrorT reschedule_result =
      saTmrTimerReschedule(handle, timer_id, &attributes, &call_time);
  EXPECT_EQ(reschedule_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 1u);
  EXPECT_EQ(callback_timer_id_, timer_id);
  EXPECT_EQ(reinterpret_cast<SaTmrHandleT>(callback_timer_data_), handle);
  EXPECT_EQ(callback_expiration_count_, 2u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}

TEST_F(SaTmrTest, SelectionObjectShallNotBeReadableAfterInit) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  SaSelectionObjectT sel_obj = 0;
  SaAisErrorT selection_result = saTmrSelectionObjectGet(handle, &sel_obj);
  EXPECT_EQ(selection_result, SA_AIS_OK);
  int poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 0);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  poll_result = osaf_poll_one_fd(static_cast<int>(sel_obj), 0);
  EXPECT_EQ(poll_result, 0);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, InitializeManyHandles) {
  SaTmrHandleT handle[260];
  SaTmrTimerIdT timer_id[260][3];
  SaTmrCallbacksT callbacks{&CallbackFunction};
  for (int n = 0; n != 4; ++n) {
    for (int i = 0; i != 260; ++i) {
      SaVersionT version{'A', 1, 0};
      SaAisErrorT init_result =
          saTmrInitialize(handle + i, &callbacks, &version);
      ASSERT_EQ(init_result, SA_AIS_OK);
      for (int t = 0; t != 3; ++t) {
        const SaTmrTimerAttributesT attributes{SA_TIME_DURATION,
                                               SA_TIME_ONE_HOUR, 0};
        SaTimeT call_time{};
        SaAisErrorT start_result = saTmrTimerStart(
            handle[i], &attributes, reinterpret_cast<void*>(handle[i]),
            &timer_id[i][t], &call_time);
        EXPECT_EQ(start_result, SA_AIS_OK);
      }
    }
    for (int i = 0; i != 260; ++i) {
      SaAisErrorT finalize_result = saTmrFinalize(handle[i]);
      EXPECT_EQ(finalize_result, SA_AIS_OK);
    }
  }
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, InitializeManyTimers) {
  std::thread timer_thread[2];
  for (int t = 0; t != 2; ++t) {
    timer_thread[t] = std::thread{[t] {
      SaTmrHandleT handle;
      SaVersionT version{'A', 1, 0};
      SaTmrCallbacksT callbacks{&CallbackFunction};
      SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
      ASSERT_EQ(init_result, SA_AIS_OK);
      SaTmrTimerIdT timer_id[2500];
      const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                             0};
      for (int i = 0; i != 2500; ++i) {
        SaTimeT call_time{};
        SaAisErrorT start_result = saTmrTimerStart(
            handle, &attributes, reinterpret_cast<void*>(i + 12345 * t),
            timer_id + i, &call_time);
        EXPECT_EQ(start_result, SA_AIS_OK);
      }
      for (int i = 0; i != 2500; ++i) {
        void* arg;
        SaAisErrorT cancel_result = saTmrTimerCancel(handle, timer_id[i], &arg);
        EXPECT_EQ(cancel_result, SA_AIS_OK);
        EXPECT_EQ(arg, reinterpret_cast<void*>(i + 12345 * t));
      }
      SaAisErrorT finalize_result = saTmrFinalize(handle);
      EXPECT_EQ(finalize_result, SA_AIS_OK);
    }};
  }
  for (int t = 0; t != 2; ++t) {
    timer_thread[t].join();
  }
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, CancelTwice) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_ONE_HOUR, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  void* data_ptr = nullptr;
  SaAisErrorT cancel_result = saTmrTimerCancel(handle, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_OK);
  EXPECT_EQ(data_ptr, reinterpret_cast<void*>(handle));
  data_ptr = nullptr;
  cancel_result = saTmrTimerCancel(handle, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_ERR_NOT_EXIST);
  EXPECT_EQ(data_ptr, nullptr);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, CancelExpiredBeforeCallback) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION,
                                         SA_TIME_ONE_MICROSECOND, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  void* data_ptr = nullptr;
  SaAisErrorT cancel_result = saTmrTimerCancel(handle, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_OK);
  EXPECT_EQ(data_ptr, reinterpret_cast<void*>(handle));
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, CancelExpiredAfterCallback) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION,
                                         SA_TIME_ONE_MICROSECOND, 0};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  base::Sleep(base::kOneMillisecond);
  SaAisErrorT dispatch_result = saTmrDispatch(handle, SA_DISPATCH_ONE);
  EXPECT_EQ(dispatch_result, SA_AIS_OK);
  void* data_ptr = nullptr;
  SaAisErrorT cancel_result = saTmrTimerCancel(handle, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_ERR_NOT_EXIST);
  EXPECT_EQ(data_ptr, nullptr);
  EXPECT_EQ(callback_timer_id_, timer_id);
  EXPECT_EQ(callback_timer_data_, reinterpret_cast<void*>(handle));
  EXPECT_EQ(callback_expiration_count_, 1u);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 1u);
}

TEST_F(SaTmrTest, InvalidTimerId) {
  SaTmrHandleT handle{};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_ONE_HOUR,
                                         SA_TIME_ONE_HOUR};
  SaTimeT call_time{};
  SaTmrTimerIdT timer_id[3] = {static_cast<SaTmrTimerIdT>(0),
                               static_cast<SaTmrTimerIdT>(666),
                               static_cast<SaTmrTimerIdT>(4711)};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  SaAisErrorT result;
  void* data_ptr = nullptr;
  for (int i = 0; i != 3; ++i) {
    result = saTmrTimerReschedule(handle, timer_id[i], &attributes, &call_time);
    EXPECT_EQ(result, SA_AIS_ERR_NOT_EXIST);
    result = saTmrTimerCancel(handle, timer_id[i], &data_ptr);
    EXPECT_EQ(result, SA_AIS_ERR_NOT_EXIST);
    result = saTmrPeriodicTimerSkip(handle, timer_id[i]);
    EXPECT_EQ(result, SA_AIS_ERR_NOT_EXIST);
    SaTimeT remaining = 1234;
    result = saTmrTimerRemainingTimeGet(handle, timer_id[i], &remaining);
    EXPECT_EQ(result, SA_AIS_ERR_NOT_EXIST);
    EXPECT_EQ(remaining, 1234);
    SaTmrTimerAttributesT attr_out{SA_TIME_ABSOLUTE, SA_TIME_ONE_MILLISECOND,
                                   SA_TIME_ONE_MICROSECOND};
    result = saTmrTimerAttributesGet(handle, timer_id[i], &attr_out);
    EXPECT_EQ(result, SA_AIS_ERR_NOT_EXIST);
    EXPECT_EQ(attr_out.type, SA_TIME_ABSOLUTE);
    EXPECT_EQ(attr_out.initialExpirationTime, SA_TIME_ONE_MILLISECOND);
    EXPECT_EQ(attr_out.timerPeriodDuration, SA_TIME_ONE_MICROSECOND);
  }
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
  EXPECT_EQ(expirations_, 0u);
}

TEST_F(SaTmrTest, InvalidSaTmrHandle) {
  SaTmrHandleT handle{};
  SaTmrHandleT invalid_handle0{0};
  SaTmrHandleT invalid_handle1{1234};
  SaVersionT version{'A', 1, 0};
  SaTmrCallbacksT callbacks{&CallbackFunction};
  SaAisErrorT init_result = saTmrInitialize(&handle, &callbacks, &version);
  ASSERT_EQ(init_result, SA_AIS_OK);
  const SaTmrTimerAttributesT attributes{SA_TIME_DURATION, SA_TIME_ONE_SECOND,
                                         SA_TIME_ONE_SECOND};
  SaTmrTimerIdT timer_id{};
  SaTimeT call_time{};
  SaAisErrorT start_result =
      saTmrTimerStart(invalid_handle0, &attributes,
                      reinterpret_cast<void*>(handle), &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_ERR_BAD_HANDLE);
  start_result =
      saTmrTimerStart(invalid_handle1, &attributes,
                      reinterpret_cast<void*>(handle), &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_ERR_BAD_HANDLE);
  start_result =
      saTmrTimerStart(handle, &attributes, reinterpret_cast<void*>(handle),
                      &timer_id, &call_time);
  EXPECT_EQ(start_result, SA_AIS_OK);
  void* data_ptr = nullptr;
  SaAisErrorT cancel_result =
      saTmrTimerCancel(invalid_handle0, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_ERR_BAD_HANDLE);
  cancel_result = saTmrTimerCancel(invalid_handle1, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_ERR_BAD_HANDLE);
  cancel_result = saTmrTimerCancel(handle, timer_id, &data_ptr);
  EXPECT_EQ(cancel_result, SA_AIS_OK);
  SaAisErrorT finalize_result = saTmrFinalize(handle);
  EXPECT_EQ(finalize_result, SA_AIS_OK);
}
