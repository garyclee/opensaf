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

#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <algorithm>

#include "ais/include/saImm.h"
#include "ais/include/saAis.h"
#include "base/osaf_extended_name.h"

#include "smf/smfd/imm_om_ccapi/common/common.h"
#include "smf/smfd/imm_om_ccapi/om_handle.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_handle.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_handle.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_create.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_set.h"

#ifndef SMF_SMFD_IMM_MODIFY_CONFIG_IMMCCB_H_
#define SMF_SMFD_IMM_MODIFY_CONFIG_IMMCCB_H_

/* Handle modifications in the IMM model
 *
 * USER-HANDLING
 * To request a modification or a set of modifications the following has to be
 * done:
 * 1. Describe a modification. This is done by filling in a modification object
 *    Possible modifications are:
 *    - Create an object
 *    - Delete an object
 *    - Modify attribute(s) of an existing object
 * 2. Add the modification to the modification list
 *    Only modifications that can be done in the same CCB can be added
 * 3. Repeat 1 and 2 for all wanted modifications
 * 4. Request modifications to be done. The modification list is given as input
 *    This is the step where all IMM handling is done and where the result is
 *    reported (Success or Fail)
 *
 * MODIFICATIONs are done in three steps:
 * 1. Initialize the IMM OM API and become admin owner
 * 2. Create an IMM CCB containing all modifications to be done.
 *    In this step admin ownership is set for objects to be modified
 *    All information needed to create the CCB is stored to make it possible to
 *    try again if applying the CCB fails but a retry is valid to do.
 * 3. Apply the CCB and clear admin ownership
 *
 * RETRY handling is done in three levels:
 * 1. For each IMM API. This is mainly try again loops
 * 2. For sequences of APIs. The main reason is if the OM handle has to be
 *    recreated (SA_AIS_ERR_BAD_HANDLE) however there are some other
 *    recoverable problems as well
 * 3. For failing to apply a CCB if the reason is not un-recoverable e.g.
 *    a validation error
 *
 * ERROR handling:
 * The execute API returns a boolean where false means that the modification
 * failed. All non recoverable internal fails are logged to the syslog. The user
 * should log a fail in order to track where the modification was requested but
 * does not have to log any detailed information.
 * Example of user logging:
 * LOG_NO("%s: DoModification() Fail", __FUNCTION__);
 *
 * Possible reasons for a fail:
 *  * An IMM API has returned an unrecoverable error. This includes try again
 *    timeout
 *  * Validation error
 *  * Other unrecoverable error from an Object Implementer
 *  * All CCB apply retries has failed
 *  * A general timeout for the whole modify sequence has happened before
 *    the CCB was applied successfully
 */

namespace modelmodify {

// Functions for converting some SAF sepcific types to string. For numeric
// values the STL to_string can be used
// -----------------------------------------------------------------------
// Convert SaNameT to a std::string to be used with an AttributeDescriptor
// Note: Both types may contain data that cannot be read as text string but
//       their data can still be contained in a std::string object
//       Shall be used with AttributeDescriptor object
static inline std::string SaNametToString(SaNameT* name_value);

// Convert SaAnyT to a std::string to be used with an AttributeDescriptor
// Note: std::string uses type char (8 bit) for each data but SaAnyT data is
//       unsigned char. However in this case this does not matter. The
//       std::string will not be used as a "string"
static inline std::string SaAnytToString(SaAnyT* anyt_value);

// Use if the attribute type is given as a string.
// Note: The string shall correspond with the SaImmValueTypeT enum name
// Example: SA_IMM_ATTR_SAINT32T corresponds with "SA_IMM_ATTR_SAINT32T"
static inline SaImmValueTypeT StringToSaImmValueType(const std::string&
                                                        type_string);
// Use if the attribute modification type is given as a string.
// Note: The string shall correspond with the SaImmAttrModificationTypeT
// enum name.
// Example: SA_IMM_ATTR_VALUES_ADD corresponds with "SA_IMM_ATTR_VALUES_ADD"

static inline SaImmAttrModificationTypeT StringToImmAttrModType(const
                                          std::string& modification_type);

// AttributeDescriptor: Describes one attribute
// Note: Several attributes can be added to one modification
// An attribute always has a name and a type.
// An attribute value can be given as follows:
//  * One value of the given value_type
//  * No value at all. This means that the attribute will be given an "empty"
//    value
//  * Multiple values of the given value_type. This is allowed only if the
//    attribute is defined as a multi value attribute
// Used with IMM object creation and value modifications
// Note: Use <object name>.values_as_strings.clear() before adding any values
//       to remove any previous values if the object is reused
//
struct AttributeDescriptor {
  std::string attribute_name;
  SaImmValueTypeT value_type;
  std::vector<std::string> values_as_strings;

