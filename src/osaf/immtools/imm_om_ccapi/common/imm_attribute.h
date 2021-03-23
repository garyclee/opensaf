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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_COMMON_IMM_ATTRIBUTE_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_COMMON_IMM_ATTRIBUTE_H_

#include <string.h>
#include <string>
#include <vector>
#include "ais/include/saImm.h"
#include "osaf/immtools/imm_om_ccapi/common/common.h"

//>
// These below 03 classes represent C IMM data structures
// SaImmAttrDefinitionT_2, SaImmSearchOneAttrT_2, SaImmAttrValuesT_2
// and SaImmAttrModificationT_2, SaImmAdminOperationParamsT_2.
//
// In these C IMM data structures, lot of duplicated fields such as
// attrName, attrValueType, attrValuesNumber, etc. By introducing
// these 03 classes, that duplicated data are removed.
//
// Here is simple class diagram, showing relationship among them.
//                      +---------------------+
//                      |  AttributeProperty  |
//                      +---------+-----------+
//                                |
//                                |
//                                |
//            +-------------------+-----------------+
//            |                                     |
//            |                                     |
// +----------+------------+           +------------+----------+
// | AttributeModification |           | AttributeDefinition   |
// +-----------------------+           +-----------------------+
//
// AttributeProperty class holds generic information of IMM attribute, include
// attribute name, attribute value type, attribute value(s) and number
// of attribute values. Besides, AttributeProperty class provides help methods
// to convert itself to other C IMM data structures it represent for.
//
// AttributeModification and AttributeModification class represent
// SaImmAttrDefinitionT_2, and SaImmAttrModificationT_2 respectively.
// They also have help methods to convert themself to C IMM data structures.
//
// Example:
// AttributeProperty* attribute = new AttributeProperty(name);
// attribute->set_value<SaUint32T>(value);
//
// SaImmAttrValuesT_2 immattr;
// attribute->FormAttrValuesT_2(&immattr);
//<
class AttributeProperty {
 public:
  explicit AttributeProperty(const std::string& name)
      : attribute_name_{name},
        attribute_values_pointers_{nullptr},
        num_of_values_{0},
        list_ptr_to_cpp_strings_{nullptr} {}

  virtual ~AttributeProperty() {
    for (auto& item : list_ptr_to_cpp_strings_) {
      if (item != nullptr)  delete[] item;
    }
    FreeMemory();
  }

  template <typename T>
  AttributeProperty& set_value(T* ptr_to_value) {
    attribute_type_ = ImmBase::GetAttributeValueType<T>();
    if (ptr_to_value != nullptr) {
      attribute_values_pointers_ = reinterpret_cast<void**>(new T*[1]());
      assert(attribute_values_pointers_ != nullptr);
      attribute_values_pointers_[0] = ptr_to_value;
      num_of_values_ = 1;
    }
    return *this;
  }

  template <typename T>
  AttributeProperty& set_value(const std::vector<T*>& list_of_ptr_to_values) {
    attribute_type_ = ImmBase::GetAttributeValueType<T>();
    if (list_of_ptr_to_values.empty() == false) {
      size_t size = list_of_ptr_to_values.size();
      attribute_values_pointers_ = reinterpret_cast<void**>(new T*[size]());
      assert(attribute_values_pointers_ != nullptr);
      unsigned i = 0;
      for (auto& ptr_to_value : list_of_ptr_to_values) {
        attribute_values_pointers_[i++] = ptr_to_value;
      }
      num_of_values_ = size;
    }
    return *this;
  }

  // Convert this class object to given `output` C IMM data structure
  void FormAttrValuesT_2(SaImmAttrValuesT_2* output) const;
  void FormSearchOneAttrT_2(SaImmSearchOneAttrT_2* output) const;
  void FormAdminOperationParamsT_2(SaImmAdminOperationParamsT_2* output) const;

 private:
  void FreeMemory();

 protected:
  std::string attribute_name_;
  void** attribute_values_pointers_;
  SaUint32T num_of_values_;
  SaImmValueTypeT attribute_type_;
  // Introduce this attribute to deal with typename T = std::string.
  // Each element holds C string which is returned by std::string.c_str();
  std::vector<char*> list_ptr_to_cpp_strings_;
};

class AttributeDefinition : public AttributeProperty {
 public:
  explicit AttributeDefinition(const std::string& name,
                               SaImmAttrFlagsT flags)
      : AttributeProperty{name}, attribute_flags_{flags} {}

