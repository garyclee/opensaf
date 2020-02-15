#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include "msg/apitest/msgtest.h"
#include "msg/apitest/tet_mqsv.h"
#include <saMsg.h>
#include <sys/poll.h>

static SaVersionT msg3_1 = { 'B', 3, 0 };

static const SaNameT queueName = {
  sizeof("safMq=TestQueue") - 1,
  "safMq=TestQueue"
};

static const SaMsgQueueCreationAttributesT creationAttributes = {
  0,
  { 5, 5, 5, 5 },
  0
};

static const int maxNumOfQueues(516);

static void setMaxQueues(void) {
  do {
    bool alreadySet(false);

    std::stringstream ss;
    ss << maxNumOfQueues;

    std::ifstream ifs("/proc/sys/kernel/msgmni", std::ifstream::in);

    while (!ifs.eof()) {
      std::string line;
      getline(ifs, line);

      if (line == ss.str()) {
        alreadySet = true;
        break;
      }
    }

    if (alreadySet)
      break;

    std::string command("echo ");

    command += ss.str();
    command += " > /proc/sys/kernel/msgmni";

    int rc(system(command.c_str()));
    assert(!rc);

    // restart msgnd to pick up the change
    command = "pkill -TERM msgnd";

    rc = system(command.c_str());
    assert(!rc);

    // wait for msgnd to come back
    while (true) {
      SaMsgHandleT msgHandle;
      SaAisErrorT aisrc(saMsgInitialize(&msgHandle, 0, &msg3_1));
      if (aisrc == SA_AIS_OK)
        break;
      else if (aisrc == SA_AIS_ERR_TRY_AGAIN || aisrc == SA_AIS_ERR_TIMEOUT) {
        timespec ts = { 1, 0 };
        while (nanosleep(&ts, &ts) < 0 && errno == EINTR);
      } else
        assert(false);
    }
  } while (false);
}

static SaAisErrorT openQueue(SaMsgHandleT msgHandle,
                             const SaNameT& queueName,
                             SaMsgQueueHandleT& queueHandle) {
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saMsgQueueOpen(msgHandle,
                        &queueName,
                        &creationAttributes,
                        SA_MSG_QUEUE_CREATE,
                        SA_TIME_ONE_SECOND * 20,
                        &queueHandle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

static SaAisErrorT groupCreate(SaMsgHandleT msgHandle,
                               const SaNameT& queueGroupName) {
  SaAisErrorT rc(SA_AIS_OK);
  bool got_timeout(false);

  while (true) {
    rc = saMsgQueueGroupCreate(msgHandle,
                               &queueGroupName,
                               SA_MSG_QUEUE_GROUP_BROADCAST);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      sleep(1);

      if (rc == SA_AIS_ERR_TIMEOUT)
        got_timeout = true;
    } else if (rc == SA_AIS_ERR_EXIST && got_timeout) {
        rc = SA_AIS_OK;
        break;
    } else
      break;
  }

  return rc;
}

static SaAisErrorT groupInsert(SaMsgHandleT msgHandle,
                               const SaNameT& queueGroupName,
                               const SaNameT& queueName) {
  SaAisErrorT rc(SA_AIS_OK);
  bool got_timeout(false);

  while (true) {
    rc = saMsgQueueGroupInsert(msgHandle,
                               &queueGroupName,
                               &queueName);
    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      sleep(1);

      if (rc == SA_AIS_ERR_TIMEOUT)
        got_timeout = true;
    } else if (rc == SA_AIS_ERR_EXIST && got_timeout) {
      rc = SA_AIS_OK;
      break;
    } else
      break;
  }

  return rc;
}

static SaAisErrorT groupDelete(SaMsgHandleT msgHandle,
                               const SaNameT& queueGroupName) {
  SaAisErrorT rc(SA_AIS_OK);
  bool got_timeout(false);

  while (true) {
    rc = saMsgQueueGroupDelete(msgHandle, &queueGroupName);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      sleep(1);

      if (rc == SA_AIS_ERR_TIMEOUT)
        got_timeout = true;
    } else if (rc == SA_AIS_ERR_NOT_EXIST && got_timeout) {
      // timeout could have succeeded
      rc = SA_AIS_OK;
      break;
    } else
      break;
  }

  return rc;
}

