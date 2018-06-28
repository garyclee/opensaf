#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <unistd.h>
#include "ais/include/saImm.h"
#include "ais/include/saImmOm.h"
#include "ais/include/saLck.h"
#include "lck/apitest/lcktest.h"

static SaVersionT lck3_1 = { 'B', 3, 0 };

static SaImmAccessorHandleT accessorHandle;

static SaNameT resourceName = {
  sizeof("safLock=resource1_101") - 1,
  "safLock=resource1_101"
};

static const SaImmAttrNameT names[] = {
  const_cast<char *>("saLckResourceStrippedCount"),
  const_cast<char *>("saLckResourceNumOpeners"),
  const_cast<char *>("saLckResourceIsOrphaned"),
  0
};

static void immInit(void)
{
  SaImmHandleT immHandle;
  SaVersionT immVersion = { 'A', 2, 17 };

  SaAisErrorT rc(saImmOmInitialize(&immHandle, 0, &immVersion));
  assert(rc == SA_AIS_OK);

  rc = saImmOmAccessorInitialize(immHandle, &accessorHandle);
  assert(rc == SA_AIS_OK);
}

static const SaInvocationT ASYNC_HANDLE_1 = 1;
static const SaInvocationT ASYNC_HANDLE_2 = 2;

static SaLckResourceHandleT asyncLockResourceHandle1;
static SaLckResourceHandleT asyncLockResourceHandle2;

static void resourceOpenCallback(SaInvocationT invocation,
                                 SaLckResourceHandleT lockResourceHandle,
                                 SaAisErrorT error)
{
  assert(error == SA_AIS_OK);

  if (invocation == ASYNC_HANDLE_1)
    asyncLockResourceHandle1 = lockResourceHandle;
  else if (invocation == ASYNC_HANDLE_2)
    asyncLockResourceHandle2 = lockResourceHandle;
}

static void lockGrantCallback(SaInvocationT invocation,
                              SaLckLockStatusT lockStatus,
                              SaAisErrorT error)
{
  assert(error == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);
}

static void verifyOutput(SaUint32T strippedCount,
                         SaUint32T numOpeners,
                         bool isOrphaned)
{
  SaImmAttrValuesT_2 **attributes(0);

  SaAisErrorT rc(saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes));
  if (rc != SA_AIS_OK)
    std::cerr << "saImmOmAccessorGet_2 returned: " << rc << std::endl;
  assert(rc == SA_AIS_OK);

  int i(0);
  bool gotStripped(false), gotNumOpeners(false), gotIsOrphaned(false);

  for (SaImmAttrValuesT_2 *attr(attributes[0]); attr; attr = attributes[++i]) {
    if (!strcmp(attr->attrName, "saLckResourceStrippedCount")) {
      assert(*static_cast<SaUint32T *>(*attr->attrValues) == strippedCount);
      gotStripped = true;
    } else if (!strcmp(attr->attrName, "saLckResourceNumOpeners")) {
      assert(*static_cast<SaUint32T *>(*attr->attrValues) == numOpeners);
      gotNumOpeners = true;
    } else if (!strcmp(attr->attrName, "saLckResourceIsOrphaned")) {
      assert(*static_cast<SaUint32T *>(*attr->attrValues) == isOrphaned);
      gotIsOrphaned = true;
    }
  }

  test_validate(gotStripped && gotNumOpeners && gotIsOrphaned, true);
}

static void saLckResourceClass_01(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);
  verifyOutput(0, 1, false);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);
}

static void saLckResourceClass_02(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  verifyOutput(0, 1, false);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);
}

static void saLckResourceClass_03(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_04(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ONE);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_05(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle1;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle2;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  verifyOutput(0, 2, false);

  rc = saLckResourceClose(lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle2);
  assert(rc == SA_AIS_OK);
}

static void saLckResourceClass_06(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_2,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  verifyOutput(0, 2, false);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle2);
  assert(rc == SA_AIS_OK);
}

