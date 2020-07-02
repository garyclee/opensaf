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
 * Author(s): Ericsson AB
 *
 */

#include "log/logd/lgs_oi_admin.h"

#include <atomic>
#include <thread>

#include "ais/include/saAis.h"
#include "ais/include/saImmOi.h"
#include "base/logtrace.h"
#include "base/mutex.h"
#include "base/saf_error.h"
#include "base/time.h"

#include "log/logd/lgs.h"
#include "log/logd/lgs_imm.h"
#include "log/logd/lgs_config.h"
#include "log/logd/lgs_util.h"

// Note: Always use the set and get function with these varibales
static base::Mutex lgs_OI_init_mutex;
static SaImmOiHandleT oi_handle_ = 0;
static SaSelectionObjectT oi_selection_object_ = -1;

static const SaImmOiImplementerNameT kImplementerName =
    const_cast<SaImmOiImplementerNameT>("safLogService");
// The log server is OI for the following configuration classes
static const SaImmClassNameT kLogConfigurationClass =
    const_cast<SaImmClassNameT>("OpenSafLogConfig");
static const SaImmClassNameT kLogStreamClass =
    const_cast<SaImmClassNameT>("SaLogStreamConfig");

// Max time for creation of OI
static const uint64_t kOiCreationTimeout = 90000;  // 1.5 minutes
// Max time for getting an OI handle
static const uint64_t kOiHandleRequestTimeout = 60000;  // 1 minute
// Max time for other APIs in the OI creation sequence
static const uint64_t kOiRequestTimeout = 10000;  // 10 sec
// Max time to wait for the thread to stop if OI create stop is requested
static const uint64_t kOiWaitStopTimeout = 11000; //  11 sec
// Delay time used in timed out wait loops
static const timespec kOiTryAgainDelay = base::kOneHundredMilliseconds;

// Internal help functions
// ---------------------------------

static void setProtectedGlobals(SaImmOiHandleT oi_handle,
                                SaSelectionObjectT oi_selction_object) {
  TRACE_ENTER();
  base::Lock protect_globals(lgs_OI_init_mutex);
  oi_handle_ = oi_handle;
  oi_selection_object_ = oi_selction_object;
}

static SaAisErrorT finalizeOi(SaImmOiHandleT oi_handle) {
  TRACE_ENTER();
  SaAisErrorT ais_rc = SA_AIS_OK;
  base::Timer try_again_timer(kOiRequestTimeout);
  while (try_again_timer.is_timeout() == false) {
    ais_rc = saImmOiFinalize(oi_handle);
    if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(kOiTryAgainDelay);
      continue;
    } else if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
      // If we get BAD HANDLE the OI is already finalized
      ais_rc = SA_AIS_OK;
      break;
    } else if (ais_rc != SA_AIS_OK) {
      TRACE("%s: saImmOiFinalize() Fail, %s", __FUNCTION__, saf_error(ais_rc));
    }
    break;
  }
  if ((try_again_timer.is_timeout() == true) &&
      (ais_rc == SA_AIS_ERR_TRY_AGAIN)) {
    LOG_NO("%s: saImmOiFinalize() Fail, TRY AGAIN timeout", __FUNCTION__);
  }

  return ais_rc;
}

// IMM API help function used in the OI creation thread
// Communication flag read in OI thread and set by the lgs_OI_delete() and
// create functions. When this flag is true the thread shall stop and return
// as soon as possible
// Note1: All of these functions will stop internal loops and return if
//        the global atomic flag oi_stop == true
// Note2: Only the function used for stopping the OI is allowed to write this
//        flag
static std::atomic<bool> stop_oi_create(false);
// Flag indicating whether the OI creation thread is running or not
static std::atomic<bool> oi_create_is_executing(false);