static SaAisErrorT unlinkQueue(SaMsgHandleT msgHandle,
                               const SaNameT& queueName) {
  SaAisErrorT rc(SA_AIS_OK);
  bool got_timeout(false);

  while (true) {
    rc = saMsgQueueUnlink(msgHandle, &queueName);

    if (rc == SA_AIS_OK)
      break;
    else if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      sleep(1);

      if (rc == SA_AIS_ERR_TIMEOUT)
        got_timeout = true;
    } else if (rc == SA_AIS_ERR_NOT_EXIST && got_timeout) {
      rc = SA_AIS_OK;
      break;
    } else if (rc != SA_AIS_OK) {
      break;
    }
  }

  return rc;
}

static SaAisErrorT msgFinalize(SaMsgHandleT msgHandle) {
  SaAisErrorT rc(SA_AIS_OK);

  while (true) {
    rc = saMsgFinalize(msgHandle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT)
      sleep(1);
    else
      break;
  }

  return rc;
}

static void sendRecv(SaMsgHandleT msgHandle) {
  char data[] = "a";

  SaMsgMessageT message = {
    1, 1, sizeof(data) + 1, 0, data, SA_MSG_MESSAGE_LOWEST_PRIORITY
  };

  SaMsgMessageT recvMessage = { 0 };
  SaTimeT replyTime;

  SaAisErrorT rc(saMsgMessageSendReceive(msgHandle,
                                         &queueName,
                                         &message,
                                         &recvMessage,
                                         &replyTime,
                                         SA_TIME_ONE_SECOND));
  assert(rc == SA_AIS_ERR_TIMEOUT);
}

static std::mutex m;
static std::condition_variable cv;
static bool ready(false);
static bool redo(false);

static void msgQueueOpenCallback(SaInvocationT invocation,
                                 SaMsgQueueHandleT queueHandle,
                                 SaAisErrorT error) {
  if (error == SA_AIS_ERR_TRY_AGAIN || error == SA_AIS_ERR_TIMEOUT) {
    redo = true;
    sleep(1);
  } else if (invocation != SA_AIS_OK)
    aisrc_validate(error, static_cast<SaAisErrorT>(invocation));

  {
    std::lock_guard<std::mutex> lk(m);
    ready = true;
  }

  cv.notify_one();
}

static void msgDeliveredCallback(SaInvocationT invocation, SaAisErrorT error) {
  aisrc_validate(error, static_cast<SaAisErrorT>(invocation));

  {
    std::lock_guard<std::mutex> lk(m);
    ready = true;
  }

  cv.notify_one();
}

static void getAsyncCallback(SaMsgHandleT msgHandle) {
  SaSelectionObjectT selObj(0);

  SaAisErrorT rc(saMsgSelectionObjectGet(msgHandle, &selObj));
  assert(rc == SA_AIS_OK);

  pollfd fd = { static_cast<int>(selObj), POLLIN };

  do {
    if (poll(&fd, 1, -1) < 0) {
      m_TET_MQSV_PRINTF("poll FAILED: %i\n", errno);
      break;
    }

    if (fd.revents & POLLIN) {
      rc = saMsgDispatch(msgHandle, SA_DISPATCH_ONE);
      assert(rc == SA_AIS_OK);
    }
  } while (false);
}