static void saLckResourceClass_07(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle1;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle2;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_08(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_2,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_09(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckHandleT lckHandle2(0);

  rc = saLckInitialize(&lckHandle2, 0, &lck3_1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle1;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle2;
  rc = saLckResourceOpen(lckHandle2,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_10(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckHandleT lckHandle2(0);

  rc = saLckInitialize(&lckHandle2, &callbacks, &lck3_1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle2,
                              ASYNC_HANDLE_2,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckDispatch(lckHandle2, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_11(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle1;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle2;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_12(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_2,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_13(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_14(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_15(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(asyncLockResourceHandle1,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_16(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(asyncLockResourceHandle1,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_17(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(asyncLockResourceHandle1,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_18(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(asyncLockResourceHandle1,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_19(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_20(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_21(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         SA_LCK_LOCK_ORPHAN,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(0, 0, true);

  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckLockPurge(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_22(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(asyncLockResourceHandle1,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         SA_LCK_LOCK_ORPHAN,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(0, 0, true);

  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckLockPurge(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_23(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              SA_LCK_LOCK_ORPHAN,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(0, 0, true);

  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckLockPurge(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_24(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(asyncLockResourceHandle1,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              SA_LCK_LOCK_ORPHAN,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(0, 0, true);

  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckLockPurge(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_25(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle2;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_26(void)
{
  SaLckHandleT lckHandle(0);

  SaAisErrorT rc(saLckInitialize(&lckHandle, 0, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle2;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_EX_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(2, 1, false);

  rc = saLckResourceClose(lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_27(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    0,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle2;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(lockResourceHandle,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(lockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_28(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(asyncLockResourceHandle1,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_29(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_2,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(asyncLockResourceHandle1,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(asyncLockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_30(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_2,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(asyncLockResourceHandle1,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(asyncLockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_31(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    lockGrantCallback,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  rc = saLckResourceLockAsync(asyncLockResourceHandle1,
                              1,
                              &lockId,
                              SA_LCK_PR_LOCK_MODE,
                              0,
                              1);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_32(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(asyncLockResourceHandle1);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_33(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  SaLckResourceHandleT lockResourceHandle;
  rc = saLckResourceOpen(lckHandle,
                         &resourceName,
                         SA_LCK_RESOURCE_CREATE,
                         SA_TIME_ONE_SECOND * 5,
                         &lockResourceHandle);
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_2,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  sleep(1);

  rc = saLckDispatch(lckHandle, SA_DISPATCH_ALL);
  assert(rc == SA_AIS_OK);

  SaLckLockIdT lockId;
  SaLckLockStatusT lockStatus;
  rc = saLckResourceLock(lockResourceHandle,
                         &lockId,
                         SA_LCK_PR_LOCK_MODE,
                         0,
                         1,
                         SA_TIME_ONE_SECOND * 5,
                         &lockStatus);
  assert(rc == SA_AIS_OK);
  assert(lockStatus == SA_LCK_LOCK_GRANTED);

  rc = saLckResourceClose(lockResourceHandle);
  assert(rc == SA_AIS_OK);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_OK);

  verifyOutput(1, 1, false);

  rc = saLckResourceClose(asyncLockResourceHandle2);
  assert(rc == SA_AIS_OK);

  sleep(1);

  attributes = 0;
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  assert(rc == SA_AIS_ERR_NOT_EXIST);
}

static void saLckResourceClass_34(void)
{
  SaLckHandleT lckHandle(0);

  SaLckCallbacksT callbacks = {
    resourceOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc(saLckInitialize(&lckHandle, &callbacks, &lck3_1));
  assert(rc == SA_AIS_OK);

  rc = saLckResourceOpenAsync(lckHandle,
                              ASYNC_HANDLE_1,
                              &resourceName,
                              SA_LCK_RESOURCE_CREATE);
  assert(rc == SA_AIS_OK);

  rc = saLckFinalize(lckHandle);
  assert(rc == SA_AIS_OK);

  sleep(1);

  SaImmAttrValuesT_2 **attributes(0);
  rc = saImmOmAccessorGet_2(accessorHandle, &resourceName, names, &attributes);
  aisrc_validate(rc, SA_AIS_ERR_NOT_EXIST);
}

__attribute__((constructor)) static void saLckResourceClass(void)
{
  immInit();

  test_suite_add(20, "SaLckResource Class Test Suite");
  test_case_add(20,
                saLckResourceClass_01,
                "saLckResourceNumOpeners is 1 after saLckResourceOpen");
  test_case_add(20,
                saLckResourceClass_02,
                "saLckResourceNumOpeners is 1 after saLckResourceOpenAsync");
  test_case_add(20,
                saLckResourceClass_03,
                "SaLckResource object is gone after saLckResourceClose and"
                " saLckResourceNumOpeners is 0");
  test_case_add(20,
                saLckResourceClass_04,
                "SaLckResource object is gone after saLckResourceClose and"
                " saLckResourceNumOpeners is 0 (async open)");
  test_case_add(20,
                saLckResourceClass_05,
                "saLckResourceNumOpeners is 2 after 2 saLckResourceOpen calls");
  test_case_add(20,
                saLckResourceClass_06,
                "saLckResourceNumOpeners is 2 after 2 saLckResourceOpenAsync "
                "calls");
  test_case_add(20,
                saLckResourceClass_07,
                "SaLckResource object is gone after saLckResourceNumOpeners is "
                "2 and 2 calls to saLckResourceClose");
  test_case_add(20,
                saLckResourceClass_08,
                "SaLckResource object is gone after saLckResourceNumOpeners is "
                "2 and 2 calls to saLckResourceClose (async)");
  test_case_add(20,
                saLckResourceClass_09,
                "SaLckResource object is gone after 2 different saLckInitialize"
                ", 2 saLckResourceOpen, and 2 saLckResourceClose");
  test_case_add(20,
                saLckResourceClass_10,
                "SaLckResource object is gone after 2 different saLckInitialize"
                ", 2 saLckResourceOpen, and 2 saLckResourceClose (async)");
  test_case_add(20,
                saLckResourceClass_11,
                "SaLckResource object is gone after 2 saLckResourceOpen, "
                "1 saLckResourceClose, and 1 saLckFinalize");
  test_case_add(20,
                saLckResourceClass_12,
                "SaLckResource object is gone after 2 saLckResourceOpen, "
                "1 saLckResourceClose, and 1 saLckFinalize (async)");
  test_case_add(20,
                saLckResourceClass_13,
                "SaLckResource object is gone after saLckResourceOpen, "
                "saLckResourceLock, and saLckFinalize");
  test_case_add(20,
                saLckResourceClass_14,
                "SaLckResource object is gone after saLckResourceOpen, "
                "saLckResourceLock, and saLckResourceClose");
  test_case_add(20,
                saLckResourceClass_15,
                "SaLckResource object is gone after saLckResourceOpenAsync, "
                "saLckResourceLock, and saLckFinalize");
  test_case_add(20,
                saLckResourceClass_16,
                "SaLckResource object is gone after saLckResourceOpenAsync, "
                "saLckResourceLock, and saLckResourceClose");
  test_case_add(20,
                saLckResourceClass_17,
                "SaLckResource object is gone after saLckResourceOpenAsync, "
                "saLckResourceLockAsync, and saLckFinalize");
  test_case_add(20,
                saLckResourceClass_18,
                "SaLckResource object is gone after saLckResourceOpenAsync, "
                "saLckResourceLockAsync, and saLckResourceClose");
  test_case_add(20,
                saLckResourceClass_19,
                "SaLckResource object is gone after saLckResourceOpen, "
                "saLckResourceLockAsync, and saLckFinalize");
  test_case_add(20,
                saLckResourceClass_20,
                "SaLckResource object is gone after saLckResourceOpen, "
                "saLckResourceLockAsync, and saLckResourceClose");
  test_case_add(20,
                saLckResourceClass_21,
                "saLckResourceIsOrphaned set when a resource is orphaned");
  test_case_add(20,
                saLckResourceClass_22,
                "saLckResourceIsOrphaned set when a resource is orphaned (async)");
  test_case_add(20,
                saLckResourceClass_23,
                "saLckResourceIsOrphaned set when a resource is orphaned (async #2)");
  test_case_add(20,
                saLckResourceClass_24,
                "saLckResourceIsOrphaned set when a resource is orphaned (async #3)");
  test_case_add(20,
                saLckResourceClass_25,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed");
  test_case_add(20,
                saLckResourceClass_26,
                "saLckResourceStrippedCount set to 2 when a locked resource is "
                "closed, reopened, locked, then closed again");
  test_case_add(20,
                saLckResourceClass_27,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed (async)");
  test_case_add(20,
                saLckResourceClass_28,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed (async #2)");
  test_case_add(20,
                saLckResourceClass_29,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed (async #3)");
  test_case_add(20,
                saLckResourceClass_30,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed (async #4)");
  test_case_add(20,
                saLckResourceClass_31,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed (async #5)");
  test_case_add(20,
                saLckResourceClass_32,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed (async #6)");
  test_case_add(20,
                saLckResourceClass_33,
                "saLckResourceStrippedCount set to 1 when a locked resource is "
                "closed (async #7)");
  test_case_add(20,
                saLckResourceClass_34,
                "resource is removed after OpenAsync, but saLckDispatch is "
                "never called, and then saLckFinalize is called");
  /*
   * resource is removed when finalized
   *  x) node leaves cluster
   */
}
