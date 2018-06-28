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

#include <sched.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "base/ncssysf_tmr.h"
#include "base/time.h"
#include "gtest/gtest.h"

// The fixture for testing c-function sysf_tmr
class SysfTmrTest : public ::testing::Test {
 protected:
  SysfTmrTest()
      : distribution_(1 * 60 * 60 * 100, 2 * 60 * 60 * 100 - 1), timers_{} {
    enable_slow_tests_ = getenv("OSAF_SLOW_UNITTESTS") != nullptr;
    // Setup work can be done here for each test.
  }

  virtual ~SysfTmrTest() {
    // Cleanup work that doesn't throw exceptions here.
  }

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
    expired_ = false;
    sysfTmrCreate();
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
    bool rc = sysfTmrDestroy();
    // mock_clock_gettime.disable_mock = false;
    ASSERT_EQ(rc, true);
  }

  // Objects declared here can be used by all tests in the test case.
  static constexpr int kMaxCounter = 100;
  static int counter;
  static std::chrono::steady_clock::time_point last_time;
  static std::chrono::steady_clock::duration times[kMaxCounter];
  static std::atomic<bool> finished;
  static std::atomic<bool> first_time_through;
  void createIntervalTimers(int64_t timeout_in_ms, int no_of_counters);
  static void TimerCallback(void *arg);
  static void IntervalTimerCallback(void *arg);
  std::mt19937 generator_;
  std::uniform_int_distribution<uint32_t> distribution_;
  tmr_t timers_[1000000];
  static bool enable_slow_tests_;
  static bool expired_;
};

int SysfTmrTest::counter{0};
std::chrono::steady_clock::time_point SysfTmrTest::last_time{};
std::chrono::steady_clock::duration SysfTmrTest::times[kMaxCounter]{};
std::atomic<bool> SysfTmrTest::finished{false};
std::atomic<bool> SysfTmrTest::first_time_through{true};

bool SysfTmrTest::enable_slow_tests_{false};
bool SysfTmrTest::expired_{false};

void SysfTmrTest::IntervalTimerCallback(void *) {
  if (counter >= kMaxCounter) {
    for (int i = 0; i < kMaxCounter; i++) {
      if (enable_slow_tests_) {
        std::cout << std::chrono::duration_cast<std::chrono::microseconds>(
                         times[i])
                         .count()
                  << "\n";
      }
    }
    finished = true;
    return;
  }

  auto elapsed = std::chrono::steady_clock::now();

  if (!first_time_through) {
    times[counter] = elapsed - last_time;
    ++counter;
  } else {
    first_time_through = false;
  }
  last_time = elapsed;
}

void SysfTmrTest::TimerCallback(void *) { expired_ = true; }

void SysfTmrTest::createIntervalTimers(int64_t timeout_in_ms,
                                       int no_of_counters) {
  for (int i = 1; i <= no_of_counters; i++) {
    tmr_t tmr_id = ncs_tmr_alloc(__FILE__, __LINE__);
    ASSERT_NE(tmr_id, nullptr);
    ncs_tmr_start(tmr_id, (timeout_in_ms * i) / 10, IntervalTimerCallback, 0,
                  __FILE__, __LINE__);
  }
}

// Tests sysf_tmr as an interval timer and measure "jitter". Timeout values are
// written to stdout and can be read by e.g. gnuplot
TEST_F(SysfTmrTest, TestIntervalTimer) {
  createIntervalTimers(enable_slow_tests_ ? 100 : 1, kMaxCounter + 2);
  while (!finished) {
    std::this_thread::yield();
  }
}

// Test creating a large number of sysf timers
TEST_F(SysfTmrTest, CreateAndDeleteManyTimers) {
  expired_ = false;
  uint32_t no_of_timers = enable_slow_tests_ ? 1000000 : 1000;
  for (uint32_t i = 0; i != no_of_timers; ++i) {
    tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
    ASSERT_NE(tmr1, TMR_T_NULL);
    int64_t timeout = distribution_(generator_);
    tmr_t tmr2 = ncs_tmr_start(tmr1, timeout, TimerCallback, nullptr, __FILE__,
                               __LINE__);
    ASSERT_NE(tmr2, TMR_T_NULL);
    timers_[i] = tmr2;
  }
  for (uint32_t i = 0;;) {
    ASSERT_NE(timers_[i], TMR_T_NULL);
    ncs_tmr_stop(timers_[i]);
    ncs_tmr_free(timers_[i]);
    timers_[i] = TMR_T_NULL;
    i = (i + 197167) % no_of_timers;
    if (i == 0) break;
  }
  for (uint32_t i = 0; i != no_of_timers; ++i) {
    ASSERT_EQ(timers_[i], TMR_T_NULL);
  }
  EXPECT_EQ(expired_, false);
}

TEST_F(SysfTmrTest, ZeroTimeout) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  tmr_t tmr2 =
      ncs_tmr_start(tmr1, 0, TimerCallback, nullptr, __FILE__, __LINE__);
  ASSERT_NE(tmr2, TMR_T_NULL);
  sched_yield();
  int64_t remaining = -1;
  uint32_t result = ncs_tmr_remaining(tmr2, &remaining);
  if (result == NCSCC_RC_SUCCESS) {
    ASSERT_LT(remaining, 1000);
    ASSERT_GE(remaining, 0);
  }
  ncs_tmr_stop(tmr2);
  ncs_tmr_free(tmr2);
}