  void AddValue(std::string value_as_string) {
    values_as_strings.push_back(value_as_string);
    // To make it possible to compare
    std::sort(values_as_strings.begin(), values_as_strings.end());
  }

  bool operator==(const AttributeDescriptor& as) const {
    return ((as.attribute_name == attribute_name) &&
            (as.value_type == value_type) &&
            (as.values_as_strings == values_as_strings));
  }

  // Compares attribute name only
  bool operator<(const AttributeDescriptor& as) const {
    return attribute_name < as.attribute_name;
  }
};

// AttributeModifyDescriptor: Describes one attribute to modify
// The value(s) of an attribute can be modified in the following ways:
//  * ADD:      Add one or several value(s) to an attribute. Can be used if an
//              attribute is "empty" (has no value) or is a multi value
//              attribute.
//              - If it is a multi value attribute and the value already exists
//                nothing will be added
//              - If it is a single value attribute that is not "empty" nothing
//                will be added or changed
//  * DELETE:   Remove one or several value(s) from an attribute. If all values
//              are removed the attribute is considered to be "empty"
//  * REPLACE:  Replace all values with a new set of values. If the new set is
//              "empty" the attribute will be "empty".
//              - If the attribute is multi value all existing values will be
//                removed and replaced with the new set
//              - If the attribute is not multi value the new set shall consist
//                of one value
//
struct AttributeModifyDescriptor {
  // Type of modification
  SaImmAttrModificationTypeT modification_type;
  // The attribute to be modified and the new value(s)
  AttributeDescriptor attribute_descriptor;
};


// ========================
// Modification descriptors
//
// Structures to fill in to define one modification.
// Possible modifications are:
//  * Create an IMM object
//  * Delete an IMM object
//  * Modify the value(s) of one or more attribute(s) in an IMM object
// Note:  Several modifications can be added to one CCB.
//
// Some general notes:
// *  Object naming shall follow the IMM AIS rule:
//    Full DN:  "Object_RDN,Parent_Object_DN"
//    RDN part: "RDN_attribute_name=RDN_attribute_value"
// *  Some IMM types are used in the structures. They can be found in saImm.h
// =====================

// CreateDescriptor: Create one object
// Note:  The name of the object to be created must always be given.
//        This is done using an AttributeDescriptor:
//          * The attribute name is the rdn attribute defined in the IMM class
//          * The value type is SA_IMM_ATTR_SASTRINGT
//          * The value is the RDN part of the object name
//
struct CreateDescriptor {
  CreateDescriptor() : ignore_ais_err_exist(false) { }
  // By default a SA_AIS_ERR_EXIST when trying to add a create operation to a
  // CCB is considered to be an unrecoverable error and will Fail the
  // DoModelModification() request. If this flag is set to true this error will
  // be ignored. The default value for this flag is false.
  bool ignore_ais_err_exist;
  // The name of the class that defines the object to be created
  std::string class_name;
  // The full DN of the object that is parent to the object to be created
  // If empty (no parent) a root object will be created
  std::string parent_name;
  // Attributes to be given values at object creation
  std::vector<AttributeDescriptor> attributes;
  void AddAttribute(AttributeDescriptor one_attribute) {
    attributes.push_back(one_attribute);
    // To make it possible to compare (sorted on attribute name)
    std::sort(attributes.begin(), attributes.end());
  }

