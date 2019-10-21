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

// Note:
// IMM C++ "wrappers" for CCB handling IMM AOIs are copied from
// experimental/immcpp and used slightly modified here. The "wrapper" copies
// can be found in the smfd/imm_om_ccapi directory.

#include "smf/smfd/imm_modify_config/immccb.h"

#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <utility>

#include "ais/include/saImm.h"
#include "ais/include/saAis.h"
#include "base/logtrace.h"
#include "base/time.h"
#include "base/saf_error.h"

#include "smf/smfd/imm_modify_config/add_operation_to_ccb.h"

#include "smf/smfd/imm_om_ccapi/common/common.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_clear.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_handle.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_set.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_handle.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_create.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_delete.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_modify.h"
#include "smf/smfd/imm_om_ccapi/om_handle.h"

namespace modelmodify {

//-------------------------
// class ObjectModification
//-------------------------

// The global instance number
std::atomic<unsigned int> ModelModification::next_instance_number_{1};

ModelModification::ModelModification()
    : imm_om_handle_(nullptr),
      imm_ccb_handle_(nullptr),
      imm_ao_handle_(nullptr),
      imm_ao_owner_set_(nullptr),
      modification_timeout_(120000),
      ccb_flags_(0),
      api_name_("saImmOmCcbApply"),
      ais_error_(SA_AIS_OK) {
    TRACE_ENTER();
    // Create a unique admin owner name for this ObjectModification object
    instance_number_ = next_instance_number_++;
    admin_owner_name_ = "SmfObjectModification" +
        std::to_string(instance_number_);
    TRACE_LEAVE();
  }

ModelModification::~ModelModification() {
  // If an OM handle exists, cleanup the CCB handling by finalizing the handle
  // Note that releaseOwnershipFinalize must be set when creating an OM admin
  // owner handle so that admin ownership is released
  TRACE_ENTER();
  FinalizeHandles();
  TRACE_LEAVE();
}

bool ModelModification::DoModelModification(CcbDescriptor modifications) {
  TRACE_ENTER();
  bool return_status = false;
  int recovery_info = kNotSet;
  base::Timer modification_time(modification_timeout_);

  while (modification_time.is_timeout() == false) {
    // If CCB creation fail invalid handles shall be finalized
    // Note: This function is safe to call also if there are no handles
    FinalizeHandles();

    // Create all needed handles
    recovery_info = CreateHandles();
    if (recovery_info == kFail) {
      LOG_NO("%s: CreateHandles() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: CreateHandles() Restart", __FUNCTION__);
      continue;
    }

    // AddCreates. Request creation of IMM objects
    recovery_info = AddCreates(modifications.create_descriptors);
    if (recovery_info == kFail) {
      LOG_NO("%s: AddCreates() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: AddCreates() Restart", __FUNCTION__);
      continue;
    }

    // AddModifications. Request modifications of exiting IMM objects
    recovery_info = AddModifies(modifications.modify_descriptors);
    if (recovery_info == kFail) {
      LOG_NO("%s: AddModifies() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: AddModifies() Restart", __FUNCTION__);
      continue;
    }

    // AddDeletes. Delete IMM objects
    recovery_info = AddDeletes(modifications.delete_descriptors);
    if (recovery_info == kFail) {
      LOG_NO("%s: AddDeletes() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: AddDeletes() Restart", __FUNCTION__);
      continue;
    }

    // Apply the IMM CCB
    recovery_info = ApplyModifications();
    if (recovery_info == kFail) {
      LOG_NO("%s: ApplyModifications() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: ApplyModifications() Restart", __FUNCTION__);
      continue;
    }

    // The CCB is applied
    return_status = true;
    break;
  }  // while (modification_timer.is_timeout() == false)
  if ((modification_time.is_timeout() == true) && (return_status == false)) {
    // Recovery handling has timed out
    LOG_NO("%s: Fail, Timeout", __FUNCTION__);
  }

  // Cleanup all requested resources
  FinalizeHandles();
  TRACE_LEAVE2("return_status = %s", return_status? "true" : "false");
  return return_status;
}

// Create handles needed for doing modification using an IMM CCB
// Handles are: OM handle, Admin Owner handle and CCB handle
// Note1: Handle objects are created and private handle pointers are filled in
// Note2: No admin ownership is set here
// Return:  Recovery information. See Private constants
int ModelModification::CreateHandles() {
  TRACE_ENTER();

  // Create Object Manager handle
  int recovery_info = CreateObjectManager();
  if (recovery_info == kFail) {
    LOG_NO("%s: CreateOmHandle() Fail", __FUNCTION__);
  }

  // The Admin Owner handle is created and an admin owner name is
  // registered. Uses Object Manager handle
  // Note: Admin Owner Set is not done here
  if (recovery_info == kContinue) {
    recovery_info = CreateAdminOwner();
    if (recovery_info == kFail) {
      LOG_NO("%s: CreateAdminOwner() Fail", __FUNCTION__);
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: CreateAdminOwner() Restart", __FUNCTION__);
    }
  }

  // A CCB is initialized and a CCB handle is created.
  // Uses Admin Owner handle
  if (recovery_info == kContinue) {
    recovery_info = CreateCcb();
    if (recovery_info == kFail) {
      LOG_NO("%s: CreateCcb() Fail", __FUNCTION__);
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: CreateCcb() Restart", __FUNCTION__);
    }
  }
  TRACE_LEAVE();
  return recovery_info;
}


// Create an OM handle. Try again timeout should be 60 seconds
// Recovery: No AIS return code except TRY AGAIN is valid for retry
// Output:  A valid OM handle. An OM handle object is created
// Return:  Recovery information. See Private constants
int ModelModification::CreateObjectManager() {
  TRACE_ENTER();
  int recovery_info = kNotSet;
  if (imm_om_handle_ == nullptr) {
    imm_om_handle_ = std::unique_ptr<immom::ImmOmHandle>
        (new immom::ImmOmHandle());
    // Change TRY AGAIN timeout to 60 sec
    ImmBase::RetryControl RetryTiming;
    RetryTiming.interval = base::kFourtyMilliseconds;
    RetryTiming.timeout = 60000;
    imm_om_handle_->ChangeDefaultRetryControl(RetryTiming);
  } else {
    TRACE("%s: OM handle object exists", __FUNCTION__);
  }

  bool return_state = imm_om_handle_->InitializeHandle();
  if (return_state == false) {
    SaAisErrorT ais_error = imm_om_handle_->ais_error();
    if (ais_error == SA_AIS_ERR_TRY_AGAIN) {
      recovery_info = kRestartOm;
    } else {
      // No recovery is possible
      LOG_NO("%s: OM-handle, RestoreHandle(), Fail", __FUNCTION__);
      recovery_info = kFail;
      api_name_ = "saImmOmInitialize";
    }
  } else {
    recovery_info = kContinue;
  }
  TRACE_LEAVE2("%s", RecoveryTxt(recovery_info));
  return recovery_info;
}

// Create an admin owner. Based on a valid OM handle
// Creates an admin owner handle object, initializes a handle and creates an
// admin owner object for handling admin owner set and release
// Note:  Important to set releaseOwnershipOnFinalize to true
// Recovery:  BAD HANDLE; Restart CCB handling
// Input:   An admin owner name. Should be cluster unique and unique for each
//          instance of this class. See private variables
// Output:  A valid Admin Owner (AO) handle. An admin owner handle object and
//          an Admin Owner Set object is created
// Return:  Recovery information. See Private constants
int ModelModification::CreateAdminOwner() {
  TRACE_ENTER();
  int recovery_info = kNotSet;
  if (imm_ao_handle_ == nullptr) {
    imm_ao_handle_ = std::unique_ptr<immom::ImmOmAdminOwnerHandle>
        (new immom::ImmOmAdminOwnerHandle(0, admin_owner_name_));
  }

  SaImmHandleT om_handle = imm_om_handle_->GetHandle();
  imm_ao_handle_->ReleaseOwnershipOnFinalize();
  bool rc = imm_ao_handle_->InitializeHandle(om_handle);
  if (rc == false) {
    SaAisErrorT ais_rc = imm_ao_handle_->ais_error();
    if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
      TRACE("%s: Admin Owner handle, InitializeHandle() Restart",
            __FUNCTION__);
      recovery_info = kRestartOm;
    } else {
      LOG_NO("%s: Admin Owner handle, InitializeHandle() Fail",
             __FUNCTION__);
      recovery_info = kFail;
      api_name_ = "saImmOmAdminOwnerInitialize";
      ais_error_ = ais_rc;
    }
  } else {
    SaImmAdminOwnerHandleT ao_handle = imm_ao_handle_->GetHandle();
    if (imm_ao_owner_set_ == nullptr) {
      imm_ao_owner_set_ = std::unique_ptr<immom::ImmOmAdminOwnerSet>
          (new immom::ImmOmAdminOwnerSet(ao_handle));
    } else {
      imm_ao_owner_set_->UpdateHandle(ao_handle);
    }
    recovery_info = kContinue;
  }
  TRACE_LEAVE2("%s", RecoveryTxt(recovery_info));
  return recovery_info;
}

