#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <sys/poll.h>
#include "msg/apitest/msgtest.h"
#include "msg/apitest/tet_mqsv.h"
#include <saMsg.h>
#include <saNtf.h>

static SaVersionT msg3_1 = {'B', 3, 0};

typedef std::queue<SaMsgMessageCapacityStatusT> TestQueue;
typedef std::queue<int> ResultQueue;
static TestQueue testQueue;
static ResultQueue resultQueue;
static std::mutex m;
static std::condition_variable cv;
static bool ready(false);
static bool pollDone(false);

static const SaNameT queueName = {
  sizeof("safMq=TestQueue") - 1,
  "safMq=TestQueue"
};

static const SaNameT queueGroupName = {
  sizeof("safMqg=TestQueueGroup") - 1,
  "safMqg=TestQueueGroup"
};

static const SaMsgQueueCreationAttributesT creationAttributes = {
  0,
  { 5, 5, 5, 5 },
  SA_TIME_ONE_SECOND
};

static void wait(void)
{
  timespec ts = { 1, 0 };
  while (nanosleep(&ts, &ts) < 0 && errno == EINTR);
}

static SaAisErrorT msgInitialize(SaMsgHandleT *msgHandle,
                                 int callbacks,
                                 SaVersionT *version)
{
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saMsgInitialize(msgHandle, 0, version);

    if (rc == SA_AIS_OK)
      break;
    else if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      wait();
    else
      break;
  }

  return rc;
}

SaMsgQueueHandleT openQueue(SaMsgHandleT msgHandle) {
  SaMsgQueueHandleT queueHandle;

  while (true) {
    SaAisErrorT rc(saMsgQueueOpen(msgHandle,
                                  &queueName,
                                  &creationAttributes,
                                  SA_MSG_QUEUE_CREATE,
                                  SA_TIME_ONE_SECOND * 50,
                                  &queueHandle));

    if (rc == SA_AIS_OK)
      break;
    else if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      wait();
    else {
      std::cerr << "saMsgQueueOpen failed: " << rc << std::endl;
      assert(false);
    }
  }

  return queueHandle;
}


static void ntfCallback(SaNtfSubscriptionIdT subscriptionId,
                        const SaNtfNotificationsT *n) {
  int asyncTestResult(TET_FAIL);
  int asyncTest(testQueue.front());
  testQueue.pop();

  do {
    if (n->notificationType != SA_NTF_TYPE_STATE_CHANGE) {
      break;
    }

    if (n->notification.stateChangeNotification.notificationHeader.notificationClassId->vendorId != SA_NTF_VENDOR_ID_SAF) {
      break;
    }

    if (n->notification.stateChangeNotification.notificationHeader.notificationClassId->majorId != SA_SVC_MSG) {
      break;
    }

    if (n->notification.stateChangeNotification.numStateChanges != 1) {
      break;
    }

    if (*n->notification.stateChangeNotification.sourceIndicator != SA_NTF_OBJECT_OPERATION) {
      break;
    }

    if (n->notification.stateChangeNotification.changedStates[0].stateId != SA_MSG_DEST_CAPACITY_STATUS) {
      break;
    }

    if (n->notification.stateChangeNotification.notificationHeader.notifyingObject->length != sizeof("safApp=safMsgService") - 1) {
      break;
    }

    if (memcmp(n->notification.stateChangeNotification.notificationHeader.notifyingObject->value,
               "safApp=safMsgService",
               n->notification.stateChangeNotification.notificationHeader.notifyingObject->length)) {
      break;
    }

    if (asyncTest == SA_MSG_QUEUE_CAPACITY_REACHED) {
      if (n->notification.stateChangeNotification.notificationHeader.notificationClassId->minorId != 0x65) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].newState != SA_MSG_QUEUE_CAPACITY_REACHED) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].oldStatePresent != SA_FALSE) {
        break;
      }

      asyncTestResult = TET_PASS;
    } else if (asyncTest == SA_MSG_QUEUE_CAPACITY_AVAILABLE) {
      if (n->notification.stateChangeNotification.notificationHeader.notificationClassId->minorId != 0x66) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].newState != SA_MSG_QUEUE_CAPACITY_AVAILABLE) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].oldStatePresent != SA_TRUE) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].oldState != SA_MSG_QUEUE_CAPACITY_REACHED) {
        break;
      }

      asyncTestResult = TET_PASS;
    } else if (asyncTest == SA_MSG_QUEUE_GROUP_CAPACITY_REACHED) {
      if (n->notification.stateChangeNotification.notificationHeader.notificationClassId->minorId != 0x67) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].newState != SA_MSG_QUEUE_GROUP_CAPACITY_REACHED) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].oldStatePresent != SA_FALSE) {
        break;
      }

      asyncTestResult = TET_PASS;
    } else if (asyncTest == SA_MSG_QUEUE_GROUP_CAPACITY_AVAILABLE) {
      if (n->notification.stateChangeNotification.notificationHeader.notificationClassId->minorId != 0x68) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].newState != SA_MSG_QUEUE_GROUP_CAPACITY_AVAILABLE) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].oldStatePresent != SA_TRUE) {
        break;
      }

      if (n->notification.stateChangeNotification.changedStates[0].oldState != SA_MSG_QUEUE_GROUP_CAPACITY_REACHED) {
        break;
      }

      asyncTestResult = TET_PASS;
    } else {
      assert(false);
      break;
    }
  } while (false);

  resultQueue.push(asyncTestResult);

  if (testQueue.empty()) {
    {
      std::lock_guard<std::mutex> lk(m);
      ready = true;
    }

    cv.notify_one();
  }
}