  // Compare all variables. All equal is true.
  // Note: The vectors are compared using vector operator ==. This means that
  // not only the content must be the same but also the order.
  // TBD() could be handled by sorting
  bool operator==(const CreateDescriptor& cd) {
    return ((cd.ignore_ais_err_exist == ignore_ais_err_exist) &&
            (cd.class_name == class_name) &&
            (cd.parent_name == parent_name) &&
            (cd.attributes == attributes));
  }
};

// DeleteDescriptor: Delete one object
//
struct DeleteDescriptor {
  DeleteDescriptor() : ignore_ais_err_not_exist(true) { }
  // By default a SA_AIS_ERR_NOT_EXIST when trying to add a delete operation
  // to a CCB is not considered to be an unrecoverable error and will be
  // ignored If this flag is set to false this error will be considered an
  // unrecoverable error. The default value for this flag is true.
  bool ignore_ais_err_not_exist;
  // Full DN of the object to be deleted
  std::string object_name;
};

// ModifyDescriptor: Modify value(s) of one or several attributes in one object
// Note: Use only one ModifyDescriptor per object in the same CCB
//
struct ModifyDescriptor {
  // Full DN of the object to be modified
  std::string object_name;
  std::vector<AttributeModifyDescriptor> modifications;
  void AddAttributeModification(AttributeModifyDescriptor one_modification) {
    modifications.push_back(one_modification);
  }
};

// ==============
// CCB descriptor

// CcbDescriptor: Holds information about a complete CCB to create and apply
// Here all modifications shall be added. Is used as in-parameter for the
// modification request
//
struct CcbDescriptor {
  std::vector<CreateDescriptor> create_descriptors;
  std::vector<DeleteDescriptor> delete_descriptors;
  std::vector<ModifyDescriptor> modify_descriptors;

  // Use these methods to add the modifications

  // Duplicate descriptors cannot be added. If a duplicate
  // create descriptor is given as input it will not be added.
  // Returns false if duplicate
  bool AddCreate(CreateDescriptor new_create_descriptor) {
    bool is_added = true;
    for (auto& stored_create_descriptor : create_descriptors) {
      if (stored_create_descriptor == new_create_descriptor) {
        is_added = false;
        break;
      }
    }
    if (is_added == true)
      create_descriptors.push_back(new_create_descriptor);
    return is_added;
  }

  void AddDelete(DeleteDescriptor new_delete_descriptor) {
    delete_descriptors.push_back(new_delete_descriptor);
  }
  void AddModify(ModifyDescriptor new_modify_descriptor) {
    modify_descriptors.push_back(new_modify_descriptor);
  }
};

// =================
// Error information

// ErrorInformation: Output data.
// Used with GetErrorInformation() to get AIS error information if
// DoModelModification() returns false (Fail). No relevant information if not
// Fail
// In some cases immccb may Fail for other reasons than an AIS error. This
// includes some internal errors and timeouts caused by environmental problems.
// Such timeouts can be:
//  - Timeout when trying to become admin owner of an object
//    (some other admin owner does not release the resource in time)
//  - Timeout when trying to add a delete or modification to a CCB
//  - Timeout of the whole modification handling
// In this case api_name will be empty and ais_error is not relevant.
// Information about the problem will be logged to syslog.
//
struct ErrorInformation {
  // Name of the AIS API that returned an unrecoverable error causing the Fail
  std::string api_name;
  // AIS return code in case of fail based on AIS return code
  SaAisErrorT ais_error;
};

// Internal recovery information
// -----------------------------
// An operation may Fail and no recovery is possible but it may also fail
// but some recovery action is possible e.g. bad handle meaning that recovery
// is possible by restarting the API sequence.
// No recovery information is given
const int kNotSet = 0;
// No recovery needed, continue with next step
const int kContinue = 1;
// Restart sequence from Create OM Handle
const int kRestartOm = 2;
// Fail, no recovery possible
const int kFail = 3;

// Internal timeout and wait time for BUSY return from IMM
const uint64_t kBusyTimeout = 30000;  // 30 sec
const uint64_t kBusyWait = 2000;  // 2 sec
// Internal timeout and wait time for EXIST return from IMM
const uint64_t kExistTimeout = 30000;  // 30 sec
const uint64_t kExistWait = 2000;  // 2 sec

// Recovery code to text
// Used with log and trace
inline static const char* RecoveryTxt(int recovery_info) {
  switch (recovery_info) {
    case modelmodify::kContinue:
      return "Continue";
    case modelmodify::kFail:
      return "Fail";
    case modelmodify::kNotSet:
      return "NotSet";
    case modelmodify::kRestartOm:
      return "RestartOm";
    default:
      return "Unknown recovery info";
  }
}

// Object modification
// -------------------
// Creates and applies a CCB with all described modifications
// Handles internally recovery actions based on AIS return values as described
// in AIS, IMM PR document and IMM README files
//
// Note1: The DoModification method is synchronous and may take a long time.
//        It may take up to one minute to get an OM handle. This is also the
//        case if the handle becomes invalid during the modify operation and
//        has to be re-created. However, this is not normally the case
// Note2: All IMM resources needed for the operation will be allocated
//        internally when needed and released before DoModification returns.
//
// Example: Create an IMM object Obj1 based on IMM class Class1. The rdn
//          attribute name is obj1Name and the object name (rdn) shall be
//          obj1=1. Parent is safApp=safSmfService. No other attributes will be
//          set at creation time
//
//  // 1. Fill in an attribute descriptor for the object name:
//  modelmodify::AttributeDescriptor object_name("obj1Name");
//  object_name.AddValue("obj1=1");
//
//  // 2. Fill in a create descriptor:
//  modelmodify::CreateDescriptor create_obj1;
//  create_obj1.class_name = "Class1";
//  create_obj1.parent_name = "safApp=safSmfService";
//  create_obj1.AddAttribute(object_name);
//
//  // 3. Fill in a CCB descriptor:
//  modelmodify::CcbDescriptor obj1_ccb;
//  obj1_ccb.AddCreate(create_obj1);
//
//  // 4. Create CCB according to the CCB descriptor and apply it:
//  modelmodify::ObjectModification obj1_creator;
//  if (obj1_creator.DoModification(obj1_ccb) == false) {
//    LOG_NO("%s: DoModification() Fail", __FUNCTION__);
//    // Do some other error handling...
//  }
//
class ModelModification {
 public:
  ModelModification();
  ~ModelModification();