// Initialize a CCB. Based on a valid Admin Owner handle
// Recovery:  BAD HANDLE; Restart CCB handling
// Output:  A valid CCB handle. A CCB handle object is created
// Return:  Recovery information. See Private constants
int ModelModification::CreateCcb() {
  TRACE_ENTER();
  int recovery_info = kNotSet;
  if (imm_ccb_handle_ == nullptr) {
    imm_ccb_handle_ = std::unique_ptr<immom::ImmOmCcbHandle>
        (new immom::ImmOmCcbHandle(0, ccb_flags_));
  }

  SaImmAdminOwnerHandleT ao_handle = imm_ao_handle_->GetHandle();
  bool rc = imm_ccb_handle_->InitializeHandle(ao_handle);
  if (rc == false) {
    SaAisErrorT ais_rc = imm_ccb_handle_->ais_error();
    if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
      TRACE("%s: CCB handle, InitializeHandle() Restart", __FUNCTION__);
      recovery_info = kRestartOm;
    } else {
      LOG_NO("%s: CCB handle, InitializeHandle() Fail", __FUNCTION__);
      recovery_info = kFail;
      api_name_ = "saImmOmCcbInitialize";
      ais_error_ = ais_rc;
    }
  } else {
    recovery_info = kContinue;
  }

  TRACE_LEAVE();
  return recovery_info;
}

