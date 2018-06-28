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

#include "smf/smfd/imm_modify_config/add_operation_to_ccb.h"

#include <limits.h>

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <stdexcept>

#include "base/logtrace.h"
#include "base/time.h"
#include "ais/include/saImm.h"
#include "ais/include/saAis.h"

#include "smf/smfd/imm_modify_config/immccb.h"
#include "smf/smfd/imm_modify_config/attribute.h"

#include "smf/smfd/imm_om_ccapi/common/common.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_create.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_delete.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_modify.h"

namespace modelmodify {

// Error information. See immccb.h
static std::string api_name_;
static SaAisErrorT ais_error_ = SA_AIS_OK;

void GetAddToCbbErrorInfo(ErrorInformation *error_info) {
  error_info->api_name = api_name_;
  error_info->ais_error = ais_error_;
}

bool IsResorceAbort(const SaImmCcbHandleT& ccbHandle) {
  bool rc = false;

  const SaStringT *errString = nullptr;
  SaAisErrorT ais_rc = saImmOmCcbGetErrorStrings(ccbHandle, &errString);
  if ((ais_rc == SA_AIS_OK) && (errString != nullptr)) {
    LOG_NO("%s: Error string: '%s'", __FUNCTION__, errString[0]);
    std::string err_str(errString[0]);
    if (err_str.find("IMM: Resource abort: ") != std::string::npos) {
      // Is Resource Abort
      rc = true;
    }
  }

  return rc;
}

int AddCreateToCcb(const SaImmCcbHandleT& ccb_handle,
                       const CreateDescriptor& create_descriptor) {
  TRACE_ENTER2("Parent '%s', Class '%s'",
               create_descriptor.parent_name.c_str(),
               create_descriptor.class_name.c_str());

  int recovery_info = kNotSet;

  // Setup a creator with class name and parent name
  // -----------------------------------------------
  immom::ImmOmCcbObjectCreate creator(ccb_handle);
  if (create_descriptor.parent_name.empty() != true) {
    // Do not call this method if no parent name is given. The object will be
    // created as a root object
    creator.SetParentName(create_descriptor.parent_name);
  }
  creator.SetClassName(create_descriptor.class_name);

  // Add all values for all attributes that shall be set at creation time
  // including the object name to the creator
  // Note:  For each attribute the creator needs a vector of pointers to the
  //        attribute values. This vector  must have the same scope as the
  //        creator.
  AttributeHandler attributes(&creator);
  if(attributes.AddAttributesForObjectCreate(create_descriptor) == false) {
    LOG_NO("%s: SetAttributeValues() Fail", __FUNCTION__);
    recovery_info = kFail;
    api_name_.clear();
    ais_error_ = SA_AIS_OK;
  }

  // Add the create operation to the CCB
  if (recovery_info != kFail) {
    if (creator.AddObjectCreateToCcb() == false) {
      // Add create Fail
      SaAisErrorT ais_rc = creator.ais_error();
      if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
        recovery_info = kRestartOm;
        TRACE("%s: AddObjectCreateToCcb(), %s, kRestartOm",
              __FUNCTION__, saf_error(ais_rc));
      } else if (ais_rc == SA_AIS_ERR_FAILED_OPERATION) {
        if (IsResorceAbort(ccb_handle)) {
          TRACE("%s: AddObjectCreateToCcb(), %s, kRestartOm",
                __FUNCTION__, saf_error(ais_rc));
          recovery_info = kRestartOm;
        } else {
          LOG_NO("%s: AddObjectCreateToCcb() Fail, %s", __FUNCTION__,
                 saf_error(ais_rc));
          recovery_info = kFail;
          api_name_ = "saImmOmCcbObjectCreate_2";
          ais_error_ = ais_rc;
        }
      } else if (ais_rc == SA_AIS_ERR_EXIST) {
        if (create_descriptor.ignore_ais_err_exist == true) {
          recovery_info = kContinue;
        } else {
          recovery_info = kFail;
        }
        // Note: This information is always needed also if we do not fail
        api_name_ = "saImmOmCcbObjectCreate_2";
        ais_error_ = ais_rc;
      } else {
        // Unrecoverable Fail
        LOG_NO("%s: ObjectCreateCcbAdd(), %s, kFail",
               __FUNCTION__, saf_error(ais_rc));
        recovery_info = kFail;
        api_name_ = "saImmOmCcbObjectCreate_2";
        ais_error_ = ais_rc;
      }
    } else {
      // Add create success
      recovery_info = kContinue;
    }
  }

  TRACE_LEAVE2("recovery_info = %s", RecoveryTxt(recovery_info));
  return recovery_info;
}