static void limitGet_01(void) {
  SaLimitValueT value;
  SaAisErrorT rc(saMsgLimitGet(0xdeadbeef,
                               SA_MSG_MAX_PRIORITY_AREA_SIZE_ID,
                               &value));
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void limitGet_02(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_PRIORITY_AREA_SIZE_ID, &value);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void limitGet_03(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_PRIORITY_AREA_SIZE_ID, 0);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_04(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, static_cast<SaMsgLimitIdT>(8), &value);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_05(void) {
  SaMsgHandleT msgHandle;
  SaVersionT msg1_1 = {'B', 1, 0};
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg1_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_PRIORITY_AREA_SIZE_ID, &value);
  aisrc_validate(rc, SA_AIS_ERR_VERSION);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_06(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_PRIORITY_AREA_SIZE_ID, &value);
  aisrc_validate(rc, SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_07(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_QUEUE_SIZE_ID, &value);
  aisrc_validate(rc, SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_08(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_NUM_QUEUES_ID, &value);
  aisrc_validate(rc, SA_AIS_OK);

  assert(value.uint64Value == maxNumOfQueues);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_09(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_NUM_QUEUE_GROUPS_ID, &value);
  aisrc_validate(rc, SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_10(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_NUM_QUEUES_PER_GROUP_ID, &value);
  aisrc_validate(rc, SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_11(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_MESSAGE_SIZE_ID, &value);
  aisrc_validate(rc, SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_12(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_REPLY_SIZE_ID, &value);
  aisrc_validate(rc, SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_13(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_REPLY_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(0);
  rc = openQueue(msgHandle, queueName, queueHandle);
  assert(rc == SA_AIS_OK);

  std::thread sendRecvThread(sendRecv, msgHandle);

  SaMsgSenderIdT senderId;
  SaMsgMessageT recvMessage;

  char msgData[256];
  recvMessage.data = msgData;
  recvMessage.size = sizeof(msgData);
  recvMessage.senderName = 0;

  rc = saMsgMessageGet(queueHandle,
                       &recvMessage,
                       0,
                       &senderId,
                       SA_TIME_ONE_SECOND * 5);
  assert(rc == SA_AIS_OK);

  // create data longer than max
  char *data = new char[value.uint64Value + 1];
  memset(data, 0, value.uint64Value + 1);

  SaMsgMessageT message = {
    1, 1, value.uint64Value + 1, 0, data, SA_MSG_MESSAGE_LOWEST_PRIORITY
  };

  rc = saMsgMessageReply(msgHandle, &message, &senderId, SA_TIME_ONE_SECOND);
  aisrc_validate(rc, SA_AIS_ERR_TOO_BIG);

  rc = saMsgQueueClose(queueHandle);
  assert(rc == SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  sendRecvThread.join();

  delete[] data;
}

static void limitGet_14(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_MESSAGE_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(0);
  rc = openQueue(msgHandle, queueName, queueHandle);
  assert(rc == SA_AIS_OK);

  char *data(new char[value.uint64Value]);

  SaMsgMessageT message = {
    1, 1, value.uint64Value + 1, 0, data, SA_MSG_MESSAGE_LOWEST_PRIORITY
  };

  rc = saMsgMessageSend(msgHandle, &queueName, &message, SA_TIME_ONE_SECOND);
  aisrc_validate(rc, SA_AIS_ERR_TOO_BIG);

  rc = saMsgQueueClose(queueHandle);
  assert(rc == SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  delete[] data;
}

static void limitGet_15(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_PRIORITY_AREA_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  const SaMsgQueueCreationAttributesT creationAttributesTooBig = {
    0,
    { value.uint64Value + 1, 5, 5, 5 },
    0
  };

  SaMsgQueueHandleT queueHandle;

  rc = saMsgQueueOpen(msgHandle,
                      &queueName,
                      &creationAttributesTooBig,
                      SA_MSG_QUEUE_CREATE,
                      SA_TIME_ONE_SECOND * 50,
                      &queueHandle);

  aisrc_validate(rc, SA_AIS_ERR_TOO_BIG);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_16(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_QUEUE_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  const SaMsgQueueCreationAttributesT creationAttributesTooBig = {
    0,
    { value.uint64Value,
      value.uint64Value,
      value.uint64Value,
      value.uint64Value },
    0
  };

  SaMsgQueueHandleT queueHandle;

  rc = saMsgQueueOpen(msgHandle,
                      &queueName,
                      &creationAttributesTooBig,
                      SA_MSG_QUEUE_CREATE,
                      SA_TIME_ONE_SECOND * 50,
                      &queueHandle);

  aisrc_validate(rc, SA_AIS_ERR_TOO_BIG);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_17(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_NUM_QUEUES_ID, &value);
  assert(rc == SA_AIS_OK);

  SaNameT localQueueName = { 0 };
  for (SaUint64T i(0); i <= value.uint64Value; i++) {
    sprintf(reinterpret_cast<char *>(localQueueName.value), "safMq=test_q_%llu", i);
    localQueueName.length = strlen(reinterpret_cast<const char *>(localQueueName.value));
    SaMsgQueueHandleT queueHandle;

    rc = openQueue(msgHandle, localQueueName, queueHandle);

    if (i == value.uint64Value)
      break;
    else
      assert(rc == SA_AIS_OK);
  }

  aisrc_validate(rc, SA_AIS_ERR_NO_RESOURCES);

  memset(&localQueueName, 0, sizeof(localQueueName));
  for (SaUint64T i(0); i < value.uint64Value; i++) {
    sprintf(reinterpret_cast<char *>(localQueueName.value), "safMq=test_q_%llu", i);
    localQueueName.length = strlen(reinterpret_cast<const char *>(localQueueName.value));

    rc = unlinkQueue(msgHandle, localQueueName);
    assert(rc == SA_AIS_OK);
  }

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_18(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_NUM_QUEUE_GROUPS_ID, &value);
  assert(rc == SA_AIS_OK);

  SaNameT queueGroupName = { 0 };
  for (SaUint64T i(0); i <= value.uint64Value; i++) {
    sprintf(reinterpret_cast<char *>(queueGroupName.value), "safMqg=test_q_%llu", i);
    queueGroupName.length = strlen(reinterpret_cast<const char *>(queueGroupName.value));
    rc = groupCreate(msgHandle, queueGroupName);

    if (i == value.uint64Value)
      break;
    else
      assert(rc == SA_AIS_OK);
  }

  aisrc_validate(rc, SA_AIS_ERR_NO_RESOURCES);

  queueGroupName = { 0 };
  for (SaUint64T i(0); i < value.uint64Value; i++) {
    sprintf(reinterpret_cast<char *>(queueGroupName.value), "safMqg=test_q_%llu", i);
    queueGroupName.length = strlen(reinterpret_cast<const char *>(queueGroupName.value));
    rc = groupDelete(msgHandle, queueGroupName);
    assert(rc == SA_AIS_OK);
  }

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_19(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_NUM_QUEUES_PER_GROUP_ID, &value);
  assert(rc == SA_AIS_OK);

  SaNameT queueGroupName = {
    sizeof("safMqg=test_mqg") - 1,
    "safMqg=test_mqg"
  };

  rc = groupCreate(msgHandle, queueGroupName);
  assert(rc == SA_AIS_OK);

  SaNameT localQueueName = { 0 };
  for (SaUint64T i(0); i <= value.uint64Value; i++) {
    sprintf(reinterpret_cast<char *>(localQueueName.value), "safMq=test_q_%llu", i);
    localQueueName.length = strlen(reinterpret_cast<const char *>(localQueueName.value));
    SaMsgQueueHandleT queueHandle;

    rc = openQueue(msgHandle, localQueueName, queueHandle);
    assert(rc == SA_AIS_OK);

    rc = groupInsert(msgHandle, queueGroupName, localQueueName);

    if (i == value.uint64Value)
      break;
    else
      assert(rc == SA_AIS_OK);
  }

  aisrc_validate(rc, SA_AIS_ERR_NO_RESOURCES);

  rc = groupDelete(msgHandle, queueGroupName);
  assert(rc == SA_AIS_OK);

  for (SaUint64T i(0); i < value.uint64Value; i++) {
    sprintf(reinterpret_cast<char *>(localQueueName.value), "safMq=test_q_%llu", i);
    localQueueName.length = strlen(reinterpret_cast<const char *>(localQueueName.value));

    rc = unlinkQueue(msgHandle, localQueueName);
    assert(rc == SA_AIS_OK);
  }

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_20(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_MESSAGE_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(0);
  rc = openQueue(msgHandle, queueName, queueHandle);
  assert(rc == SA_AIS_OK);

  char *data(new char[value.uint64Value]);

  SaMsgMessageT message = {
    1, 1, value.uint64Value + 1, 0, data, SA_MSG_MESSAGE_LOWEST_PRIORITY
  };

  SaMsgMessageT recvMessage = { 0 };
  SaTimeT replyTime;

  rc = saMsgMessageSendReceive(msgHandle,
                               &queueName,
                               &message,
                               &recvMessage,
                               &replyTime,
                               SA_TIME_ONE_SECOND);

  aisrc_validate(rc, SA_AIS_ERR_TOO_BIG);

  rc = saMsgQueueClose(queueHandle);
  assert(rc == SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  delete[] data;
}

static void limitGet_21(void) {
  SaMsgHandleT msgHandle;

  const SaMsgCallbacksT callbacks = {
    msgQueueOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc = saMsgInitialize(&msgHandle, &callbacks, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_PRIORITY_AREA_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  const SaMsgQueueCreationAttributesT creationAttributesTooBig = {
    0,
    { value.uint64Value + 1, 5, 5, 5 },
    0
  };

  do {
    rc = saMsgQueueOpenAsync(msgHandle,
                             SA_AIS_ERR_TOO_BIG,
                             &queueName,
                             &creationAttributesTooBig,
                             SA_MSG_QUEUE_CREATE);
    assert(rc == SA_AIS_OK);

    ready = redo = false;
    getAsyncCallback(msgHandle);

    // wait for the response
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait(lk, []{ return ready; });
    }
  } while (redo);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_22(void) {
  SaMsgHandleT msgHandle;

  const SaMsgCallbacksT callbacks = {
    0,
    0,
    msgDeliveredCallback,
    0
  };

  SaAisErrorT rc = saMsgInitialize(&msgHandle, &callbacks, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_MESSAGE_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(0);
  rc = openQueue(msgHandle, queueName, queueHandle);
  assert(rc == SA_AIS_OK);

  char *data(new char[value.uint64Value]);

  SaMsgMessageT message = {
    1, 1, value.uint64Value + 1, 0, data, SA_MSG_MESSAGE_LOWEST_PRIORITY
  };

  rc = saMsgMessageSendAsync(msgHandle,
                             SA_AIS_ERR_TOO_BIG,
                             &queueName,
                             &message,
                             SA_MSG_MESSAGE_DELIVERED_ACK);
  assert(rc == SA_AIS_OK);

  ready = false;
  getAsyncCallback(msgHandle);

  // wait for the response
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, []{ return ready; });
  }

  rc = saMsgQueueClose(queueHandle);
  assert(rc == SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  delete[] data;
}

static void limitGet_23(void) {
  SaMsgHandleT msgHandle;
  SaAisErrorT rc = saMsgInitialize(&msgHandle, 0, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_REPLY_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  SaMsgQueueHandleT queueHandle(0);
  rc = openQueue(msgHandle, queueName, queueHandle);
  assert(rc == SA_AIS_OK);

  std::thread sendRecvThread(sendRecv, msgHandle);

  SaMsgSenderIdT senderId;
  SaMsgMessageT recvMessage;

  char msgData[256];
  recvMessage.data = msgData;
  recvMessage.size = sizeof(msgData);
  recvMessage.senderName = 0;

  rc = saMsgMessageGet(queueHandle,
                       &recvMessage,
                       0,
                       &senderId,
                       SA_TIME_ONE_SECOND * 5);
  assert(rc == SA_AIS_OK);

  // create data longer than max
  char *data = new char[value.uint64Value + 1];
  memset(data, 0, value.uint64Value + 1);

  SaMsgMessageT message = {
    1, 1, value.uint64Value + 1, 0, data, SA_MSG_MESSAGE_LOWEST_PRIORITY
  };

  rc = saMsgMessageReplyAsync(msgHandle, 1, &message, &senderId, 0);
  aisrc_validate(rc, SA_AIS_ERR_TOO_BIG);

  rc = saMsgQueueClose(queueHandle);
  assert(rc == SA_AIS_OK);

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);

  sendRecvThread.join();

  delete[] data;
}

static void limitGet_24(void) {
  SaMsgHandleT msgHandle;

  const SaMsgCallbacksT callbacks = {
    msgQueueOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc = saMsgInitialize(&msgHandle, &callbacks, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_QUEUE_SIZE_ID, &value);
  assert(rc == SA_AIS_OK);

  const SaMsgQueueCreationAttributesT creationAttributesTooBig = {
    0,
    { value.uint64Value,
      value.uint64Value,
      value.uint64Value,
      value.uint64Value },
    0
  };

  rc = saMsgQueueOpenAsync(msgHandle,
                           SA_AIS_ERR_TOO_BIG,
                           &queueName,
                           &creationAttributesTooBig,
                           SA_MSG_QUEUE_CREATE);
  assert(rc == SA_AIS_OK);

  ready = false;
  getAsyncCallback(msgHandle);

  // wait for the response
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, []{ return ready; });
  }

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

static void limitGet_25(void) {
  SaMsgHandleT msgHandle;

  const SaMsgCallbacksT callbacks = {
    msgQueueOpenCallback,
    0,
    0,
    0
  };

  SaAisErrorT rc = saMsgInitialize(&msgHandle, &callbacks, &msg3_1);
  assert(rc == SA_AIS_OK);

  SaLimitValueT value;
  rc = saMsgLimitGet(msgHandle, SA_MSG_MAX_NUM_QUEUES_ID, &value);
  assert(rc == SA_AIS_OK);

  SaNameT localQueueName = { 0 };
  for (SaUint64T i(0); i <= value.uint64Value; i++) {
    sprintf(reinterpret_cast<char *>(localQueueName.value), "safMq=test_q_%llu", i);
    localQueueName.length = strlen(reinterpret_cast<const char *>(localQueueName.value));

    SaInvocationT invocation((i == value.uint64Value) ?
      SA_AIS_ERR_NO_RESOURCES : SA_AIS_OK);

    do {
      rc = saMsgQueueOpenAsync(msgHandle,
                               invocation,
                               &localQueueName,
                               &creationAttributes,
                               SA_MSG_QUEUE_CREATE);
      assert(rc == SA_AIS_OK);

      redo = ready = false;
      getAsyncCallback(msgHandle);

      // wait for the response
      {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, []{ return ready; });
      }
    } while (redo);
  }

  rc = msgFinalize(msgHandle);
  assert(rc == SA_AIS_OK);
}

__attribute__((constructor)) static void limitGet_constructor(void) {
  setMaxQueues();
  test_suite_add(29, "Limit Get");
  test_case_add(29,
                limitGet_01,
                "saMsgLimitGet returns BAD_HANDLE when msgHandle is bad");
  test_case_add(29,
                limitGet_02,
                "saMsgLimitGet with finalized handle");
  test_case_add(29,
                limitGet_03,
                "saMsgLimitGet with null value pointer");
  test_case_add(29,
                limitGet_04,
                "saMsgLimitGet with illegal limitId");
  test_case_add(29,
                limitGet_05,
                "saMsgLimitGet with version != B.03.01");
  test_case_add(29,
                limitGet_06,
                "saMsgLimitGet success for SA_MSG_MAX_PRIORITY_AREA_SIZE_ID");
  test_case_add(29,
                limitGet_07,
                "saMsgLimitGet success for SA_MSG_MAX_QUEUE_SIZE_ID");
  test_case_add(29,
                limitGet_08,
                "saMsgLimitGet success for SA_MSG_MAX_NUM_QUEUES_ID");
  test_case_add(29,
                limitGet_09,
                "saMsgLimitGet success for SA_MSG_MAX_NUM_QUEUE_GROUPS_ID");
  test_case_add(29,
                limitGet_10,
                "saMsgLimitGet success for SA_MSG_MAX_NUM_QUEUES_PER_GROUP_ID");
  test_case_add(29,
                limitGet_11,
                "saMsgLimitGet success for SA_MSG_MAX_MESSAGE_SIZE_ID");
  test_case_add(29,
                limitGet_12,
                "saMsgLimitGet success for SA_MSG_MAX_REPLY_SIZE_ID");
  test_case_add(29,
                limitGet_13,
                "message > SA_MSG_MAX_REPLY_SIZE returns TOO_BIG");
  test_case_add(29,
                limitGet_14,
                "message > SA_MSG_MAX_MESSAGE_SIZE returns TOO_BIG");
  test_case_add(29,
                limitGet_15,
                "queue creation with priority size > "
                  "SA_MSG_MAX_PRIORITY_AREA_SIZE_ID returns TOO_BIG");
  test_case_add(29,
                limitGet_16,
                "queue creation with queue size > "
                  "SA_MSG_MAX_QUEUE_SIZE_ID returns TOO_BIG");
  test_case_add(29,
                limitGet_17,
                "create more than SA_MSG_MAX_NUM_QUEUES_ID returns "
                  "NO_RESOURCES");
  test_case_add(29,
                limitGet_18,
                "create more than SA_MSG_MAX_NUM_QUEUE_GROUPS_ID returns "
                  "NO_RESOURCES");
  test_case_add(29,
                limitGet_19,
                "create more than SA_MSG_MAX_NUM_QUEUES_PER_GROUP_ID returns "
                  "NO_RESOURCES");
  test_case_add(29,
                limitGet_20,
                "SendReceive message > SA_MSG_MAX_MESSAGE_SIZE returns "
                "TOO_BIG");
  test_case_add(29,
                limitGet_21,
                "async queue creation with priority size > "
                  "SA_MSG_MAX_PRIORITY_AREA_SIZE_ID returns TOO_BIG");
  test_case_add(29,
                limitGet_22,
                "async message > SA_MSG_MAX_MESSAGE_SIZE returns TOO_BIG");
  test_case_add(29,
                limitGet_23,
                "async message > SA_MSG_MAX_REPLY_SIZE returns TOO_BIG");
  test_case_add(29,
                limitGet_24,
                "async queue creation with queue size > "
                  "SA_MSG_MAX_QUEUE_SIZE_ID returns TOO_BIG");
  test_case_add(29,
                limitGet_25,
                "async create more than SA_MSG_MAX_NUM_QUEUES_ID returns "
                  "NO_RESOURCES");
}