// Finalize all handles. Return code does not matter here
void ModelModification::FinalizeHandles() {
  TRACE_ENTER();
  if (imm_om_handle_ != nullptr) {
    // If an OM handle object exists the OM handle owned by the object shall be
    // finalized. If no handle object exists there is no OM handle to finalize
    imm_om_handle_->FinalizeHandle();
  }
  TRACE_LEAVE();
}

// Set admin ownership. Based on a valid Owner handle
// For modify and create operations scope shall be SA_IMM_ONE (one object)
// For delete operations scope shall be set to SA_IMM_SUBTREE (object and its
// subtree)
// Note:  If there are no objects no attempt to set an admin owner will be done
//        and kContinue will be returned. Will be the case if a root object is
//        created.
// Recovery:  BAD HANDLE; Start from "get OM Handle"
//            EXIST;  Someone else is already owner of the object(s). Wait and
//                    re-try
// Input:   List of objects to become admin owner of.
//          Scope. Shall be SA_IMM_ONE for create and modify or SA_IMM_SUBTREE
//          for delete.
// Output:  None. We have taken ownership of one or several objects. This is
//          a cluster wide resource. Others trying to become owner of the
//          object(s) will fail with EXIST
// Return:  Recovery information. See Private constants
int ModelModification::AdminOwnerSet(const std::vector<std::string>& objects,
                                     const SaImmScopeT scope) {
  TRACE_ENTER();

  if (imm_ao_owner_set_ == nullptr) {
    // Should never happen
    LOG_ER("%s: No admin owner set handler is created", __FUNCTION__);
    api_name_ = "";
    ais_error_ = SA_AIS_OK;
    return kFail;
  }

  if (objects.empty()) {
    // Should never happen
    LOG_ER("%s: No objects", __FUNCTION__);
    api_name_ = "";
    ais_error_ = SA_AIS_OK;
    return kFail;
  }

  // The ImmOmAdminOwnerSet exists and may have been used earlier so clean the
  // object list before adding new objects
  imm_ao_owner_set_->ClearObjectNames();
  // Set objects to become admin owner of
  for (auto& object : objects) {
    if (object.empty()) {
      // Do not add empty strings (IMM root)
      continue;
    }
    imm_ao_owner_set_->AddObjectName(object);
  }

  // Become admin owner
  // An object is a cluster global resource. We cannot become admin owner if
  // the object is already owned by someone else. Wait a reasonable time for the
  // resource to be released
  base::Timer exist_timer(kExistTimeout);
  SaAisErrorT ais_rc = SA_AIS_OK;
  int recovery_info = kNotSet;

  while (exist_timer.is_timeout() == false) {
    if (imm_ao_owner_set_->SetAdminOwner(scope) == false) {
      ais_rc = imm_ao_owner_set_->ais_error();
      if (ais_rc == SA_AIS_ERR_EXIST) {
        // Another owner of object(s) exist. Wait for other owner release
        base::Sleep(base::MillisToTimespec(kExistWait));
        continue;
      } else if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
        TRACE("%s: SetAdminOwner() Restart, %s", __FUNCTION__,
              saf_error(ais_rc));
        recovery_info = kRestartOm;
        break;
      } else {
        LOG_NO("%s: SetAdminOwner() Fail, %s", __FUNCTION__,
               saf_error(ais_rc));
        recovery_info = kFail;
        api_name_ = "saImmOmAdminOwnerSet";
        ais_error_ = ais_rc;
        break;
      }
    }

    recovery_info = kContinue;
    break;
  }
  if ((exist_timer.is_timeout() == true) && (recovery_info == kNotSet)) {
    LOG_NO("%s: SetAdminOwner() Fail, EXIST timeout",
           __FUNCTION__);
    recovery_info = kFail;
    api_name_ = "";
    ais_error_ = SA_AIS_OK;
  }

  TRACE_LEAVE();
  return recovery_info;
}

