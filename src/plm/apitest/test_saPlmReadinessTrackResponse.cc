#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>
#include <sys/poll.h>
#include <saPlm.h>
#include <unistd.h>
#include "plmtest.h"

static SaVersionT plm1_2 = {'A', 1, 2};

struct AsyncTest {
  SaPlmEntityGroupHandleT entityGroupHandle;
  SaUint64T               trackCookie;
  SaInvocationT           invocation;
  SaPlmTrackCauseT        cause;
  SaPlmChangeStepT        step;
  SaAisErrorT             error;
};

static struct AsyncTest asyncTest;

static void heAdmin(bool lock, const SaNameT& he) {
  std::string command("immadm -t 300 -o ");

  if (lock)
    command += '1';
  else
    command += '2';

  command += ' ';
  command += reinterpret_cast<const char *>(he.value);

  int rc(system(command.c_str()));

  int status(WEXITSTATUS(rc));

  if (status != 0) {
    std::cerr << "unable to " << (lock ? "lock" : "unlock") << " " << he.value
              << std::endl;
    exit(EXIT_FAILURE);
  }

  sleep(1);
}

static void readinessTrackCallback(
    SaPlmEntityGroupHandleT entityGroupHandle,
    SaUint64T trackCookie,
    SaInvocationT invocation,
    SaPlmTrackCauseT cause,
    const SaNameT *rootCauseEntity,
    SaNtfIdentifierT rootCorrelationId,
    const SaPlmReadinessTrackedEntitiesT *trackedEntities,
    SaPlmChangeStepT step,
    SaAisErrorT error)
{
  if (error != SA_AIS_OK) {
    std::cout << "readinessTrackCallback gives error: " << error << std::endl;
  } else {
    asyncTest.entityGroupHandle = entityGroupHandle;
    asyncTest.trackCookie       = trackCookie;
    asyncTest.invocation        = invocation;
    asyncTest.cause             = cause;
    asyncTest.step              = step;
    asyncTest.error             = error;
  }
}

static void getCallback(SaPlmHandleT plmHandle) {
  SaSelectionObjectT selObj(0);

  SaAisErrorT rc(saPlmSelectionObjectGet(plmHandle, &selObj));
  assert(rc == SA_AIS_OK);

  pollfd fd = { static_cast<int>(selObj), POLLIN };

  do {
    if (poll(&fd, 1, -1) < 0) {
      std::cerr << "poll FAILED: " << errno << std::endl;
      break;
    }

    if (fd.revents & POLLIN) {
      rc = saPlmDispatch(plmHandle, SA_DISPATCH_ONE);
      assert(rc == SA_AIS_OK);
    }
  } while (false);
}

