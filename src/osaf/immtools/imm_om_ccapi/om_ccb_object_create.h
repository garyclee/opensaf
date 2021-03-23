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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_CREATE_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_CREATE_H_

#include <string.h>
#include <string>
#include <vector>

#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "common/imm_attribute.h"
#include "base/logtrace.h"

namespace immom {

//>
// Add an IMM configuration object create request to a CCB
//
// For information about naming of objects, see IMM AIS
//
// If a parent name (dn) is given the object will be constructed as a child of
// that object. If no parent is given the object will be constructed as a top
// level object.
//
// Adding an object to the CCB is done in two steps.
// 1. The object is defined. The Set methods are used for this
// 2. The object is added to the CCB. The CreateObject() method is used
//
// Note:
// - The name of the class for the object must always be given.
//   Use SetClassName() method
// - An object must always be given a rdn meaning that the attribute tagged as
//   <rdn> in the class description must be given a value.
// - All attributes having a SA_INITIALIZED flag must be given a valid value.
//   The validation is done by the OI
// - The creation is only performed when the CCB is applied.
//
// Example:
// Define an object of class SaLogStreamConfig, add it to a CCB
// and commit the modification. Note that more modification requests can be
// added to the CCB before committing.
//
// The  <rdn> "safLgStrCfg" shall be given the name "safLgStrCfg=myLogStream"
// which is the rdn part of the object name.
// The saLogStreamFileName and saLogStreamPathName attributes have the
// SA_INITIALIZED flag and must be given a value.
//
// ImmOmCcbHandle ccbhandleobj{adminhandle};
// SaImmCcbHandleT ccbhandle = ccbhandleobj.GetHandle();
//
// ImmOmCcbObjectCreate createobj{ccbhandle, "SaLogStreamConfig"};
// createobj.SetClassName("SaLogStreamConfig");
// createobj.SetParentName("safApp=safLogService");
//
// SaStringT objectName = "safLgStrCfg=myLogStream";
// SaStringT fileName = "myStream";
// SaStringT relativePath = ".";
// createobj.SetAttributeValue("safLgStrCfg", &objectName);
// createobj.SetAttributeValue("saLogStreamFileName",&fileName);
// createobj.SetAttributeValue("saLogStreamPathName",&relativePath);
//
// if (createobj.AddObjectCreateToCcb() == false) {
//   // Error handling...
//   return;  // No meaning to try to apply the CCB
// }
//
// if (ccbhandleobj.ApplyCcb() == false) {
//   // Error handling...
// }
//<
class ImmOmCcbObjectCreate : public ImmBase {
 public:
  explicit ImmOmCcbObjectCreate(const SaImmCcbHandleT& ccb_handle);
  ~ImmOmCcbObjectCreate();

  // Set a new CCB handle. Needed when recovering from BAD_HANDLE
  void UpdateHandle(const SaImmCcbHandleT& ccb_handle) {
    ccb_handle_ = ccb_handle;
  }

  // If this method is not called the object will be created as a top level
  // object
  ImmOmCcbObjectCreate& SetParentName(const std::string& parent_object);

  // Set class for object to be created. Is mandatory
  void SetClassName(const std::string& class_name) { class_name_ = class_name; }

  // Set value/multiple values for given attribute name.
  // T must be AIS data types or std::string.
  // NOTE: must give explicitly typename T if passing NULL value.
  // Example:
  // SaConstStringT value = "hello";
  // SetAttributeValue("name", &value);
  // SaUint64T val1 = 1000, val2 = 2000;
  // const std::vector<SaUint64T*> vvalue{&val1, &val2};
  // SetAttributeValue("name", vvalue);
  template<typename T> ImmOmCcbObjectCreate&
  SetAttributeValue(const std::string& name, T* ptr_to_value = nullptr);
  template<typename T> ImmOmCcbObjectCreate&
  SetAttributeValue(const std::string& name,
                    const std::vector<T*>& list_of_ptr_to_values);

  // Add creation request to the CCB
  // Return false if failing
  // Use ais_error() to get returned AIS code.
  bool AddObjectCreateToCcb();

 private:
  void FreeAllocatedMemory();

  std::string class_name_;
  std::string parent_object_;
  ListOfAttributePropertyPtr list_of_attribute_properties_;
  SaImmCcbHandleT ccb_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmCcbObjectCreate);
};

inline ImmOmCcbObjectCreate&
ImmOmCcbObjectCreate::SetParentName(const std::string& parent_object) {
  parent_object_ = parent_object;
  return *this;
}

template<typename T> ImmOmCcbObjectCreate&
ImmOmCcbObjectCreate::SetAttributeValue(
    const std::string& name, T* ptr_to_value) {
  TRACE_ENTER();
  // typename T must be one of AIS data types or std::string. Not support
  // SaInt8T, and SaInt16T types as no SaImmValueTypeT goes with them.
  SA_AIS_TYPE_CHECK(T);
  AttributeProperty* attribute = new AttributeProperty(name);
  assert(attribute != nullptr);
  attribute->set_value<T>(ptr_to_value);
  list_of_attribute_properties_.push_back(attribute);
  return *this;
}

template<typename T> ImmOmCcbObjectCreate&
ImmOmCcbObjectCreate::SetAttributeValue(
    const std::string& name, const std::vector<T*>& list_of_ptr_to_values) {
  TRACE_ENTER();
  // typename T must be one of AIS data types or std::string. Not support
  // SaInt8T, and SaInt16T types as no SaImmValueTypeT goes with them.
  SA_AIS_TYPE_CHECK(T);
  AttributeProperty* attribute = new AttributeProperty(name);
  assert(attribute != nullptr);
  attribute->set_value<T>(list_of_ptr_to_values);
  list_of_attribute_properties_.push_back(attribute);
  return *this;
}

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_CREATE_H_