// Output: out_oi_handle. Set to 0 if fail
static SaAisErrorT initOiHandle(SaImmOiHandleT *out_oi_handle) {
  TRACE_ENTER();
  SaAisErrorT ais_rc = SA_AIS_OK;
  const SaImmOiCallbacksT_2 *callbacks = getImmOiCallbacks();
  base::Timer try_again_timer(kOiHandleRequestTimeout);
  while (try_again_timer.is_timeout() == false) {
    if (stop_oi_create) {
      ais_rc = SA_AIS_OK;
      break;
    }
    SaVersionT imm_version = kImmVersion;
    ais_rc = saImmOiInitialize_2(out_oi_handle, callbacks, &imm_version);
    if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(kOiTryAgainDelay);
      continue;
    }
    if (ais_rc != SA_AIS_OK) {
      LOG_WA("%s: saImmOiInitialize_2() Fail, %s", __FUNCTION__,
            saf_error(ais_rc));
      *out_oi_handle = 0;
    }
    break;
  }
  if ((try_again_timer.is_timeout() == true) &&
      (ais_rc == SA_AIS_ERR_TRY_AGAIN)) {
    LOG_WA("%s: saImmOiInitialize_2() Fail, TRY AGAIN timeout", __FUNCTION__);
    *out_oi_handle = 0;
  }

  return ais_rc;
}

// Return -1 if Fail
// out_ais_rc is set to return code from saImmOiSelectionObjectGet()
static SaSelectionObjectT requestOiSelectionObject(SaImmOiHandleT oi_handle,
                                                   SaAisErrorT* out_ais_rc) {
  TRACE_ENTER();
  SaSelectionObjectT oi_selection_object = -1;
  SaAisErrorT ais_rc = SA_AIS_OK;
  base::Timer try_again_timer(kOiRequestTimeout);
  while (try_again_timer.is_timeout() == false) {
    if (stop_oi_create) {
      oi_selection_object = -1;
      break;
    }
    ais_rc = saImmOiSelectionObjectGet(oi_handle, &oi_selection_object);
    if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(kOiTryAgainDelay);
      continue;
    }
    if (ais_rc != SA_AIS_OK) {
      LOG_WA("%s: saImmOiSelectionObjectGet() Fail, %s", __FUNCTION__,
             saf_error(ais_rc));
      oi_selection_object = -1;
    }
    break;
  }
  if ((try_again_timer.is_timeout() == true) &&
      (ais_rc == SA_AIS_ERR_TRY_AGAIN)) {
    LOG_WA("%s: saImmOiSelectionObjectGet() Fail, TRY AGAIN timeout",
           __FUNCTION__);
    oi_selection_object = -1;
  }

  *out_ais_rc = ais_rc;
  return oi_selection_object;
}

// Set implementer name in kImplementerName
static SaAisErrorT setOiImplementerName(SaImmOiHandleT oi_handle) {
  TRACE_ENTER();
  SaAisErrorT ais_rc = SA_AIS_OK;
  base::Timer try_again_timer(kOiRequestTimeout);
  while (try_again_timer.is_timeout() == false) {
    if (stop_oi_create) {
      ais_rc = SA_AIS_OK;
      break;
    }
    ais_rc = saImmOiImplementerSet(oi_handle, kImplementerName);
    if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(kOiTryAgainDelay);
      continue;
    } else if (ais_rc == SA_AIS_ERR_EXIST) {
      // Should be released. May be temporary. Lets try again
      TRACE("%s saImmOiImplementerSet() Fail %s. Try to create again",
             __FUNCTION__, saf_error(ais_rc));
      base::Sleep(kOiTryAgainDelay);
      continue;
    } else if (ais_rc != SA_AIS_OK) {
      LOG_WA("%s: saImmOiImplementerSet() Fail, %s", __FUNCTION__,
             saf_error(ais_rc));
      break;
    }
    break;
  }
  if ((try_again_timer.is_timeout() == true) &&
      (ais_rc == SA_AIS_ERR_TRY_AGAIN)) {
    LOG_WA("%s: saImmOiImplementerSet() Fail, TRY AGAIN timeout",
           __FUNCTION__);
  }

  TRACE_LEAVE2("ais_rc = %s", saf_error(ais_rc));
  return ais_rc;
}

static SaAisErrorT setClassImplementer(SaImmOiHandleT oi_handle,
                                       const SaImmClassNameT class_name) {
  TRACE_ENTER();
  SaAisErrorT ais_rc = SA_AIS_OK;
  base::Timer try_again_timer(kOiRequestTimeout);
  while (try_again_timer.is_timeout() == false) {
    if (stop_oi_create) {
      ais_rc = SA_AIS_OK;
      break;
    }
    ais_rc = saImmOiClassImplementerSet(oi_handle, class_name);
    if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(kOiTryAgainDelay);
      continue;
    }
    if (ais_rc != SA_AIS_OK) {
      LOG_WA("%s: saImmOiClassImplementerSet() Fail, %s", __FUNCTION__,
             saf_error(ais_rc));
    }
    break;
  }
  if ((try_again_timer.is_timeout() == true) &&
      (ais_rc == SA_AIS_ERR_TRY_AGAIN)) {
    LOG_WA("%s: saImmOiClassImplementerSet() Fail, TRY AGAIN timeout",
           __FUNCTION__);
  }

  return ais_rc;
}