  // Set CCB Flags
  // If flag SA_IMM_CCB_REGISTERED_OI is set then an Object Implementer must
  // validate the requested modifications, for more info see IMM AIS.
  // For SMF this is normally not the case so the default setting is to not
  // set this flag.
  void SetCcbFlags(SaImmCcbFlagsT ccb_flags) { ccb_flags_ = ccb_flags; }

  // Returns False if an unrecoverable problem occurs. This may for example be
  // a validation error
  bool DoModelModification(CcbDescriptor modifications);

  // Will fill in an ErrorInformation structure.
  // For more information see ErrorInformation structure
  void GetErrorInformation(ErrorInformation& error_info) {
    error_info.api_name = api_name_;
    error_info.ais_error = ais_error_;
  }

 private:
  void FinalizeHandles(void);
  int CreateHandles(void);
  int CreateObjectManager(void);
  int CreateAdminOwner(void); // Create a handle
  int CreateCcb(void);

  int AdminOwnerSet(const std::vector<std::string>& objects,
                    const SaImmScopeT scope);

  // Handle om_ccb_object_create()
  int AddCreates(const std::vector<CreateDescriptor>& create_descriptors);
  int AddCreate(const CreateDescriptor& create_descriptor);
  // Handle om_ccb_object_delete()
  int AddDeletes(const std::vector<DeleteDescriptor>& delete_descriptors);
  int AddDelete(const DeleteDescriptor& delete_descriptor);
  // Handle om_ccb_object_modify()
  int AddModifies(const std::vector<ModifyDescriptor>& modify_descriptors);
  int AddModify(const ModifyDescriptor& modify_descriptor);

  int ApplyModifications(void);

  const SaVersionT imm_version{'A', 2, 11};
  // Handle objects
  std::unique_ptr<immom::ImmOmHandle> imm_om_handle_;
  std::unique_ptr<immom::ImmOmCcbHandle> imm_ccb_handle_;
  std::unique_ptr<immom::ImmOmAdminOwnerHandle> imm_ao_handle_;
  std::unique_ptr<immom::ImmOmAdminOwnerSet> imm_ao_owner_set_;