static bool has_object(const std::vector<CreateDescriptor>& create_descriptors,
                       const std::string& object) {
  for (const auto& desc : create_descriptors) {
    if (object == desc.parent_name) return true;
  }
  return false;
}

// Add create requests for all objects to be created
// Set admin ownership for parent to all objects to be created with scope
// SA_IMM_ONE.
// Add all create operations to the CCB
// Note1: We have to be owner of the parent(s)
// Note2: It is not a problem to set admin ownership of an object that already
//        has an admin owner if the same admin owner name is used, meaning that
//        this is not needed to check
// Recovery:  BAD HANDLE; Restart CCB handling
//            FAILED OPERATION; Restart CCB handling if resource abort
// Return:  Recovery information
int ModelModification::AddCreates(const std::vector<CreateDescriptor>&
                                  create_descriptors) {
  TRACE_ENTER();
  int recovery_info = kNotSet;

  // Do this for all create descriptors
  for (auto& create_descriptor : create_descriptors) {
    if (create_descriptor.parent_name.empty() == false) {
      // Become admin owner of parent if there is a parent. If no parent the
      // IMM object will be created as a root object
      std::vector<std::string> imm_objects;
      std::string parent = create_descriptor.parent_name;
      imm_objects.push_back(parent);
      recovery_info = AdminOwnerSet(imm_objects, SA_IMM_ONE);
      if (recovery_info == kFail) {
        // data for this ccb is valid but it is aborted by imm to start
        // IMM sync, therefore marking this ccb is restartable.
        if (ais_error_ == SA_AIS_ERR_NOT_EXIST &&
            has_object(create_descriptors, parent)) {
          recovery_info = kRestartOm;
          break;
        }
        LOG_NO("%s: AdminOwnerSet() Fail", __FUNCTION__);
        break;
      } else if (recovery_info == kRestartOm) {
        TRACE("%s: AdminOwnerSet() Restart", __FUNCTION__);
        break;
      }
    }

    // Add the object create request to the ccb:
    recovery_info = AddCreate(create_descriptor);
    if (recovery_info == kFail) {
      LOG_NO("%s: AddCreate() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: AddCreate() Restart", __FUNCTION__);
      break;
    }
  }  // For all create descriptors

  if (recovery_info == kNotSet) {
    // No creates are added, no recovery needed
    recovery_info = kContinue;
  }
  TRACE_LEAVE();
  return recovery_info;
}

