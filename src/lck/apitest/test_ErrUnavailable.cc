#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <poll.h>
#include <unistd.h>
#include "ais/include/saLck.h"
#include "lck/apitest/lcktest.h"

static SaVersionT lck3_1 = { 'B', 3, 0 };

static SaNameT name = {
  sizeof("safLock=TestLockUp") - 1,
  "safLock=TestLockUp"
};

static void resourceOpenCallback(SaInvocationT invocation,
                                 SaLckResourceHandleT lockResourceHandle,
                                 SaAisErrorT error)
{
}

static void lockGrantCallback(SaInvocationT invocation,
                              SaLckLockStatusT lockStatus,
                              SaAisErrorT error)
{
}

static void resourceUnlockCallback(SaInvocationT invocation, SaAisErrorT error)
{
}

static std::string getClmNodeName(void)
{
  std::ifstream ifs("/etc/opensaf/node_name");
  std::string nodeName;

  ifs >> nodeName;

  return nodeName;
}

static void lockUnlockNode(bool lock)
{
  std::string command("immadm -o ");

  if (lock)
    command += '2';
  else
    command += '1';

  command += " safNode=";

  std::string nodeName(getClmNodeName());

  command += nodeName;
  command += ",safCluster=myClmCluster";

  int rc(system(command.c_str()));

  int status(WEXITSTATUS(rc));

  if (status != 0) {
    std::cerr << "unable to " << (lock ? "lock" : "unlock") << " local node"
              << std::endl;
    exit(EXIT_FAILURE);
  }

  sleep(1);
}