static void testNtfStateChange(SaNtfHandleT ntfHandle,
                               SaSelectionObjectT ntfSelObj) {
  pollfd fd = { static_cast<int>(ntfSelObj), POLLIN };

  do {
    if (pollDone)
      break;

    if (poll(&fd, 1, 1000) < 0) {
      m_TET_MQSV_PRINTF("poll FAILED: %i\n", errno);
      break;
    }

    if (fd.revents & POLLIN) {
      SaAisErrorT rc(saNtfDispatch(ntfHandle, SA_DISPATCH_ONE));

      if (rc != SA_AIS_OK) {
        if (rc != SA_AIS_ERR_BAD_HANDLE)
          std::cerr << "saNtfDispatch failed: " << rc << std::endl;
        break;
      }
    }
  } while (true);
}

static int testNtfInit(SaNtfHandleT& ntfHandle, SaSelectionObjectT& ntfSelObj) {
  int result(TET_PASS);

  do {
    SaNtfCallbacksT callbacks = { ntfCallback, 0 };
    SaVersionT version = { 'A', 1, 0 };
    SaNtfStateChangeNotificationFilterT stateChangeFilter;

    SaAisErrorT rc(saNtfInitialize(&ntfHandle, &callbacks, &version));
    if (rc != SA_AIS_OK) {
      result = TET_FAIL;
      break;
    }

    const int numMinorIds(4);

    rc = saNtfStateChangeNotificationFilterAllocate(
      ntfHandle, &stateChangeFilter, 1, 0, 0, numMinorIds, 0, 0);

    if (rc != SA_AIS_OK) {
      result = TET_FAIL;
      break;
    }

    *stateChangeFilter.notificationFilterHeader.eventTypes =
      SA_NTF_OBJECT_STATE_CHANGE;

    for (int i(0); i < numMinorIds; i++) {
      stateChangeFilter.notificationFilterHeader.notificationClassIds[i].vendorId =
        SA_NTF_VENDOR_ID_SAF;
      stateChangeFilter.notificationFilterHeader.notificationClassIds[i].majorId =
        SA_SVC_MSG;
      stateChangeFilter.notificationFilterHeader.notificationClassIds[i].minorId =
        i + 0x65;
    }

    SaNtfNotificationTypeFilterHandlesT notificationFilterHandles = {
      0, 0, stateChangeFilter.notificationFilterHandle, 0, 0
    };

    rc = saNtfNotificationSubscribe(&notificationFilterHandles, 1);

    if (rc != SA_AIS_OK) {
      result = TET_FAIL;
      break;
    }

    rc = saNtfNotificationFilterFree(stateChangeFilter.notificationFilterHandle);
    if (rc != SA_AIS_OK) {
      result = TET_FAIL;
      break;
    }

    rc = saNtfSelectionObjectGet(ntfHandle, &ntfSelObj);
    if (rc != SA_AIS_OK) {
      result = TET_FAIL;
      break;
    }

    pollDone = false;
  } while (false);

  return result;
}