// Activate the poll loop in main()
// This will reinstall IMM poll event handling
static void sendOiCreatedMessageToMainThread() {
  TRACE_ENTER();
  // Note: lgsv_evt must be allocated C style since it will be freed in other
  //       part of the code with free()
  lgsv_lgs_evt_t *lgsv_evt;
  lgsv_evt = static_cast<lgsv_lgs_evt_t *>(calloc(1, sizeof(lgsv_lgs_evt_t)));
  osafassert(lgsv_evt);

  lgsv_evt->evt_type = LGSV_EVT_NO_OP;
  if (m_NCS_IPC_SEND(&lgs_mbx, lgsv_evt, LGS_IPC_PRIO_CTRL_MSGS) !=
      NCSCC_RC_SUCCESS) {
    LOG_WA("imm_reinit_thread failed to send IPC message to main thread");
    /*
     * Se no reason why this would happen. But if it does at least there
     * is something in the syslog. The main thread should still pick up
     * the new imm FD when there is a healthcheck, but it could take
     *minutes.
     */
    free(lgsv_evt);
  }
}

// Creates the log service OI
// stop_oi_create:  Used to stop thread execution as soon as possible
//                  Will stop the thread when set to true.
static void createLogServerOi() {
  TRACE_ENTER();
  SaAisErrorT ais_rc = SA_AIS_OK;
  SaImmOiHandleT oi_handle = 0;
  SaSelectionObjectT oi_selection_object = -1;
  base::Timer io_create_timeout(kOiCreationTimeout);

  while (io_create_timeout.is_timeout() == false) {
    // OI init sequence
    // Start by finalize OI (handle) in case of sequence recovery
    // See continue below
    finalizeOi(oi_handle);

    // Init OI handle
    if (stop_oi_create) break;
    ais_rc = initOiHandle(&oi_handle);
    if (ais_rc != SA_AIS_OK) {
      LOG_NO("%s: initOiHandle() Fail", __FUNCTION__);
      oi_handle = 0;
      break;
    }

    // Set OI implementer name
    if (stop_oi_create) break;
    ais_rc = setOiImplementerName(oi_handle);
    if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
      TRACE("%s: setOiImplementerName() Fail %s. Try to recover",
            __FUNCTION__, saf_error(ais_rc));
      continue;
    } else if (ais_rc != SA_AIS_OK) {
      LOG_NO("%s: setOiImplementerName() Fail", __FUNCTION__);
      break;
    }

    // Become class implementer for log config class
    if (stop_oi_create) break;
    ais_rc = setClassImplementer(oi_handle, kLogConfigurationClass);
    if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
      TRACE("%s: setClassImplementer(LogConfigurationClass) Fail %s. "
            "Try to recover", __FUNCTION__, saf_error(ais_rc));
      continue;
    } else if (ais_rc != SA_AIS_OK) {
      LOG_NO("%s: setClassImplementer(LogConfigurationClass) Fail",
             __FUNCTION__);
      break;
    }

    // Become class implementer for stream config class
    if (stop_oi_create) break;
    ais_rc = setClassImplementer(oi_handle, kLogStreamClass);
    if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
      TRACE("%s: setClassImplementer(kLogStreamClass) Fail %s. "
            "Try to recover", __FUNCTION__, saf_error(ais_rc));
      continue;
    } else if (ais_rc != SA_AIS_OK) {
      LOG_NO("%s: setClassImplementer(kLogStreamClass) Fail", __FUNCTION__);
      break;
    } else {
      // Get the selection object
      oi_selection_object = requestOiSelectionObject(oi_handle, &ais_rc);
      if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
        TRACE("%s: requestOiSelectionObject() Fail", __FUNCTION__);
      }
      break;
    }
  }

  // Creation is done, has failed or is stopped.
  if (stop_oi_create) {
    TRACE("%s: OI create is stopped", __FUNCTION__);
  } else if ((io_create_timeout.is_timeout() == true) &&
             (ais_rc != SA_AIS_OK)) {
    LOG_WA("%s: Fail, OI creation timeout", __FUNCTION__);
    // The legacy behavior is to exit log service in this case
    lgs_exit("createLogServerOi() failed");
  } else if (ais_rc != SA_AIS_OK) {
    LOG_WA("%s: Fail, OI creation error", __FUNCTION__);
    // The legacy behavior is to exit log service in this case
    lgs_exit("createLogServerOi() failed");
  }

  // Save OI handle and selection object
  // Note: This can be done unconditionally here since failing will result in
  // a log server restart
  setProtectedGlobals(oi_handle, oi_selection_object);
}