static void response_01(void) {
  SaAisErrorT rc(plmReadinessTrackResponse(0xdeadbeef,
                                           0,
                                           SA_PLM_CALLBACK_RESPONSE_OK));
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

static void response_02(void) {
  SaPlmHandleT plmHandle;
  SaAisErrorT rc(plmInitialize(&plmHandle, 0, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  rc = plmEntityGroupDelete(entityGroupHandle);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrackResponse(entityGroupHandle,
                                 0,
                                 SA_PLM_CALLBACK_RESPONSE_OK);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);
}

static void response_03(void) {
  SaPlmHandleT plmHandle;
  SaAisErrorT rc(plmInitialize(&plmHandle, 0, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrackResponse(entityGroupHandle,
                                 0,
                                 static_cast<SaPlmReadinessTrackResponseT>(0));
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);
}

static void response_04(void) {
  SaPlmHandleT plmHandle;
  SaAisErrorT rc(plmInitialize(&plmHandle, 0, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrackResponse(entityGroupHandle,
                                 0,
                                 static_cast<SaPlmReadinessTrackResponseT>(4));
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);
}

static void response_05(void) {
  SaPlmHandleT plmHandle;
  SaPlmCallbacksT callbacks = { readinessTrackCallback };

  SaAisErrorT rc(plmInitialize(&plmHandle, &callbacks, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  const SaNameT entityNames[] = { amc_slot_1_dn };

  rc = plmEntityGroupAdd(entityGroupHandle,
                         entityNames,
                         sizeof(entityNames) / sizeof(SaNameT),
                         SA_PLM_GROUP_SINGLE_ENTITY);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrack(entityGroupHandle,
                         SA_TRACK_CHANGES_ONLY | SA_TRACK_START_STEP,
                         0,
                         0);
  assert(rc == SA_AIS_OK);

  std::thread t(&heAdmin, true, amc_slot_1_dn);

  getCallback(plmHandle);

  rc = plmReadinessTrackResponse(entityGroupHandle,
                                 asyncTest.invocation,
                                 SA_PLM_CALLBACK_RESPONSE_OK);
  aisrc_validate(rc, SA_AIS_OK);

  t.join();

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);

  heAdmin(false, amc_slot_1_dn);
}

static void response_06(void) {
  SaPlmHandleT plmHandle;
  SaPlmCallbacksT callbacks = { readinessTrackCallback };

  SaAisErrorT rc(plmInitialize(&plmHandle, &callbacks, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  const SaNameT entityNames[] = { amc_slot_1_dn };

  rc = plmEntityGroupAdd(entityGroupHandle,
                         entityNames,
                         sizeof(entityNames) / sizeof(SaNameT),
                         SA_PLM_GROUP_SINGLE_ENTITY);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrack(entityGroupHandle,
                         SA_TRACK_CHANGES_ONLY | SA_TRACK_START_STEP,
                         0,
                         0);
  assert(rc == SA_AIS_OK);

  std::thread t(&heAdmin, true, amc_slot_1_dn);

  getCallback(plmHandle);

  rc = plmReadinessTrackResponse(entityGroupHandle,
                                 asyncTest.invocation - 1,
                                 SA_PLM_CALLBACK_RESPONSE_OK);
  aisrc_validate(rc, SA_AIS_ERR_INVALID_PARAM);

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);

  t.join();

  heAdmin(false, amc_slot_1_dn);
}

static void response_07(void) {
  SaPlmHandleT plmHandle;
  SaPlmCallbacksT callbacks = { readinessTrackCallback };

  SaAisErrorT rc(plmInitialize(&plmHandle, &callbacks, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  const SaNameT entityNames[] = { amc_slot_1_dn };

  rc = plmEntityGroupAdd(entityGroupHandle,
                         entityNames,
                         sizeof(entityNames) / sizeof(SaNameT),
                         SA_PLM_GROUP_SINGLE_ENTITY);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrack(entityGroupHandle,
                         SA_TRACK_CHANGES_ONLY | SA_TRACK_START_STEP,
                         0,
                         0);
  assert(rc == SA_AIS_OK);

  std::thread t(&heAdmin, true, amc_slot_1_dn);

  getCallback(plmHandle);

  rc = plmReadinessTrackResponse(entityGroupHandle - 1,
                                 asyncTest.invocation,
                                 SA_PLM_CALLBACK_RESPONSE_OK);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);

  t.join();

  heAdmin(false, amc_slot_1_dn);
}

static void response_08(void) {
  SaPlmHandleT plmHandle;
  SaPlmCallbacksT callbacks = { readinessTrackCallback };

  SaAisErrorT rc(plmInitialize(&plmHandle, &callbacks, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  const SaNameT entityNames[] = { amc_slot_1_dn };

  rc = plmEntityGroupAdd(entityGroupHandle,
                         entityNames,
                         sizeof(entityNames) / sizeof(SaNameT),
                         SA_PLM_GROUP_SINGLE_ENTITY);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrack(entityGroupHandle,
                         SA_TRACK_CHANGES_ONLY | SA_TRACK_START_STEP,
                         0,
                         0);
  assert(rc == SA_AIS_OK);

  std::thread t(&heAdmin, true, amc_slot_1_dn);

  getCallback(plmHandle);

  rc = plmReadinessTrackStop(entityGroupHandle);
  assert(rc == SA_AIS_OK);

  rc = plmEntityGroupDelete(entityGroupHandle);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrackResponse(entityGroupHandle,
                                 asyncTest.invocation,
                                 SA_PLM_CALLBACK_RESPONSE_OK);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);

  t.join();

  heAdmin(false, amc_slot_1_dn);
}

static void response_09(void) {
  SaPlmHandleT plmHandle;
  SaPlmCallbacksT callbacks = { readinessTrackCallback };

  SaAisErrorT rc(plmInitialize(&plmHandle, &callbacks, &plm1_2));
  assert(rc == SA_AIS_OK);

  SaPlmEntityGroupHandleT entityGroupHandle(0);
  rc = plmEntityGroupCreate(plmHandle, &entityGroupHandle);
  assert(rc == SA_AIS_OK);

  const SaNameT entityNames[] = { amc_slot_1_dn };

  rc = plmEntityGroupAdd(entityGroupHandle,
                         entityNames,
                         sizeof(entityNames) / sizeof(SaNameT),
                         SA_PLM_GROUP_SINGLE_ENTITY);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrack(entityGroupHandle,
                         SA_TRACK_CHANGES_ONLY | SA_TRACK_START_STEP,
                         0,
                         0);
  assert(rc == SA_AIS_OK);

  std::thread t(&heAdmin, true, amc_slot_1_dn);

  getCallback(plmHandle);

  rc = plmFinalize(plmHandle);
  assert(rc == SA_AIS_OK);

  rc = plmReadinessTrackResponse(entityGroupHandle,
                                 asyncTest.invocation,
                                 SA_PLM_CALLBACK_RESPONSE_OK);
  aisrc_validate(rc, SA_AIS_ERR_BAD_HANDLE);

  t.join();

  heAdmin(false, amc_slot_1_dn);
}


__attribute__((constructor)) static void Response_constructor(void) {
  test_suite_add(4, "saPlmReadinessTrackResponse");
  test_case_add(4,
                response_01,
                "saPlmReadinessTrackResponse returns BAD_HANDLE when entity "
                "group handle is bad");
  test_case_add(4,
                response_02,
                "saPlmReadinessTrackResponse returns BAD_HANDLE when entity "
                "group has been deleted");
  test_case_add(4,
                response_03,
                "saPlmReadinessTrackResponse returns INVALID_PARAM when "
                "response is invalid");
  test_case_add(4,
                response_04,
                "saPlmReadinessTrackResponse returns INVALID_PARAM when "
                "response is invalid");
  test_case_add(4,
                response_05,
                "saPlmReadinessTrackResponse respond to START success");
  test_case_add(4,
                response_06,
                "saPlmReadinessTrackResponse respond to START with bad "
                  "invocation");
  test_case_add(4,
                response_07,
                "saPlmReadinessTrackResponse respond to START with bad entity "
                  "group");
  test_case_add(4,
                response_08,
                "saPlmReadinessTrackResponse respond to START with deleted "
                "entity group");
  test_case_add(4,
                response_09,
                "saPlmReadinessTrackResponse respond to START with finalized "
                "handle");
}