// Add one create descriptor to the ccb
//  Set parent name and class name
//  Add all attributes
//  Add the create request to the ccb
// Return: Recovery information
int ModelModification::AddCreate(const CreateDescriptor& create_descriptor) {
  TRACE_ENTER();

  SaImmCcbHandleT ccb_handle = imm_ccb_handle_->GetHandle();
  int recovery_info = AddCreateToCcb(ccb_handle, create_descriptor);
  if (recovery_info == kFail) {
    ErrorInformation error_info;
    GetAddToCbbErrorInfo(&error_info);
    api_name_ = error_info.api_name;
    ais_error_ = error_info.ais_error;
  }

  TRACE_LEAVE();
  return recovery_info;
}

// Add delete requests for all objects to be deleted
// Set admin ownership for objects to be deleted with scope SA_IMM_SUBTREE.
// Add all delete operations to the CCB
// Note1: We have to be owner of the object(s) and the sub tree(s)
// Note2: It is not a problem to set admin ownership of an object that already
//        has an admin owner if the same admin owner name is used, meaning that
//        this is not needed to check
// Recovery:  BAD HANDLE; Restart CCB handling
//            FAILED OPERATION; Restart CCB handling if resource abort
// Return:  Recovery information
int ModelModification::AddDeletes(const std::vector<DeleteDescriptor>&
                                  delete_descriptors) {
  TRACE_ENTER();
  int recovery_info = kNotSet;

  // Do this for all delete descriptors
  for (auto& delete_descriptor : delete_descriptors) {
    if (delete_descriptor.object_name.empty() == false) {
      // Become admin owner of object if there is an object. If not Fail
      std::vector<std::string> imm_objects;
      imm_objects.push_back(delete_descriptor.object_name);
      recovery_info = AdminOwnerSet(imm_objects, SA_IMM_SUBTREE);
      if (recovery_info == kFail) {
        if ((ais_error_ == SA_AIS_ERR_NOT_EXIST) &&
            (delete_descriptor.ignore_ais_err_not_exist)) {
          // Ignore this error if not exist and ignore_ais_err_not_exist = true
          recovery_info = kContinue;
          TRACE("%s: Ignoring '%s'", __FUNCTION__,
                 saf_error(ais_error_));
        } else {
          LOG_NO("%s: AdminOwnerSet() Fail", __FUNCTION__);
          break;
        }
      } else if (recovery_info == kRestartOm) {
        TRACE("%s: AdminOwnerSet() Restart", __FUNCTION__);
        break;
      }
    } else {
      LOG_NO("%s: AddDeletes() Fail, Object name is missing",
             __FUNCTION__);
      recovery_info = kFail;
      api_name_ = "";
      ais_error_ = SA_AIS_OK;  // Not ais error
      break;
    }

    // Add the object delete request to the ccb:
    recovery_info = AddDelete(delete_descriptor);
    if (recovery_info == kFail) {
      LOG_NO("%s: AddDelete() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: AddDelete() Restart", __FUNCTION__);
      break;
    }
  }  // For all delete descriptors

  if (recovery_info == kNotSet) {
    // No deletes are added, no recovery needed
    recovery_info = kContinue;
  }
  TRACE_LEAVE();
  return recovery_info;
}

// Add one delete descriptor to the ccb
//  Set object name
//  Add the delete request to the ccb
// Return: Recovery information
int ModelModification::AddDelete(const DeleteDescriptor& delete_descriptor) {
  TRACE_ENTER();

  SaImmCcbHandleT ccb_handle = imm_ccb_handle_->GetHandle();
  int recovery_info = AddDeleteToCcb(ccb_handle, delete_descriptor);
  if (recovery_info == kFail) {
    ErrorInformation error_info;
    GetAddToCbbErrorInfo(&error_info);
    api_name_ = error_info.api_name;
    ais_error_ = error_info.ais_error;
  }

  TRACE_LEAVE();
  return recovery_info;
}