int AddDeleteToCcb(const SaImmCcbHandleT& ccb_handle,
                         const DeleteDescriptor&
                         delete_descriptor) {
  TRACE_ENTER();
  int recovery_info = kNotSet;

  // Setup an object deleter
  immom::ImmOmCcbObjectDelete deletor(ccb_handle);

  // Add the delete operation to the CCB. Try again if BUSY
  base::Timer busy_timer(kBusyTimeout);
  while (busy_timer.is_timeout() != true) {
    if (deletor.AddObjectDeleteToCcb(delete_descriptor.object_name) == false) {
      // Add delete Fail
      SaAisErrorT ais_rc = deletor.ais_error();
      if (ais_rc == SA_AIS_ERR_BUSY) {
        base::Sleep(base::MillisToTimespec(kBusyWait));
        continue;
      } else if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
        TRACE("%s: AddObjectDeleteToCcb() RestartOm, %s", __FUNCTION__,
              saf_error(ais_rc));
        recovery_info = kRestartOm;
        break;
      } else if (ais_rc == SA_AIS_ERR_FAILED_OPERATION) {
        if (IsResorceAbort(ccb_handle)) {
          TRACE("%s: AddObjectDeleteToCcb(), %s, RestartOm",
                __FUNCTION__, saf_error(ais_rc));
          recovery_info = kRestartOm;
          break;
        } else {
          // Unrecoverable Fail
          LOG_NO("%s: AddObjectDeleteToCcb() Fail, %s", __FUNCTION__,
                 saf_error(ais_rc));
          recovery_info = kFail;
          api_name_ = "saImmOmCcbObjectDelete";
          ais_error_ = ais_rc;
          break;
        }
      } else if (ais_rc == SA_AIS_ERR_NOT_EXIST) {
        if (delete_descriptor.ignore_ais_err_not_exist == true) {
          recovery_info = kContinue;
        } else {
          LOG_NO("%s: AddObjectDeleteToCcb() Fail, %s", __FUNCTION__,
                 saf_error(ais_rc));
          recovery_info = kFail;
          api_name_ = "saImmOmCcbObjectDelete";
          ais_error_ = ais_rc;
        }
      }  else {
        // Other unrecoverable Fail
        LOG_NO("%s: AddObjectDeleteToCcb() Fail, %s", __FUNCTION__,
               saf_error(ais_rc));
        recovery_info = kFail;
        api_name_ = "saImmOmCcbObjectDelete";
        ais_error_ = ais_rc;
        break;
      }
    }

    // AddObjectDeleteToCcb() Success
    recovery_info = kContinue;
    break;
  }
  if ((busy_timer.is_timeout() == true) &&
      (recovery_info == kNotSet)) {
    LOG_NO("%s: AddObjectDeleteToCcb() Fail, BUSY timeout",
           __FUNCTION__);
    recovery_info = kFail;
    api_name_ = "";
    ais_error_ = SA_AIS_OK;
  }

  TRACE_LEAVE();
  return recovery_info;
}

// Add one modify operation to a CCB
// Recovery:  BAD HANDLE; kRestartOm
//            FAILED OPERATION; kRestartOm or kFail
//            BUSY; An admin operation is ongoing on an object to be modified
//                  We can try again to add the modify
// return: Recovery information. See immccb.h
int AddModifyToCcb(const SaImmCcbHandleT& ccb_handle,
                         const ModifyDescriptor&
                         modify_descriptor) {
  TRACE_ENTER();
  int recovery_info = kNotSet;

  // Setup an object modifier
  immom::ImmOmCcbObjectModify modifier(ccb_handle,
                                       modify_descriptor.object_name);

  // One modify operation does only modify one object but may modify several
  // attributes in that object.
  // Add all values for all attributes that shall be modified
  // including the object name to the modifier
  // Note:  For each attribute the modifier needs a vector of pointers to the
  //        modify descriptors. This vector  must have the same scope as the
  //        modifier. Each modify descriptor contains a value and a
  //        modification type
  AttributeHandler modifications(&modifier);
  if(modifications.AddAttributesForModification(modify_descriptor) == false) {
    LOG_NO("%s: SetAttributeValues() Fail", __FUNCTION__);
    recovery_info = kFail;
    api_name_ = "";
    ais_error_ = SA_AIS_OK;
  }

  if (recovery_info == kNotSet) {
    // Add the modification request to the CCB. Try gain if BUSY
    SaAisErrorT ais_rc = SA_AIS_OK;
    base::Timer busy_timer(kBusyTimeout);
    while (busy_timer.is_timeout() != true) {
      if (modifier.AddObjectModifyToCcb() == false) {
        ais_rc = modifier.ais_error();
        if (ais_rc == SA_AIS_ERR_BUSY) {
          base::Sleep(base::MillisToTimespec(kBusyWait));
          continue;
        } else if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
          TRACE("%s: AddObjectModifyToCcb() RestartOm, %s", __FUNCTION__,
                saf_error(ais_rc));
          recovery_info = kRestartOm;
          break;
        } else if (ais_rc == SA_AIS_ERR_FAILED_OPERATION) {
          if (IsResorceAbort(ccb_handle)) {
            TRACE("%s: AddObjectModifyToCcb(), %s, RestartOm",
                  __FUNCTION__, saf_error(ais_rc));
            recovery_info = kRestartOm;
            break;
          } else {
            // Unrecoverable Fail
            LOG_NO("%s: AddObjectModifyToCcb() Fail, %s", __FUNCTION__,
                   saf_error(ais_rc));
            recovery_info = kFail;
            api_name_ = "saImmOmCcbObjectModify_2";
            ais_error_ = ais_rc;
            break;
          }
        } else {
            // Unrecoverable Fail
            LOG_NO("%s: AddObjectModifyToCcb() Fail, %s", __FUNCTION__,
                   saf_error(ais_rc));
            recovery_info = kFail;
            api_name_ = "saImmOmCcbObjectModify_2";
            ais_error_ = ais_rc;
            break;
        }
      }

      // Add Modify to CCB Success
      recovery_info = kContinue;
      break;
    }
    if ((busy_timer.is_timeout() == true) &&
        (recovery_info == kNotSet)) {
      LOG_NO("%s: AddObjectModifyToCcb() Fail, BUSY timeout",
             __FUNCTION__);
      recovery_info = kFail;
      api_name_ = "";
      ais_error_ = SA_AIS_OK;
    }
  }

  TRACE_LEAVE();
  return recovery_info;
}

}  // namespace modelmodify