  // Instance number for this class used to create a unique applier name
  // This is needed if instances are used in several threads
  // By using a unique name we can avoid taking admin ownership from some other
  // thread that has an ongoing modification
  // At construction time the static instance number is copied to the local
  // instance number and the global instance number is incremented for next
  // instance to use.
  static std::atomic<unsigned int> next_instance_number_;
  unsigned int instance_number_;
  std::string admin_owner_name_;

  CcbDescriptor ccb_descriptor_;
  // A list of all objects to become admin owner of
  std::vector<std::string> admin_owner_objects_;
  // Main timeout for DoModification()
  uint64_t modification_timeout_;
  SaImmCcbFlagsT ccb_flags_;

  // See struct ErrorInformation
  std::string api_name_;
  SaAisErrorT ais_error_;

  DELETE_COPY_AND_MOVE_OPERATORS(ModelModification);
};

static inline std::string SaNametToString(SaNameT* name_value) {
  std::string out_string;
  if (osaf_is_extended_name_empty(name_value)) {
    out_string = "";
  } else {
    SaConstStringT name_data = osaf_extended_name_borrow(name_value);
    out_string = name_data;
  }

  return out_string;
}

static inline std::string SaAnytToString(SaAnyT* anyt_value) {
  std::string out_string;
  if ((anyt_value->bufferSize == 0) || (anyt_value->bufferAddr == nullptr)) {
    out_string = "";
  } else {
    SaSizeT anyt_size = anyt_value->bufferSize;
    SaUint8T* anyt_buffer = anyt_value->bufferAddr;
    printf("  bufferAddr = %p\n", anyt_buffer);
    for (SaSizeT i = 0; i < anyt_size; i++) {
      out_string.push_back(static_cast<char>(*anyt_buffer++));
    }
  }

  return out_string;
}

static inline SaImmValueTypeT StringToSaImmValueType(const std::string&
                                                        value_type) {
  if (value_type.compare("SA_IMM_ATTR_SAINT32T") == 0) {
    return SA_IMM_ATTR_SAINT32T;

  } else if (value_type.compare("SA_IMM_ATTR_SAUINT32T") == 0) {
    return SA_IMM_ATTR_SAUINT32T;

  } else if (value_type.compare("SA_IMM_ATTR_SAINT64T") == 0) {
    return SA_IMM_ATTR_SAINT64T;

  } else if (value_type.compare("SA_IMM_ATTR_SAUINT64T") == 0) {
    return SA_IMM_ATTR_SAUINT64T;

  } else if (value_type.compare("SA_IMM_ATTR_SATIMET") == 0) {
    return SA_IMM_ATTR_SATIMET;

  } else if (value_type.compare("SA_IMM_ATTR_SANAMET") == 0) {
    return SA_IMM_ATTR_SANAMET;

  } else if (value_type.compare("SA_IMM_ATTR_SAFLOATT") == 0) {
    return SA_IMM_ATTR_SAFLOATT;

  } else if (value_type.compare("SA_IMM_ATTR_SADOUBLET") == 0) {
    return SA_IMM_ATTR_SADOUBLET;

  } else if (value_type.compare("SA_IMM_ATTR_SASTRINGT") == 0) {
    return SA_IMM_ATTR_SASTRINGT;

  } else if (value_type.compare("SA_IMM_ATTR_SAANYT") == 0) {
    return SA_IMM_ATTR_SAANYT;

  } else {
    LOG_ER("%s: Unknown value-type: '%s'", __FUNCTION__, value_type.c_str());
    assert(0);
  }
}

static inline SaImmAttrModificationTypeT StringToImmAttrModType(const
                                          std::string& modification_type) {
  if (modification_type.compare("SA_IMM_ATTR_VALUES_ADD") == 0)
    return SA_IMM_ATTR_VALUES_ADD;
  else if (modification_type.compare("SA_IMM_ATTR_VALUES_DELETE") == 0)
    return SA_IMM_ATTR_VALUES_DELETE;
  else if (modification_type.compare("SA_IMM_ATTR_VALUES_REPLACE") == 0)
    return SA_IMM_ATTR_VALUES_REPLACE;
  else {
    LOG_ER("%s: Unknown modification-type: %s", __FUNCTION__,
           modification_type.c_str());
    assert(0);
  }
}

}  // namespace modelmodify

#endif  // SMF_SMFD_IMM_MODIFY_CONFIG_IMMCCB_H_