static int testNtfCleanup(SaNtfHandleT ntfHandle) {
  int result(TET_PASS);
  SaAisErrorT rc(saNtfFinalize(ntfHandle));
  if (rc != SA_AIS_OK)
    result = TET_FAIL;

  pollDone = true;

  return result;
}

static void capacityThresholds_01(void) {
  SaMsgQueueThresholdsT thresholds;
  SaAisErrorT rc(saMsgQueueCapacityThresholdsSet(0xdeadbeef, &thresholds));
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void capacityThresholds_02(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  SaMsgQueueThresholdsT thresholds;
  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void capacityThresholds_03(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  rc = saMsgQueueClose(queueHandle);
  assert(rc == SA_AIS_OK);

  SaMsgQueueThresholdsT thresholds;
  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_04(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, 0);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_05(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 4, 5, 5, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_06(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 4, 5, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_07(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 5, 4, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_08(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 5, 5, 4 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_09(void) {
  SaMsgHandleT msgHandle;
  SaVersionT msg1_1 = {'B', 1, 0};
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg1_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 5, 5, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_VERSION);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_10(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 6, 5, 5, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_11(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 6, 5, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_12(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 5, 6, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_13(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 5, 5, 6 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_14(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 5, 5, 5, 5 }, { 5, 5, 5, 5 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_OK);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_15(void) {
  SaMsgQueueThresholdsT thresholds;
  SaAisErrorT rc(saMsgQueueCapacityThresholdsGet(0xdeadbeef, &thresholds));
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void capacityThresholds_16(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  SaMsgQueueThresholdsT thresholds;
  rc = saMsgQueueCapacityThresholdsGet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void capacityThresholds_17(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  rc = saMsgQueueClose(queueHandle);
  assert(rc == SA_AIS_OK);

  SaMsgQueueThresholdsT thresholds;
  rc = saMsgQueueCapacityThresholdsGet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_18(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  rc = saMsgQueueCapacityThresholdsGet(queueHandle, 0);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_19(void) {
  SaMsgHandleT msgHandle;
  SaVersionT msg1_1 = {'B', 1, 0};
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg1_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  SaMsgQueueThresholdsT thresholds;
  rc = saMsgQueueCapacityThresholdsGet(queueHandle, &thresholds);
  aisrc_validate(rc, SA_AIS_ERR_VERSION);

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_20(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

  const SaMsgQueueThresholdsT thresholds = {
    { 1, 2, 3, 4 }, { 1, 2, 3, 4 }
  };

  rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
  assert(rc == SA_AIS_OK);

  SaMsgQueueThresholdsT thresholdsGet;
  rc = saMsgQueueCapacityThresholdsGet(queueHandle, &thresholdsGet);
  aisrc_validate(rc, SA_AIS_OK);

  assert(!memcmp(&thresholds, &thresholdsGet, sizeof(SaMsgQueueThresholdsT)));

  rc = saMsgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void capacityThresholds_21(void) {
  int result(TET_PASS);

  do {
    SaMsgHandleT msgHandle;
    SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
    assert(rc == SA_AIS_OK);

    SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

    const SaMsgQueueThresholdsT thresholds = {
      { 3, 3, 3, 3 }, { 2, 2, 2, 2 }
    };

    rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
    assert(rc == SA_AIS_OK);

    SaNtfHandleT ntfHandle;
    SaSelectionObjectT ntfSelObj;
    result = testNtfInit(ntfHandle, ntfSelObj);
    if (result != TET_PASS)
      break;

    ready = false;
    std::thread t(&testNtfStateChange, ntfHandle, ntfSelObj);
    testQueue.push(SA_MSG_QUEUE_CAPACITY_REACHED);

    /* fill up the q */
    for (SaUint8T i(SA_MSG_MESSAGE_HIGHEST_PRIORITY);
         i <= SA_MSG_MESSAGE_LOWEST_PRIORITY;
         i++) {
      char data[] = "abcd";

      SaMsgMessageT message = {
        1, 1, sizeof(data) - 1, 0, data, i
      };

      rc = saMsgMessageSend(msgHandle, &queueName, &message, SA_TIME_ONE_SECOND);
      assert(rc == SA_AIS_OK);
    }

    // wait for the notifications
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, []{ return ready; });
    }

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    result = testNtfCleanup(ntfHandle);
    if (result != TET_PASS)
      break;

    rc = saMsgQueueUnlink(msgHandle, &queueName);
    assert(rc == SA_AIS_OK);

    rc = saMsgFinalize(msgHandle);
    assert(rc == SA_AIS_OK);

    t.join();
  } while (false);

  test_validate(result, TET_PASS);

  // let the queue close
  sleep(2);
}

static void capacityThresholds_22(void) {
  int result(TET_PASS);

  do {
    SaMsgHandleT msgHandle;
    SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
    assert(rc == SA_AIS_OK);

    SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

    const SaMsgQueueThresholdsT thresholds = {
      { 3, 3, 3, 3 }, { 2, 2, 2, 2 }
    };

    rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
    assert(rc == SA_AIS_OK);

    SaNtfHandleT ntfHandle;
    SaSelectionObjectT ntfSelObj;
    result = testNtfInit(ntfHandle, ntfSelObj);
    if (result != TET_PASS)
      break;

    ready = false;
    std::thread t(&testNtfStateChange, ntfHandle, ntfSelObj);
    testQueue.push(SA_MSG_QUEUE_CAPACITY_REACHED);

    /* fill up the q */
    for (SaUint8T i(SA_MSG_MESSAGE_HIGHEST_PRIORITY);
         i <= SA_MSG_MESSAGE_LOWEST_PRIORITY;
         i++) {
      char data[] = "abcd";

      SaMsgMessageT message = {
        1, 1, sizeof(data) - 1, 0, data, i
      };

      rc = saMsgMessageSend(msgHandle, &queueName, &message, SA_TIME_ONE_SECOND);
      assert(rc == SA_AIS_OK);
    }

    // wait for the notifications
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, []{ return ready; });
    }

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    ready = false;

    testQueue.push(SA_MSG_QUEUE_CAPACITY_AVAILABLE);

    /* read a message */
    char data[4];
    SaMsgMessageT msg;
    SaMsgSenderIdT senderId;
    memset(&msg, 0, sizeof(msg));
    msg.data = &data;
    msg.size = sizeof(data);

    rc = saMsgMessageGet(queueHandle, &msg, 0, &senderId, SA_TIME_ONE_SECOND);
    assert(rc == SA_AIS_OK);

    // wait for the notifications
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, []{ return ready; });
    }

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    result = testNtfCleanup(ntfHandle);
    if (result != TET_PASS)
      break;

    rc = saMsgQueueUnlink(msgHandle, &queueName);
    assert(rc == SA_AIS_OK);

    rc = saMsgFinalize(msgHandle);
    assert(rc == SA_AIS_OK);

    t.join();
  } while (false);

  test_validate(result, TET_PASS);

  // let the queue close
  sleep(2);
}

static void capacityThresholds_23(void) {
  int result(TET_PASS);

  do {
    SaMsgHandleT msgHandle;
    SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
    assert(rc == SA_AIS_OK);

    SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

    const SaMsgQueueThresholdsT thresholds = {
      { 3, 3, 3, 3 }, { 2, 2, 2, 2 }
    };

    rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
    assert(rc == SA_AIS_OK);

    rc = saMsgQueueGroupCreate(msgHandle,
                               &queueGroupName,
                               SA_MSG_QUEUE_GROUP_ROUND_ROBIN);
    assert(rc == SA_AIS_OK);

    rc = saMsgQueueGroupInsert(msgHandle, &queueGroupName, &queueName);
    assert(rc == SA_AIS_OK);

    SaNtfHandleT ntfHandle;
    SaSelectionObjectT ntfSelObj;
    result = testNtfInit(ntfHandle, ntfSelObj);
    if (result != TET_PASS)
      break;

    ready = false;
    std::thread t(&testNtfStateChange, ntfHandle, ntfSelObj);
    testQueue.push(SA_MSG_QUEUE_CAPACITY_REACHED);
    testQueue.push(SA_MSG_QUEUE_GROUP_CAPACITY_REACHED);

    /* fill up the q */
    for (SaUint8T i(SA_MSG_MESSAGE_HIGHEST_PRIORITY);
         i <= SA_MSG_MESSAGE_LOWEST_PRIORITY;
         i++) {
      char data[] = "abcd";

      SaMsgMessageT message = {
        1, 1, sizeof(data) - 1, 0, data, i
      };

      rc = saMsgMessageSend(msgHandle,
                            &queueGroupName,
                            &message,
                            SA_TIME_ONE_SECOND);
      assert(rc == SA_AIS_OK);
    }

    // wait for the notifications
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, []{ return ready; });
    }

    ready = false;

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    result = testNtfCleanup(ntfHandle);
    if (result != TET_PASS) {
      break;
    }

    rc = saMsgQueueGroupDelete(msgHandle, &queueGroupName);
    assert(rc == SA_AIS_OK);

    rc = saMsgQueueUnlink(msgHandle, &queueName);
    assert(rc == SA_AIS_OK);

    rc = saMsgFinalize(msgHandle);
    assert(rc == SA_AIS_OK);

    t.join();
  } while (false);

  test_validate(result, TET_PASS);

  // let the queue close
  sleep(2);
}

static void capacityThresholds_24(void) {
  int result(TET_PASS);

  do {
    SaMsgHandleT msgHandle;
    SaAisErrorT rc = msgInitialize(&msgHandle, 0, &msg3_1);
    assert(rc == SA_AIS_OK);

    SaMsgQueueHandleT queueHandle(openQueue(msgHandle));

    const SaMsgQueueThresholdsT thresholds = {
      { 3, 3, 3, 3 }, { 2, 2, 2, 2 }
    };

    rc = saMsgQueueCapacityThresholdsSet(queueHandle, &thresholds);
    assert(rc == SA_AIS_OK);

    rc = saMsgQueueGroupCreate(msgHandle,
                               &queueGroupName,
                               SA_MSG_QUEUE_GROUP_ROUND_ROBIN);
    assert(rc == SA_AIS_OK);

    rc = saMsgQueueGroupInsert(msgHandle, &queueGroupName, &queueName);
    assert(rc == SA_AIS_OK);

    SaNtfHandleT ntfHandle;
    SaSelectionObjectT ntfSelObj;
    result = testNtfInit(ntfHandle, ntfSelObj);
    if (result != TET_PASS) {
      break;
    }

    ready = false;
    std::thread t(&testNtfStateChange, ntfHandle, ntfSelObj);
    testQueue.push(SA_MSG_QUEUE_CAPACITY_REACHED);
    testQueue.push(SA_MSG_QUEUE_GROUP_CAPACITY_REACHED);

    /* fill up the q */
    for (SaUint8T i(SA_MSG_MESSAGE_HIGHEST_PRIORITY);
         i <= SA_MSG_MESSAGE_LOWEST_PRIORITY;
         i++) {
      char data[] = "abcd";

      SaMsgMessageT message = {
        1, 1, sizeof(data) - 1, 0, data, i
      };

      rc = saMsgMessageSend(msgHandle, &queueName, &message, SA_TIME_ONE_SECOND);
      assert(rc == SA_AIS_OK);
    }

    // wait for the notifications
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, []{ return ready; });
    }

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    ready = false;

    testQueue.push(SA_MSG_QUEUE_CAPACITY_AVAILABLE);
    testQueue.push(SA_MSG_QUEUE_GROUP_CAPACITY_AVAILABLE);

    /* read a message */
    char data[4];
    SaMsgMessageT msg;
    SaMsgSenderIdT senderId;
    memset(&msg, 0, sizeof(msg));
    msg.data = &data;
    msg.size = sizeof(data);

    rc = saMsgMessageGet(queueHandle, &msg, 0, &senderId, SA_TIME_ONE_SECOND);
    assert(rc == SA_AIS_OK);

    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, []{ return ready; });
    }

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    result = resultQueue.front();
    resultQueue.pop();
    if (result != TET_PASS)
      break;

    result = testNtfCleanup(ntfHandle);
    if (result != TET_PASS) {
      break;
    }

    rc = saMsgQueueGroupDelete(msgHandle, &queueGroupName);
    assert(rc == SA_AIS_OK);

    rc = saMsgQueueUnlink(msgHandle, &queueName);
    assert(rc == SA_AIS_OK);

    rc = saMsgFinalize(msgHandle);
    assert(rc == SA_AIS_OK);

    t.join();
  } while (false);

  test_validate(result, TET_PASS);

  // let the queue close
  sleep(2);
}

__attribute__((constructor)) static void capacityThresholds_constructor(void) {
  test_suite_add(27, "Capacity Thresholds Test Suite");
  test_case_add(27,
                capacityThresholds_01,
                "saMsgQueueCapacityThresholdsSet returns BAD_HANDLE when "
                "queueHandle is bad");
  test_case_add(27,
                capacityThresholds_02,
                "saMsgQueueCapacityThresholdsSet with finalized handle");
  test_case_add(27,
                capacityThresholds_03,
                "saMsgQueueCapacityThresholdsSet with closed queue");
  test_case_add(27,
                capacityThresholds_04,
                "saMsgQueueCapacityThresholdsSet with null thresholds pointer");
  test_case_add(27,
                capacityThresholds_05,
                "saMsgQueueCapacityThresholdsSet with capacityAvailable > "
                "capacityReached (1)");
  test_case_add(27,
                capacityThresholds_06,
                "saMsgQueueCapacityThresholdsSet with capacityAvailable > "
                "capacityReached (2)");
  test_case_add(27,
                capacityThresholds_07,
                "saMsgQueueCapacityThresholdsSet with capacityAvailable > "
                "capacityReached (3)");
  test_case_add(27,
                capacityThresholds_08,
                "saMsgQueueCapacityThresholdsSet with capacityAvailable > "
                "capacityReached (4)");
  test_case_add(27,
                capacityThresholds_09,
                "saMsgQueueCapacityThresholdsSet with version != B.03.01");
  test_case_add(27,
                capacityThresholds_10,
                "saMsgQueueCapacityThresholdsSet with size < capacityReached "
                "(1)");
  test_case_add(27,
                capacityThresholds_11,
                "saMsgQueueCapacityThresholdsSet with size < capacityReached "
                "(2)");
  test_case_add(27,
                capacityThresholds_12,
                "saMsgQueueCapacityThresholdsSet with size < capacityReached "
                "(3)");
  test_case_add(27,
                capacityThresholds_13,
                "saMsgQueueCapacityThresholdsSet with size < capacityReached "
                "(4)");
  test_case_add(27,
                capacityThresholds_14,
                "saMsgQueueCapacityThresholdsSet success");
  test_case_add(27,
                capacityThresholds_15,
                "saMsgQueueCapacityThresholdsGet returns BAD_HANDLE when "
                "queueHandle is bad");
  test_case_add(27,
                capacityThresholds_16,
                "saMsgQueueCapacityThresholdsGet with finalized handle");
  test_case_add(27,
                capacityThresholds_17,
                "saMsgQueueCapacityThresholdsGet with closed queue");
  test_case_add(27,
                capacityThresholds_18,
                "saMsgQueueCapacityThresholdsGet with null thresholds pointer");
  test_case_add(27,
                capacityThresholds_19,
                "saMsgQueueCapacityThresholdsGet with version != B.03.01");
  test_case_add(27,
                capacityThresholds_20,
                "saMsgQueueCapacityThresholdsGet success");
  test_case_add(27,
                capacityThresholds_21,
                "Capacity Reached Notification sent");
  test_case_add(27,
                capacityThresholds_22,
                "Capacity Available Notification sent");
  test_case_add(27,
                capacityThresholds_23,
                "Capacity Reached Notification sent (group)");
  test_case_add(27,
                capacityThresholds_24,
                "Capacity Available Notification sent (group)");
}