// Add modify requests for all objects to be modified
// Set admin ownership for all objects to be modified with scope
// SA_IMM_ONE.
// Add all modify operations to the CCB
// Note1: We have to be owner of the object(s)
// Note2: It is not a problem to set admin ownership of an object that already
//        has an admin owner if the same admin owner name is used, meaning that
//        this is not needed to check
// Recovery:  BAD HANDLE; Restart CCB handling
//            FAILED OPERATION; Restart CCB handling if resource abort
// Return:  Recovery information
int ModelModification::AddModifies(const std::vector<ModifyDescriptor>&
                                   modify_descriptors) {
  TRACE_ENTER();
  int recovery_info = kNotSet;

  // Do this for all modify descriptors
  for (auto& modify_descriptor : modify_descriptors) {
    if (modify_descriptor.object_name.empty() == false) {
      // Become admin owner of object if there is an object. If not Fail
      std::vector<std::string> imm_objects;
      imm_objects.push_back(modify_descriptor.object_name);
      recovery_info = AdminOwnerSet(imm_objects, SA_IMM_ONE);
      if (recovery_info == kFail) {
        LOG_NO("%s: AdminOwnerSet() Fail", __FUNCTION__);
        break;
      } else if (recovery_info == kRestartOm) {
        TRACE("%s: AdminOwnerSet() Restart", __FUNCTION__);
        break;
      }
    } else {
      LOG_NO("%s: AddModifies() Fail, Object name is missing",
             __FUNCTION__);
      recovery_info = kFail;
      break;
    }

    // Add the object modify request to the ccb:
    recovery_info = AddModify(modify_descriptor);
    if (recovery_info == kFail) {
      LOG_NO("%s: AddModify() Fail", __FUNCTION__);
      break;
    } else if (recovery_info == kRestartOm) {
      TRACE("%s: AddModify() Restart", __FUNCTION__);
      break;
    }
  }  // For all modify descriptors

  if (recovery_info == kNotSet) {
    // All deletes are added, no recovery needed
    recovery_info = kContinue;
  }
  TRACE_LEAVE();
  return recovery_info;
}

// Add one modify descriptor to the ccb
//  Set object name
//  Add the modify request to the ccb.
//    The modify request can only be for one object but may contain
//    modifications for several attributes
int ModelModification::AddModify(const ModifyDescriptor& modify_descriptor) {
  TRACE_ENTER();

  SaImmCcbHandleT ccb_handle = imm_ccb_handle_->GetHandle();
  int recovery_info = AddModifyToCcb(ccb_handle, modify_descriptor);
  if (recovery_info == kFail) {
    ErrorInformation error_info;
    GetAddToCbbErrorInfo(&error_info);
    api_name_ = error_info.api_name;
    ais_error_ = error_info.ais_error;
  }

  TRACE_LEAVE();
  return recovery_info;
}

// Apply the CCB. Based on a valid CCB handle
// Recovery:  BAD HANDLE; Restart CCBhandling
//            FAILED OPERATION; Restart CCBhandling
int ModelModification::ApplyModifications() {
  TRACE_ENTER();
  int recovery_info = kNotSet;

  if (imm_ccb_handle_->ApplyCcb() == false) {
    SaAisErrorT ais_rc = imm_ccb_handle_->ais_error();
    if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
      TRACE("%s: ApplyCcb() Restart %s", __FUNCTION__,
            saf_error(ais_rc));
      recovery_info = kRestartOm;
    } else if (ais_rc == SA_AIS_ERR_FAILED_OPERATION) {
        if (IsResorceAbort(imm_ccb_handle_->GetHandle())) {
          TRACE("%s: ApplyCcb() Restart %s", __FUNCTION__,
                saf_error(ais_rc));
          recovery_info = kRestartOm;
        } else {
          LOG_NO("%s: ApplyCcb() Fail %s", __FUNCTION__,
                 saf_error(ais_rc));
          recovery_info = kFail;
          api_name_ = "saImmOmCcbApply";
          ais_error_ = ais_rc;
        }
    } else {
      LOG_ER("%s: ApplyCcb() Fail", __FUNCTION__);
      recovery_info = kFail;
      api_name_ = "saImmOmCcbApply";
      ais_error_ = ais_rc;
    }
  } else {
    TRACE("%s: CCB is applied", __FUNCTION__);
    recovery_info = kContinue;
  }
  TRACE_LEAVE();
  return recovery_info;
}

}  // namespace modelmodify