// The thread function for creating a log server OI
// stop_oi_create:  Used to stop thread execution as soon as possible
//                  Will stop the thread when set to true.
static void oiCreationThread() {
  TRACE_ENTER();
  createLogServerOi();
  sendOiCreatedMessageToMainThread();
  oi_create_is_executing = false;
  return;
}

// Public functions
//-----------------

SaImmOiHandleT lgsGetOiHandle() {
  base::Lock protect_globals(lgs_OI_init_mutex);
  return oi_handle_;
}

SaSelectionObjectT lgsGetOiSelectionObject() {
  base::Lock protect_globals(lgs_OI_init_mutex);
  return oi_selection_object_;
}

SaSelectionObjectT lgsGetOiFdsParams(SaImmOiHandleT* out_oi_handle) {
  base::Lock protect_globals(lgs_OI_init_mutex);
  *out_oi_handle = oi_handle_;
  return oi_selection_object_;
}

void lgsOiCreateBackground() {
  TRACE_ENTER();
  if (oi_create_is_executing == true) {
    // Thread is executing. Do nothing
    return;
  }

  // Make sure that the OI is finalized before creating a new one
  finalizeOi(oi_handle_);
  setProtectedGlobals(0, -1);

  // Start a OI creation thread
  // Note: Throws an exception if "The system is unable to start a new thread"
  // Should never happen so it is not handled here (will cause node restart)
  oi_create_is_executing = true;
  std::thread (&oiCreationThread).detach();
}

void lgsOiCreateSynchronous() {
  TRACE_ENTER();
  // Note: oi_handle_ can be used unprotected here since we know that there is
  // no running oiCreationThread
  // There is no meaning to call finalizeOI() since this function only shall be
  // used for the first creation of an OI handle during initialization of the
  // log service
  oi_handle_ = 0;
  oi_selection_object_ = -1;
  createLogServerOi();
}

// Note: In this version this is just call lgsOiCreateBackground(). The reason
// for having this function is to have a place where an implementer set can
// be done instead of creating an OI if the lgsOiStop function is changed
// to just clear the OI. Note that if set fail the OI must be created anyway
//
// This function shall be used only when changing HA state from standby to
// active
void lgsOiStart() {
  TRACE_ENTER();
  lgsOiCreateBackground();
}

// Note: oi_handle_ and oi_selection_object_ is always set in thread also if
// thread is stopped. Is set to 0 and -1 respectively in all cases except when
// an OI is successfully created
//
// Note: This version will completely finalize the OI. This is probably good
//       enough. Clear / Set handling may be a bit faster but Stop / Start of
//       OI should be rare.
//       If optimization is needed we can do implementer clear here
void lgsOiStop() {
  TRACE_ENTER();
  // This is a request for the OI creation thread to stop
  stop_oi_create = true;

  // Wait for thread to stop if it is running
  base::Timer io_thread_stop_timeout(kOiWaitStopTimeout);
  while (io_thread_stop_timeout.is_timeout() != true) {
    // Wait for the OI creation thread to stop (return)
    // Note: The thread has to be detached so join cannot be used
    if (oi_create_is_executing == false) break;
    base::Sleep(base::kOneHundredMilliseconds);
  }
  if ((io_thread_stop_timeout.is_timeout() == true) &&
      (oi_create_is_executing == true)) {
    // The OI creation thread is hanging. Recover by restarting log server
    lgs_exit("lgsOiStop: Stopping OI create thread timeout");
  }

  // Note: oi_handle_ can be used unprotected here since we know that there is
  // no running oiCreationThread
  finalizeOi(oi_handle_);
  setProtectedGlobals(0, -1);
  stop_oi_create = false;
}