TEST_F(SysfTmrTest, NegativeTimeout) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  int64_t current = base::TimespecToNanos(base::ReadMonotonicClock());
  tmr_t tmr2 = ncs_tmr_start(tmr1, -current - 100000, TimerCallback, nullptr,
                             __FILE__, __LINE__);
  ASSERT_NE(tmr2, TMR_T_NULL);
  int64_t remaining = -1;
  uint32_t result = ncs_tmr_remaining(tmr2, &remaining);
  if (result == NCSCC_RC_SUCCESS) {
    ASSERT_LT(remaining, 1000);
    ASSERT_GE(remaining, 0);
  }
  ncs_tmr_stop(tmr2);
  ncs_tmr_free(tmr2);
}

TEST_F(SysfTmrTest, NullPtr) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  tmr_t tmr2 = ncs_tmr_start(TMR_T_NULL, 100000, TimerCallback, nullptr,
                             __FILE__, __LINE__);
  ASSERT_EQ(tmr2, TMR_T_NULL);
  ncs_tmr_stop(TMR_T_NULL);
  ncs_tmr_free(TMR_T_NULL);
  int64_t remaining = -1;
  uint32_t result = ncs_tmr_remaining(TMR_T_NULL, &remaining);
  ASSERT_EQ(result, static_cast<uint32_t>(NCSCC_RC_FAILURE));
  result = ncs_tmr_remaining(tmr1, nullptr);
  ASSERT_EQ(result, static_cast<uint32_t>(NCSCC_RC_FAILURE));
  ncs_tmr_free(tmr1);
}

TEST_F(SysfTmrTest, StopNonRunningTimer) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  ncs_tmr_stop(tmr1);
  tmr_t tmr2 =
      ncs_tmr_start(tmr1, 100000, TimerCallback, nullptr, __FILE__, __LINE__);
  ASSERT_NE(tmr2, TMR_T_NULL);
  ncs_tmr_stop(tmr2);
  ncs_tmr_free(tmr2);
}

TEST_F(SysfTmrTest, StartRunningTimer) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  tmr_t tmr2 =
      ncs_tmr_start(tmr1, 100000, TimerCallback, nullptr, __FILE__, __LINE__);
  ASSERT_NE(tmr2, TMR_T_NULL);
  tmr_t tmr3 =
      ncs_tmr_start(tmr2, 100000, TimerCallback, nullptr, __FILE__, __LINE__);
  ASSERT_NE(tmr3, TMR_T_NULL);
  ncs_tmr_stop(tmr3);
  ncs_tmr_free(tmr3);
}

TEST_F(SysfTmrTest, FreeRunningTimer) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  tmr_t tmr2 =
      ncs_tmr_start(tmr1, 100000, TimerCallback, nullptr, __FILE__, __LINE__);
  ASSERT_NE(tmr2, TMR_T_NULL);
  ncs_tmr_free(tmr2);
}

TEST_F(SysfTmrTest, GetRemainingTimeOneHour) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  tmr_t tmr2 = ncs_tmr_start(tmr1, 60 * 60 * 100, TimerCallback, nullptr,
                             __FILE__, __LINE__);
  ASSERT_NE(tmr2, TMR_T_NULL);
  int64_t remaining = -1;
  uint32_t result = ncs_tmr_remaining(tmr2, &remaining);
  ASSERT_EQ(result, static_cast<uint32_t>(NCSCC_RC_SUCCESS));
  ASSERT_LT(remaining, 60 * 61 * 10000);
  ASSERT_GE(remaining, 60 * 60 * 10000);
  ncs_tmr_stop(tmr2);
  ncs_tmr_free(tmr2);
}

TEST_F(SysfTmrTest, GetRemainingTimeStoppedTimer) {
  tmr_t tmr1 = ncs_tmr_alloc(__FILE__, __LINE__);
  ASSERT_NE(tmr1, TMR_T_NULL);
  tmr_t tmr2 = ncs_tmr_start(tmr1, 60 * 60 * 100, TimerCallback, nullptr,
                             __FILE__, __LINE__);
  ASSERT_NE(tmr2, TMR_T_NULL);
  ncs_tmr_stop(tmr2);
  int64_t remaining = -1;
  uint32_t result = ncs_tmr_remaining(tmr2, &remaining);
  ASSERT_EQ(result, static_cast<uint32_t>(NCSCC_RC_FAILURE));
  ncs_tmr_free(tmr2);
}

TEST_F(SysfTmrTest, RestartManyTimers) {
  std::mt19937_64 generator(4711);
  size_t no_of_timers = enable_slow_tests_ ? 10000 : 10;
  size_t no_of_iterations = enable_slow_tests_ ? 10000000 : 10000;
  for (size_t i = 0; i != no_of_timers; ++i) {
    timers_[i] = ncs_tmr_alloc(__FILE__, __LINE__);
    uint64_t timeout = (generator() % 1000) + 30000;
    timers_[i] = ncs_tmr_start(timers_[i], timeout, TimerCallback, nullptr,
                               __FILE__, __LINE__);
  }
  for (size_t i = 0; i != no_of_iterations; ++i) {
    uint64_t timeout = (generator() % 1000) + 30000;
    int j = i % no_of_timers;
    ncs_tmr_stop(timers_[j]);
    timers_[j] = ncs_tmr_start(timers_[j], timeout, TimerCallback, nullptr,
                               __FILE__, __LINE__);
  }
  for (size_t i = 0; i != no_of_timers; ++i) {
    ncs_tmr_free(timers_[i]);
    timers_[i] = TMR_T_NULL;
  }
  EXPECT_FALSE(expired_);
}

TEST_F(SysfTmrTest, CreateAndImmediatelyDestroy) {}