  ~AttributeDefinition() {}

  // Convert this class object to given `output` C IMM data structure
  void FormAttrDefinitionT_2(SaImmAttrDefinitionT_2* output) const;

 private:
  SaImmAttrFlagsT attribute_flags_;
};

class AttributeModification : public AttributeProperty {
 public:
  explicit AttributeModification(const std::string& name,
                                 SaImmAttrModificationTypeT type)
      : AttributeProperty{name}, modification_type_{type} {}

  ~AttributeModification() {}

  // Convert this class object to given `output` C IMM data structure
  void FormAttrModificationT_2(SaImmAttrModificationT_2* output) const;

 private:
  SaImmAttrModificationTypeT modification_type_;
};

template<> inline AttributeProperty&
AttributeProperty::set_value<std::string>(std::string* ptr_to_value) {
  attribute_type_ = ImmBase::GetAttributeValueType<std::string>();
  if (ptr_to_value != nullptr) {
    size_t size = ptr_to_value->length();
    attribute_values_pointers_ = reinterpret_cast<void**>(new char**[1]());
    assert(attribute_values_pointers_ != nullptr);
    num_of_values_ = 1;

    // Trick to deal with typename std::string
    list_ptr_to_cpp_strings_.resize(1);
    list_ptr_to_cpp_strings_[0] = new char[size + 1]();
    assert(list_ptr_to_cpp_strings_[0] != nullptr);
    memcpy(list_ptr_to_cpp_strings_[0], ptr_to_value->c_str(), size);
    attribute_values_pointers_[0] = &list_ptr_to_cpp_strings_[0];
  }
  return *this;
}

template<> inline AttributeProperty&
AttributeProperty::set_value<std::string>(
    const std::vector<std::string*>& list_of_ptr_to_values) {
  attribute_type_ = ImmBase::GetAttributeValueType<std::string>();
  if (list_of_ptr_to_values.empty() == false) {
    size_t size = list_of_ptr_to_values.size();
    attribute_values_pointers_ = reinterpret_cast<void**>(new char**[size]());
    assert(attribute_values_pointers_ != nullptr);
    num_of_values_ = size;

    list_ptr_to_cpp_strings_.resize(size);
    unsigned i = 0;
    for (auto& ptr_to_value : list_of_ptr_to_values) {
      size_t ssize = ptr_to_value->length();
      list_ptr_to_cpp_strings_[i] = new char[ssize + 1]();
      assert(list_ptr_to_cpp_strings_[i] != nullptr);
      memcpy(list_ptr_to_cpp_strings_[i], ptr_to_value->c_str(), ssize);
      attribute_values_pointers_[i] = &list_ptr_to_cpp_strings_[i];
      i++;
    }
  }
  return *this;
}

template <> inline AttributeProperty&
AttributeProperty::set_value<CppSaTimeT>(CppSaTimeT* ptr_to_value) {
  attribute_type_ = ImmBase::GetAttributeValueType<CppSaTimeT>();
  if (ptr_to_value != nullptr) {
    attribute_values_pointers_ = reinterpret_cast<void**>(new SaTimeT*[1]());
    assert(attribute_values_pointers_ != nullptr);
    attribute_values_pointers_[0] = &(ptr_to_value->time);
    num_of_values_ = 1;
  }
  return *this;
}

template <> inline AttributeProperty&
AttributeProperty::set_value<CppSaTimeT>(
    const std::vector<CppSaTimeT*>& list_of_ptr_to_values) {
  attribute_type_ = ImmBase::GetAttributeValueType<CppSaTimeT>();
  if (list_of_ptr_to_values.empty() == false) {
    size_t size = list_of_ptr_to_values.size();
    attribute_values_pointers_ = reinterpret_cast<void**>(new SaTimeT*[size]());
    assert(attribute_values_pointers_ != nullptr);
    unsigned i = 0;
    for (auto& ptr_to_value : list_of_ptr_to_values) {
      attribute_values_pointers_[i++] = &(ptr_to_value->time);
    }
    num_of_values_ = size;
  }
  return *this;
}

// For internal use only
using ListOfAttributePropertyPtr     = std::vector<AttributeProperty*>;
using ListOfAttributeModificationPtr = std::vector<AttributeModification*>;
using ListOfAttributeDefinitionPtr   = std::vector<AttributeDefinition*>;

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_COMMON_IMM_ATTRIBUTE_H_