static void saErrUnavailable_01(void)
{
  lockUnlockNode(true);
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_02(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaSelectionObjectT selectionObject;
  rc = saLckSelectionObjectGet(lckHandle, &selectionObject);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_03(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckOptionsT lckOptions;
  rc = saLckOptionCheck(lckHandle, &lckOptions);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_04(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckDispatch(lckHandle, SA_DISPATCH_ONE);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_05(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckFinalize(lckHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_06(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         0,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_07(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceOpenAsync(lckHandle, 1, &name, 0);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_08(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceClose(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_09(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_10(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_EX_LOCK_MODE,
                              0,
                              1);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_11(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceUnlock(lockId, SA_TIME_ONE_SECOND);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_12(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    0,
    0,
    resourceUnlockCallback
  };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceUnlockAsync(1, lockId);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_13(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckLockPurge(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_14(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLimitValueT limitValue;
  rc = saLckLimitGet(lckHandle, SA_LCK_MAX_NUM_LOCKS_ID, &limitValue);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_15(void)
{
  lockUnlockNode(true);
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  test_validate(rc, SA_AIS_ERR_LIBRARY);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_ERR_BAD_HANDLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_16(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaSelectionObjectT selectionObject;
  rc = saLckSelectionObjectGet(lckHandle, &selectionObject);
  test_validate(rc, SA_AIS_ERR_LIBRARY);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_17(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckOptionsT lckOptions;
  rc = saLckOptionCheck(lckHandle, &lckOptions);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_18(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckDispatch(lckHandle, SA_DISPATCH_ONE);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_19(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckFinalize(lckHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_20(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         0,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_21(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceOpenAsync(lckHandle, 1, &name, 0);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_22(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceClose(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_23(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_24(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_EX_LOCK_MODE,
                              0,
                              1);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_25(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceUnlock(lockId, SA_TIME_ONE_SECOND);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_26(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    0,
    0,
    resourceUnlockCallback
  };
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckResourceUnlockAsync(1, lockId);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_27(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  rc = saLckLockPurge(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
  lockUnlockNode(false);
}

static void saErrUnavailable_28(void)
{
  lockUnlockNode(true);
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_ERR_UNAVAILABLE);
  lockUnlockNode(false);
  rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  test_validate(rc, SA_AIS_OK);
  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saErrUnavailable_29(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaSelectionObjectT selectionObject;
  rc = saLckSelectionObjectGet(lckHandle, &selectionObject);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_ERR_LIBRARY);

  rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);
  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);
}

static void saErrUnavailable_30(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckOptionsT lckOptions;
  rc = saLckOptionCheck(lckHandle, &lckOptions);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_31(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckDispatch(lckHandle, SA_DISPATCH_ONE);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_32(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckFinalize(lckHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_33(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         0,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_34(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceOpenAsync(lckHandle, 1, &name, 0);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_35(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceClose(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_36(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_37(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_EX_LOCK_MODE,
                              0,
                              1);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_38(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceUnlock(lockId, SA_TIME_ONE_SECOND);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_39(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    0,
    0,
    resourceUnlockCallback
  };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceUnlockAsync(1, lockId);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_40(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckLockPurge(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_41(void)
{
  SaLckHandleT lckHandle;
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLimitValueT limitValue;
  rc = saLckLimitGet(lckHandle, SA_LCK_MAX_NUM_LOCKS_ID, &limitValue);
  test_validate(rc, SA_AIS_ERR_UNAVAILABLE);
}

static void saErrUnavailable_42(void)
{
  lockUnlockNode(true);
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  test_validate(rc, SA_AIS_ERR_LIBRARY);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_ERR_BAD_HANDLE);
  lockUnlockNode(false);
}

static void saErrUnavailable_43(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaSelectionObjectT selectionObject;
  rc = saLckSelectionObjectGet(lckHandle, &selectionObject);
  test_validate(rc, SA_AIS_ERR_LIBRARY);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_44(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckOptionsT lckOptions;
  rc = saLckOptionCheck(lckHandle, &lckOptions);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_45(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckDispatch(lckHandle, SA_DISPATCH_ONE);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_46(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckFinalize(lckHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_47(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         0,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_48(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck1_1);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceOpenAsync(lckHandle, 1, &name, 0);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_49(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceClose(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_50(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_51(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_EX_LOCK_MODE,
                              0,
                              1);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_52(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceUnlock(lockId, SA_TIME_ONE_SECOND);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_53(void)
{
  SaLckHandleT lckHandle;
  SaLckCallbacksT callbacks = {
    0,
    0,
    0,
    resourceUnlockCallback
  };
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, &callbacks, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND,
                         &lockStatus);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckResourceUnlockAsync(1, lockId);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

static void saErrUnavailable_54(void)
{
  SaLckHandleT lckHandle;
  SaVersionT lck1_1 = { 'B', 1, 0 };
  SaAisErrorT rc = saLckInitialize(&lckHandle, 0, &lck1_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &name,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  lockUnlockNode(true);
  lockUnlockNode(false);
  rc = saLckLockPurge(lockResourceHandle);
  test_validate(rc, SA_AIS_ERR_LIBRARY);
}

__attribute__((constructor)) static void saErrUnavailable_constructor(void)
{
  test_suite_add(19, "SA_AIS_ERR_UNAVAILABLE Test Suite");
  test_case_add(19, saErrUnavailable_01, "saLckInitialize");
  test_case_add(19, saErrUnavailable_02, "saLckSelectionObjectGet");
  test_case_add(19, saErrUnavailable_03, "saLckOptionCheck");
  test_case_add(19, saErrUnavailable_04, "saLckDispatch");
  test_case_add(19, saErrUnavailable_05, "saLckFinalize");
  test_case_add(19, saErrUnavailable_06, "saLckResourceOpen");
  test_case_add(19, saErrUnavailable_07, "saLckResourceOpenAsync");
  test_case_add(19, saErrUnavailable_08, "saLckResourceClose");
  test_case_add(19, saErrUnavailable_09, "saLckResourceLock");
  test_case_add(19, saErrUnavailable_10, "saLckResourceLockAsync");
  test_case_add(19, saErrUnavailable_11, "saLckResourceUnlock");
  test_case_add(19, saErrUnavailable_12, "saLckResourceUnlockAsync");
  test_case_add(19, saErrUnavailable_13, "saLckLockPurge");
  test_case_add(19, saErrUnavailable_14, "saLckLimitGet");
  test_case_add(19, saErrUnavailable_15, "saLckInitialize B.01.01");
  test_case_add(19, saErrUnavailable_16, "saLckSelectionObjectGet B.01.01");
  test_case_add(19, saErrUnavailable_17, "saLckOptionCheck B.01.01");
  test_case_add(19, saErrUnavailable_18, "saLckDispatch B.01.01");
  test_case_add(19, saErrUnavailable_19, "saLckFinalize B.01.01");
  test_case_add(19, saErrUnavailable_20, "saLckResourceOpen B.01.01");
  test_case_add(19, saErrUnavailable_21, "saLckResourceOpenAsync B.01.01");
  test_case_add(19, saErrUnavailable_22, "saLckResourceClose B.01.01");
  test_case_add(19, saErrUnavailable_23, "saLckResourceLock B.01.01");
  test_case_add(19, saErrUnavailable_24, "saLckResourceLockAsync B.01.01");
  test_case_add(19, saErrUnavailable_25, "saLckResourceUnlock B.01.01");
  test_case_add(19, saErrUnavailable_26, "saLckResourceUnlockAsync B.01.01");
  test_case_add(19, saErrUnavailable_27, "saLckLockPurge B.01.01");
  test_case_add(19, saErrUnavailable_28, "saLckInitialize (stale)");
  test_case_add(19, saErrUnavailable_29, "saLckSelectionObjectGet (stale)");
  test_case_add(19, saErrUnavailable_30, "saLckOptionCheck (stale)");
  test_case_add(19, saErrUnavailable_31, "saLckDispatch (stale)");
  test_case_add(19, saErrUnavailable_32, "saLckFinalize (stale)");
  test_case_add(19, saErrUnavailable_33, "saLckResourceOpen (stale)");
  test_case_add(19, saErrUnavailable_34, "saLckResourceOpenAsync (stale)");
  test_case_add(19, saErrUnavailable_35, "saLckResourceClose (stale)");
  test_case_add(19, saErrUnavailable_36, "saLckResourceLock (stale)");
  test_case_add(19, saErrUnavailable_37, "saLckResourceLockAsync (stale)");
  test_case_add(19, saErrUnavailable_38, "saLckResourceUnlock (stale)");
  test_case_add(19, saErrUnavailable_39, "saLckResourceUnlockAsync (stale)");
  test_case_add(19, saErrUnavailable_40, "saLckLockPurge (stale)");
  test_case_add(19, saErrUnavailable_41, "saLckLimitGet (stale)");
  test_case_add(19, saErrUnavailable_42, "saLckInitialize B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_43, "saLckSelectionObjectGet B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_44, "saLckOptionCheck B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_45, "saLckDispatch B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_46, "saLckFinalize B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_47, "saLckResourceOpen B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_48, "saLckResourceOpenAsync B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_49, "saLckResourceClose B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_50, "saLckResourceLock B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_51, "saLckResourceLockAsync B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_52, "saLckResourceUnlock B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_53, "saLckResourceUnlockAsync B.01.01 (stale)");
  test_case_add(19, saErrUnavailable_54, "saLckLockPurge B.01.01 (stale)");

  /*
   * Test lock stripping when node leaves cluster membership
   *
   * x) waiter callbacks not called
   */
}
