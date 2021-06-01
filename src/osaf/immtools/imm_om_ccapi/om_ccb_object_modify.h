/*      -*- OpenSAF  -*-
 *
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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_MODIFY_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_MODIFY_H_


#include <string>
#include <vector>

#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "common/imm_attribute.h"
#include "base/logtrace.h"

namespace immom {

//>
// Add an attribute value change (modify) request for an IMM configuration
// object to a CCB
//
// A value can be deleted, changed or added
//
// Example:
// In object "safLgStrCfg=myLogStream", replace the value of attribute
// "filesize" and the values in multi-value attribute "saDestination", add to
// CCB and commit the modification.
//
// ImmOmCcbHandle ccbhandleobj{adminhandle};
// SaImmCcbHandleT ccbhandle = ccbhandleobj.GetHandle();
//
// ImmOmCcbObjectModify updateobj{ccbhandle, "safLgStrCfg=myLogStream"};
//
// SaUint64T filesize = 1000;
// updateobj.ReplaceAttributeValue("filesize", &filesize);
//
// SaConstStringT dest1 = "dest1";
// SaConstStringT dest2 = "dest2";
// std::vector<SaConstStringT> dest = {&dest1, &dest2};
// updateobj.ReplaceAttributeValue("saDestination", dest);
//
// if (updateobj.UpdateObject() == false) {
//   // Error handling...
//   return;
//}
//
// ccbhandleobj.ApplyCcb();
//<
class ImmOmCcbObjectModify : public ImmBase {
 public:
  // Construct the object with given object name
  explicit ImmOmCcbObjectModify(const SaImmCcbHandleT& ccb_handle,
                                const std::string& object_name);
  ~ImmOmCcbObjectModify();

  // Set a new CCB handle. Needed when recovering from BAD_HANDLE
  void UpdateHandle(const SaImmCcbHandleT& ccb_handle) {
    ccb_handle_ = ccb_handle;
  }

  // Replace all existing values with a new set of values
  // T must be AIS data types or std::string.
  // NOTE: must explicitly give typename T if passing NULL value.
  // Example: If an attribute has three values 1, 2 and 3 and the new set is
  // 5, 6 the attribute will after the modification have two values 5 and 6
  template<typename T> ImmOmCcbObjectModify&
  ReplaceAttributeValue(const std::string& name, T* ptr_to_value = nullptr);
  template<typename T> ImmOmCcbObjectModify&
  ReplaceAttributeValue(const std::string& name,
                        const std::vector<T*>& list_of_ptr_to_values);

  // Delete a specified value(s). The value must be an exact match. If no match
  // nothing will happen.
  template<typename T> ImmOmCcbObjectModify&
  DeleteAttributeValue(const std::string& name, T* ptr_to_value);
  template<typename T> ImmOmCcbObjectModify&
  DeleteAttributeValue(const std::string& name,
                       const std::vector<T*>& list_of_ptr_to_values);

  // Adds a new value to a multi value attribute or adds a value to a non
  // multi-value attribute if it has no value <empty>
  template<typename T> ImmOmCcbObjectModify&
  AddAttributeValue(const std::string& name, T* ptr_to_value);
  template<typename T> ImmOmCcbObjectModify&
  AddAttributeValue(const std::string& name,
                    const std::vector<T*>& list_of_ptr_to_values);

  // Returns false if fail.
  // Use ais_error() to get returned AIS code.
  bool AddObjectModifyToCcb();

 private:
  template<typename T> ImmOmCcbObjectModify&
  ModifyAttributeValue(const std::string&,
                       const std::vector<T*>&,
                       SaImmAttrModificationTypeT);
  void FreeAllocatedMemory();

 private:
  std::string object_name_;
  ListOfAttributeModificationPtr list_of_attribute_modifications_;
  SaImmCcbHandleT ccb_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmCcbObjectModify);
};


template<typename T>
ImmOmCcbObjectModify& ImmOmCcbObjectModify::ModifyAttributeValue(
    const std::string& name,
    const std::vector<T*>& list_of_ptr_to_values,
    SaImmAttrModificationTypeT type) {
  TRACE_ENTER();
  // typename T must be one of AIS data types or std::string. Not support
  // SaInt8T, and SaInt16T types as no SaImmValueTypeT goes with them.
  SA_AIS_TYPE_CHECK(T);
  AttributeModification* attribute = new AttributeModification(name, type);
  assert(attribute != nullptr);
  attribute->set_value<T>(list_of_ptr_to_values);
  list_of_attribute_modifications_.push_back(attribute);
  return *this;
}

template<typename T>
ImmOmCcbObjectModify& ImmOmCcbObjectModify::ReplaceAttributeValue(
    const std::string& name, const std::vector<T*>& list_of_ptr_to_values) {
  return ModifyAttributeValue<T>(name, list_of_ptr_to_values,
                                 SA_IMM_ATTR_VALUES_REPLACE);
}

template<typename T>
ImmOmCcbObjectModify& ImmOmCcbObjectModify::ReplaceAttributeValue(
    const std::string& name, T* ptr_to_value) {
  if (ptr_to_value != nullptr) {
    return ReplaceAttributeValue<T>(name, std::vector<T*>{ptr_to_value});
  } else {
    return ReplaceAttributeValue<T>(name, std::vector<T*>{});
  }
}

template<typename T>
ImmOmCcbObjectModify& ImmOmCcbObjectModify::AddAttributeValue(
    const std::string& name, const std::vector<T*>& list_of_ptr_to_values) {
  return ModifyAttributeValue<T>(name, list_of_ptr_to_values,
                                 SA_IMM_ATTR_VALUES_ADD);
}

template<typename T>
ImmOmCcbObjectModify& ImmOmCcbObjectModify::AddAttributeValue(
    const std::string& name, T* ptr_to_value) {
  if (ptr_to_value != nullptr) {
    return AddAttributeValue<T>(name, std::vector<T*>{ptr_to_value});
  } else {
    return AddAttributeValue<T>(name, std::vector<T*>{});
  }
}


template<typename T>
ImmOmCcbObjectModify& ImmOmCcbObjectModify::DeleteAttributeValue(
    const std::string& name, const std::vector<T*>& list_of_ptr_to_values) {
  return ModifyAttributeValue<T>(name, list_of_ptr_to_values,
                                 SA_IMM_ATTR_VALUES_DELETE);
}

template<typename T>
ImmOmCcbObjectModify& ImmOmCcbObjectModify::DeleteAttributeValue(
    const std::string& name, T* ptr_to_value) {
  if (ptr_to_value != nullptr) {
    return DeleteAttributeValue<T>(name, std::vector<T*>{ptr_to_value});
  } else {
    return DeleteAttributeValue<T>(name, std::vector<T*>{});
  }
}

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_MODIFY_H_
