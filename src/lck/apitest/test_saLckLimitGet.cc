#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <unistd.h>
#include "ais/include/saLck.h"
#include "lck/apitest/lcktest.h"

static SaVersionT lck3_1 = { 'B', 3, 0 };

static bool redo = false;

static void lockGrantCallbackNoResources(SaInvocationT invocation,
                                         SaLckLockStatusT lockStatus,
                                         SaAisErrorT error)
{
  if (error == SA_AIS_ERR_TIMEOUT || error == SA_AIS_ERR_TRY_AGAIN) {
   redo = true;
   printf("lockGrantCallbackNoResources received timeout/tryagain\n");
   sleep(1);
  } else if (invocation == 1000)
    test_validate(error, SA_AIS_ERR_NO_RESOURCES);
  else {
    if (error != SA_AIS_OK)
      printf("error: %i\n", error);

    assert(error == SA_AIS_OK);
  }
}

static void lockGrantCallbackNoMore(SaInvocationT invocation,
                                    SaLckLockStatusT lockStatus,
                                    SaAisErrorT error)
{
  assert(error == SA_AIS_OK);
  if (invocation == 1000)
    test_validate(lockStatus, SA_LCK_LOCK_NO_MORE);
}

SaAisErrorT lockResourceOpen(SaLckHandleT lckHandle,
                             const SaNameT& name,
                             SaLckResourceOpenFlagsT flags,
                             SaTimeT timeout,
                             SaLckResourceHandleT& lockResourceHandle)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saLckResourceOpen(lckHandle,
                           &name,
                           flags,
                           timeout,
                           &lockResourceHandle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

SaAisErrorT lockResourceOpenAsync(SaLckHandleT lckHandle,
                                  SaInvocationT invocation,
                                  const SaNameT& name,
                                  SaLckResourceOpenFlagsT flags)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saLckResourceOpenAsync(lckHandle, invocation, &name, flags);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

SaAisErrorT lockResourceLockAsync(SaLckResourceHandleT lockResourceHandle,
                                  SaInvocationT invocation,
                                  SaLckLockIdT *lockId,
                                  SaLckLockModeT lockMode,
                                  SaLckLockFlagsT lockFlags,
                                  SaLckWaiterSignalT waiterSignal)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saLckResourceLockAsync(lockResourceHandle,
                                invocation,
                                lockId,
                                lockMode,
                                lockFlags,
                                waiterSignal);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

SaAisErrorT lockResourceLock(SaLckResourceHandleT lockResourceHandle,
                             SaLckLockIdT *lockId,
                             SaLckLockModeT lockMode,
                             SaLckLockFlagsT lockFlags,
                             SaLckWaiterSignalT waiterSignal,
                             SaTimeT timeout,
                             SaLckLockStatusT *lockStatus)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saLckResourceLock(lockResourceHandle,
                           lockId,
                           lockMode,
                           lockFlags,
                           waiterSignal,
                           timeout,
                           lockStatus);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

SaAisErrorT lockResourceUnlock(SaLckLockIdT lockId, SaTimeT timeout)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saLckResourceUnlock(lockId, timeout);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

SaAisErrorT lockResourceClose(SaLckResourceHandleT handle)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saLckResourceClose(handle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

SaAisErrorT lockPurge(SaLckResourceHandleT handle)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saLckLockPurge(handle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

static void saLckLimitGet_01(void)
{
  SaAisErrorT rc = saLckLimitGet(0, static_cast<SaLckLimitIdT>(0), 0);
  test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void saLckLimitGet_02(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckLimitGet(lckHandle, static_cast<SaLckLimitIdT>(0), 0);
  test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void saLckLimitGet_03(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT limitValue;
  rc = saLckLimitGet(lckHandle, static_cast<SaLckLimitIdT>(39874), &limitValue);
  test_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_04(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  rc = saLckLimitGet(lckHandle, SA_LCK_MAX_NUM_LOCKS_ID, 0);
  test_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_05(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  rc = saLckLimitGet(lckHandle, static_cast<SaLckLimitIdT>(7), 0);
  test_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_06(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };

  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT limitValue;
  rc = saLckLimitGet(lckHandle, SA_LCK_MAX_NUM_LOCKS_ID, &limitValue);
  test_validate(rc, SA_AIS_ERR_VERSION);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_07(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT limitValue;
  rc = saLckLimitGet(lckHandle, SA_LCK_MAX_NUM_LOCKS_ID, &limitValue);
  assert(rc == SA_AIS_OK);
  test_validate(limitValue.uint64Value, 1000);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_08(void)
{
  SaLckResourceHandleT resHandles[1000];

  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  for (int i(0); i < 1001; i++) {
    SaNameT name = {
      sizeof("safLock=TestLockUpXXXX") - 1,
      "safLock=TestLockUpXXXX"
    };

    char *temp = strstr(reinterpret_cast<char *>(name.value), "XXXX");
    sprintf(temp, "%.4i", i);

    rc = lockResourceOpen(lckHandle,
                          name,
                          SA_LCK_RESOURCE_CREATE,
                          SA_TIME_ONE_SECOND * 5,
                          resHandles[i]);

    if (i != 1000)
      assert(rc == SA_AIS_OK);
  }

  test_validate(rc, SA_AIS_ERR_NO_RESOURCES);

  for (int i(0); i < 1000; i++) {
    rc = lockResourceClose(resHandles[i]);
    assert(rc == SA_AIS_OK);
  }

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  // wait for resources to clean up
  sleep(2);
}

static void saLckLimitGet_09(void)
{
  SaLckResourceHandleT lockResourceHandle;

  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  const SaNameT name = {
    sizeof("safLock=TestLockUp") - 1,
    "safLock=TestLockUp"
  };

  rc = lockResourceOpen(lckHandle,
                        name,
                        SA_LCK_RESOURCE_CREATE,
                        SA_TIME_ONE_SECOND * 5,
                        lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId[1000];

  for (int i(0); i < 1001; i++) {
    SaLckLockStatusT lockStatus;
    rc = lockResourceLock(lockResourceHandle,
                          &lockId[i],
                          SA_LCK_PR_LOCK_MODE,
                          0,
                          1,
                          SA_TIME_ONE_SECOND * 5,
                          &lockStatus);

    if (i != 1000) {
      assert(lockStatus == SA_LCK_LOCK_GRANTED);
      assert(rc == SA_AIS_OK);
    }
  }

  test_validate(rc, SA_AIS_ERR_NO_RESOURCES);

  for (int i(0); i < 1000; i++) {
    rc = lockResourceUnlock(lockId[i], SA_TIME_ONE_SECOND * 5);
    assert(rc == SA_AIS_OK);
  }

  rc = lockResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_10(void)
{
  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallbackNoResources,
    0,
    0
  };

  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;

  const SaNameT name = {
    sizeof("safLock=TestLockUp") - 1,
    "safLock=TestLockUp"
  };

  rc = lockResourceOpen(lckHandle,
                        name,
                        SA_LCK_RESOURCE_CREATE,
                        SA_TIME_ONE_SECOND * 5,
                        lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId[1000];

  SaSelectionObjectT selObj(0);

  rc = saLckSelectionObjectGet(lckHandle, &selObj);
  assert(rc == SA_AIS_OK);

  pollfd fds[] = {
    { static_cast<int>(selObj), POLLIN, 0 }
  };

  for (int i(0); i < 1001; i++) {
    do {
      rc = saLckResourceLockAsync(lockResourceHandle,
                                  i,
                                  &lockId[i],
                                  SA_LCK_PR_LOCK_MODE,
                                  0,
                                  1);
      assert(rc == SA_AIS_OK);

      if (poll(fds, sizeof(fds) / sizeof(pollfd), -1) < 0)
        assert(false);

      assert(fds[0].revents & POLLIN);
      redo = false;
      rc = saLckDispatch(lckHandle, SA_DISPATCH_ONE);
      assert(rc == SA_AIS_OK);
    } while (redo);
  }

  for (int i(0); i < 1000; i++) {
    rc = lockResourceUnlock(lockId[i], SA_TIME_ONE_SECOND * 5);
    assert(rc == SA_AIS_OK);
  }

  rc = lockResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_11(void)
{
  SaLckResourceHandleT lockResourceHandle;

  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  const SaNameT name = {
    sizeof("safLock=TestLockUp") - 1,
    "safLock=TestLockUp"
  };

  rc = lockResourceOpen(lckHandle,
                        name,
                        SA_LCK_RESOURCE_CREATE,
                        SA_TIME_ONE_SECOND * 5,
                        lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId[1000];
  SaLckLockStatusT lockStatus;

  for (int i(0); i < 1001; i++) {
    rc = lockResourceLock(lockResourceHandle,
                          &lockId[i],
                          SA_LCK_PR_LOCK_MODE,
                          0,
                          1,
                          SA_TIME_ONE_SECOND * 5,
                          &lockStatus);

    if (i != 1000)
      assert(lockStatus == SA_LCK_LOCK_GRANTED);
    assert(rc == SA_AIS_OK);
  }

  test_validate(lockStatus, SA_LCK_LOCK_NO_MORE);

  for (int i(0); i < 1000; i++) {
    rc = lockResourceUnlock(lockId[i], SA_TIME_ONE_SECOND * 5);
    assert(rc == SA_AIS_OK);
  }

  rc = lockResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckLimitGet_12(void)
{
  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallbackNoMore,
    0,
    0
  };

  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;

  const SaNameT name = {
    sizeof("safLock=TestLockUp") - 1,
    "safLock=TestLockUp"
  };

  rc = lockResourceOpen(lckHandle,
                        name,
                        SA_LCK_RESOURCE_CREATE,
                        SA_TIME_ONE_SECOND * 5,
                        lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId[1000];

  SaSelectionObjectT selObj(0);

  rc = saLckSelectionObjectGet(lckHandle, &selObj);
  assert(rc == SA_AIS_OK);

  pollfd fds[] = {
    { static_cast<int>(selObj), POLLIN, 0 }
  };

  for (int i(0); i < 1001; i++) {
    rc = saLckResourceLockAsync(lockResourceHandle,
                                i,
                                &lockId[i],
                                SA_LCK_PR_LOCK_MODE,
                                0,
                                1);
    assert(rc == SA_AIS_OK);

    if (poll(fds, sizeof(fds) / sizeof(pollfd), -1) < 0)
      assert(false);

    assert(fds[0].revents & POLLIN);
    rc = saLckDispatch(lckHandle, SA_DISPATCH_ONE);
    assert(rc == SA_AIS_OK);
  }

  for (int i(0); i < 1000; i++) {
    rc = lockResourceUnlock(lockId[i], SA_TIME_ONE_SECOND * 5);
    assert(rc == SA_AIS_OK);
  }

  rc = lockResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

__attribute__((constructor)) static void saLckLimitGet_constructor(void)
{
  test_suite_add(18, "saLckLimitGet Test Suite");
  test_case_add(18, saLckLimitGet_01, "saLckLimitGet with null handle");
  test_case_add(18, saLckLimitGet_02, "saLckLimitGet with finalized handle");
  test_case_add(18, saLckLimitGet_03, "saLckLimitGet with invalid limitId");
  test_case_add(18, saLckLimitGet_04, "saLckLimitGet with null limitValue");
  test_case_add(18,
                saLckLimitGet_05,
                "saLckLimitGet with null limitValue and invalid limitId");
  test_case_add(18, saLckLimitGet_06, "saLckLimitGet with version B.01.01");
  test_case_add(18, saLckLimitGet_07, "saLckLimitGet returns 1000");
  test_case_add(18, saLckLimitGet_08, "create more than 1000 resources");
  test_case_add(18, saLckLimitGet_09, "create more than 1000 locks");
  test_case_add(18, saLckLimitGet_10, "create more than 1000 locks (async)");
  test_case_add(18, saLckLimitGet_11, "create more than 1000 locks (B.01.01)");
  test_case_add(18,
                saLckLimitGet_12,
                "create more than 1000 locks B.01.01 (async)");

  /*
   * Add test cases for:
   * x) HA tests
   *   x) LockResource during failover of lckd (imm safLock never gets cleaned up) and kill application
   *   x)
   */
}
