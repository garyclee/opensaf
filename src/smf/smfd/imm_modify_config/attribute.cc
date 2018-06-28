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
#include "smf/smfd/imm_modify_config/attribute.h"

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

#include "smf/smfd/imm_om_ccapi/common/common.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_create.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_modify.h"

namespace modelmodify {

// Convert a numeric string to a numeric value based on IMM type
// in:  - A vector with numeric strings
//      - An empty numeric vector of the given type to be filled in
//        The given type shall be the Saf type represented by the IMM type
//      - A numeric IMM type, SaImmValueTypeT
// out: - A filled in numeric vector
// return false on error. Type of error is logged in syslog
template<typename T>
static bool StringToNumericValue(const std::string& str_value,
                          T& num_value, const SaImmValueTypeT imm_type) {
  bool rc = true;

  try {
    switch (imm_type) {
      case SA_IMM_ATTR_SAINT32T: {
        num_value = std::stoi(str_value);
        break;
      }
      case SA_IMM_ATTR_SAUINT32T: {
        auto tmp_value = std::stoul(str_value);
        if (tmp_value > UINT_MAX) {
          throw std::out_of_range("Value > UINT_MAX");
        } else {
          num_value = tmp_value;
        }
        break;
      }
      case SA_IMM_ATTR_SAINT64T:
      case SA_IMM_ATTR_SATIMET: {
        num_value = std::stoll(str_value);
        break;
      }
      case SA_IMM_ATTR_SAUINT64T: {
        num_value = std::stoull(str_value);
        break;
      }
      case SA_IMM_ATTR_SAFLOATT: {
        num_value = std::stof(str_value);
        break;
      }
      case SA_IMM_ATTR_SADOUBLET: {
        num_value = std::stod(str_value);
        break;
      }
      default: {
        LOG_ER("%s: Unknown imm_type", __FUNCTION__);
        abort();
      }
    }
  } catch(std::invalid_argument& e) {
    LOG_NO("%s Fail: Invalid argument", __FUNCTION__);
    rc = false;
  } catch(std::out_of_range& e) {
    LOG_NO("%s Fail: Out of range", __FUNCTION__);
    rc = false;
  }

  return rc;
}

// ----------------------
// class AttributeHandler
// ----------------------

bool AttributeHandler::AddAttributesForObjectCreate(const CreateDescriptor&
                                     create_descriptor) {
  TRACE_ENTER();
  bool rc = true;
  for (auto& attribute_descriptor : create_descriptor.attributes) {
    rc = AddAttribute(attribute_descriptor, Request::kCreate);
    if (rc == false) {
      LOG_NO("%s: AddAttribute() Fail", __FUNCTION__);
      break;
    }
  }

  return rc;
}

bool AttributeHandler::AddAttributesForModification(const ModifyDescriptor&
                                                 modify_descriptor) {
  TRACE_ENTER();
  bool rc = true;
  for (auto& modification : modify_descriptor.modifications) {
    switch (modification.modification_type) {
      case SA_IMM_ATTR_VALUES_ADD:
        rc = AddAttribute(modification.attribute_descriptor,
                          Request::kModifyAdd);
        break;
      case SA_IMM_ATTR_VALUES_DELETE:
        rc = AddAttribute(modification.attribute_descriptor,
                          Request::kModifyDelete);
        break;
      case SA_IMM_ATTR_VALUES_REPLACE:
        rc = AddAttribute(modification.attribute_descriptor,
                          Request::kModifyReplace);
        break;
      default:
        LOG_NO("%s: Invalid modification_type", __FUNCTION__);
        rc = false;
        break;
    }
    // break out of the 'for' loop if a 'switch case' fails
    if (rc == false)
      break;
  }

  TRACE_LEAVE();
  return rc;
}

// Add one attribute to a creator or a modifier. Store the attribute and
// its values until this object goes out of scope
// The request holds information about what to do with the attribute
// Return false if fail
bool AttributeHandler::AddAttribute(const AttributeDescriptor& attribute,
                                    Request request) {
  TRACE_ENTER();

  bool rc = true;
  SaImmValueTypeT value_type = attribute.value_type;
  switch (value_type) {
    case SA_IMM_ATTR_SAINT32T:
      rc = StoreNumericAttribute<SaInt32T>(attribute, request);
      break;

    case SA_IMM_ATTR_SAUINT32T:
      rc = StoreNumericAttribute<SaUint32T>(attribute, request);
      break;

    case SA_IMM_ATTR_SAINT64T:
      rc = StoreNumericAttribute<SaInt64T>(attribute, request);
      break;

    case SA_IMM_ATTR_SAUINT64T:
      rc = StoreNumericAttribute<SaUint64T>(attribute, request);
      break;

    case SA_IMM_ATTR_SATIMET:
      rc = StoreNumericAttribute<CppSaTimeT>(attribute, request);
      // rc = StoreTimeTAttribute(attribute);
      break;

    case SA_IMM_ATTR_SAFLOATT:
      rc = StoreNumericAttribute<SaFloatT>(attribute, request);
      break;

    case SA_IMM_ATTR_SADOUBLET:
      rc = StoreNumericAttribute<SaDoubleT>(attribute, request);
      break;


    case SA_IMM_ATTR_SASTRINGT: {
      StoreStringAttribute(attribute, request);
      break;
    }

    case SA_IMM_ATTR_SANAMET:
      StoreSaNametAttribute(attribute, request);
      break;

    case SA_IMM_ATTR_SAANYT:
      StoreSaAnytAttribute(attribute, request);
      break;

    default:
      break;
  }

  TRACE_LEAVE();
  return rc;
}

// Convert all values as a string for an attribute to numeric values.
// Create a SetAttribute object and give that object the attribute name and its
// numeric values. The SetAttribute object will store the attribute and give
// the attribute name and values to a creator or a modifier
// Save the SetAttribute object in a vector so that the object will be in scope
// until "this" object goes out of scope which shall happen when the creator or
// modifier has given the operation to IMM

// All Store methods:
// For an attribute;
//  - Convert all Values As a String to its real type and save
//    those values in a temporary vector.
//  - Create a new SetAttribute object and give it the converted values
//  - Store the SetAttribute object
template<typename T>
bool AttributeHandler::
StoreNumericAttribute(const AttributeDescriptor& attribute, Request request) {
  TRACE_ENTER();
  bool rc = true;
  SaImmValueTypeT value_type = attribute.value_type;
  T numeric_value{0};
  std::vector<T> num_values;
  for (auto& value_str : attribute.values_as_strings) {
    // Create a vector containing all values for this attribute
    if (StringToNumericValue<T>
        (value_str, numeric_value, value_type) == false) {
      LOG_NO("%s Fail: name=%s, value=%s", __FUNCTION__,
             attribute.attribute_name.c_str(), value_str.c_str());
      rc = false;
      break;
    }
    num_values.push_back(numeric_value);
  }
  if (rc == true) {
    // Store the attribute give it to an attribute setter
    std::unique_ptr<SetAttribute> AnAttribute;
    if (request == Request::kCreate) {
      AnAttribute = std::unique_ptr<SetAttribute>(new SetAttribute(creator_));
    } else {
      AnAttribute =
          std::unique_ptr<SetAttribute>(new SetAttribute(modifier_, request));
    }
    AnAttribute->SetAttributeValues(attribute.attribute_name, num_values);
    set_attributesp_.push_back(std::move(AnAttribute));
  }

  TRACE_LEAVE();
  return rc;
}

void AttributeHandler::
StoreSaNametAttribute(const AttributeDescriptor& attribute, Request request) {
  TRACE_ENTER();
  SaNameT name_value;
  std::vector<SaNameT> name_values;
  for (auto& value_str : attribute.values_as_strings) {
    StringToSaNameT(value_str, &name_value);
    name_values.push_back(name_value);
  }

  // Store the attribute give it to an attribute setter
  std::unique_ptr<SetAttribute> AnAttribute;
  if (request == Request::kCreate) {
    AnAttribute = std::unique_ptr<SetAttribute>(new SetAttribute(creator_));
  } else {
    AnAttribute =
        std::unique_ptr<SetAttribute>(new SetAttribute(modifier_, request));
  }
  AnAttribute->SetAttributeValues(attribute.attribute_name, name_values);
  set_attributesp_.push_back(std::move(AnAttribute));
  TRACE_LEAVE();
}

void AttributeHandler::
StoreSaAnytAttribute(const AttributeDescriptor& attribute, Request request) {
  TRACE_ENTER();
  // Note: For this type it is also needed to store the buffer pointed to
  //       from the SaAnyT structure
  SaAnyT any_value;
  std::vector<SaAnyT> any_values;
  for (auto& value_str : attribute.values_as_strings) {
    StringToSaAnyT(value_str, &any_value);
    any_values.push_back(any_value);
  }

  // Store the attribute give it to an attribute setter
  std::unique_ptr<SetAttribute> AnAttribute;
  if (request == Request::kCreate) {
    AnAttribute = std::unique_ptr<SetAttribute>(new SetAttribute(creator_));
  } else {
    AnAttribute =
        std::unique_ptr<SetAttribute>(new SetAttribute(modifier_, request));
  }
  AnAttribute->SetAttributeValues(attribute.attribute_name, any_values);
  set_attributesp_.push_back(std::move(AnAttribute));
  TRACE_LEAVE();
}

void AttributeHandler::
StoreStringAttribute(const AttributeDescriptor& attribute, Request request) {
  TRACE_ENTER();
  // Store the attribute give it to an attribute setter
  std::unique_ptr<SetAttribute> AnAttribute;
  if (request == Request::kCreate) {
    AnAttribute = std::unique_ptr<SetAttribute>(new SetAttribute(creator_));
  } else {
    AnAttribute =
        std::unique_ptr<SetAttribute>(new SetAttribute(modifier_, request));
  }
  AnAttribute->SetAttributeValues(attribute.attribute_name,
                                  attribute.values_as_strings);
  set_attributesp_.push_back(std::move(AnAttribute));
  TRACE_LEAVE();
}

// ------------------
// class SetAttribute
// ------------------

// All SetAttributeValues methods:
// For each attribute type;
//  - Create a Value Store object
//  - Add the input values to the Value Store object value vector
//  - Add pointers to the values to the Value Store object value pointer vector
//  - Give the value pointer vector to a creator of class ImmOmCcbObjectCreate
//    that is previously created and stored in creator_

// SaAnyT
void SetAttribute::SetAttributeValues(const std::string& name,
                          const std::vector<SaAnyT>& any_values) {
  TRACE_ENTER();
  AnyValueStore_ =
      std::unique_ptr<AnyValueStore>(new AnyValueStore);
  for (auto& any_value : any_values) {
    AnyValueStore_->Values.push_back(any_value);
  }
  SaAnyT* v_ptr = AnyValueStore_->Values.data();
  for (size_t i = 0; i < AnyValueStore_->Values.size(); i++) {
    AnyValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, AnyValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, AnyValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, AnyValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, AnyValueStore_->Values_p);
  } else {
    LOG_NO("%s SaAnyT: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}


// SaNameT
void SetAttribute::SetAttributeValues(const std::string& name,
                          const std::vector<SaNameT>& name_values) {
  TRACE_ENTER();
  NameValueStore_ =
      std::unique_ptr<NameValueStore>(new NameValueStore);
  for (auto& name_value : name_values) {
    NameValueStore_->Values.push_back(name_value);
  }
  SaNameT* v_ptr = NameValueStore_->Values.data();
  for (size_t i = 0; i < NameValueStore_->Values.size(); i++) {
    NameValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, NameValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, NameValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, NameValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, NameValueStore_->Values_p);
  } else {
    LOG_NO("%s SaNameT: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}


// SaUint32T
void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<SaUint32T>& num_values) {
  TRACE_ENTER();
  Uint32ValueStore_ =
      std::unique_ptr<Uint32ValueStore>(new Uint32ValueStore);
  for (auto& num_value : num_values) {
    Uint32ValueStore_->Values.push_back(num_value);
  }
  SaUint32T* v_ptr = Uint32ValueStore_->Values.data();
  for (size_t i = 0; i < Uint32ValueStore_->Values.size(); i++) {
    Uint32ValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, Uint32ValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, Uint32ValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, Uint32ValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, Uint32ValueStore_->Values_p);
  } else {
    LOG_NO("%s SaUint32T: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

// SaInt32T
void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<SaInt32T>& num_values) {
  TRACE_ENTER();
  Int32ValueStore_ =
      std::unique_ptr<Int32ValueStore>(new Int32ValueStore);
  for (auto& num_value : num_values) {
    Int32ValueStore_->Values.push_back(num_value);
  }
  SaInt32T* v_ptr = Int32ValueStore_->Values.data();
  for (size_t i = 0; i < Int32ValueStore_->Values.size(); i++) {
    Int32ValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, Int32ValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, Int32ValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, Int32ValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, Int32ValueStore_->Values_p);
  } else {
    LOG_NO("%s SaInt32T: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

// SaUint64T
void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<SaUint64T>& num_values) {
  TRACE_ENTER();
  Uint64ValueStore_ =
      std::unique_ptr<Uint64ValueStore>(new Uint64ValueStore);
  for (auto& num_value : num_values) {
    Uint64ValueStore_->Values.push_back(num_value);
  }
  SaUint64T* v_ptr = Uint64ValueStore_->Values.data();
  for (size_t i = 0; i < Uint64ValueStore_->Values.size(); i++) {
    Uint64ValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, Uint64ValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, Uint64ValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, Uint64ValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, Uint64ValueStore_->Values_p);
  } else {
    LOG_NO("%s SaUint64T: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<SaInt64T>& num_values) {
  TRACE_ENTER();
  Int64ValueStore_ =
      std::unique_ptr<Int64ValueStore>(new Int64ValueStore);
  for (auto& num_value : num_values) {
    Int64ValueStore_->Values.push_back(num_value);
  }
  SaInt64T* v_ptr = Int64ValueStore_->Values.data();
  for (size_t i = 0; i < Int64ValueStore_->Values.size(); i++) {
    Int64ValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, Int64ValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, Int64ValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, Int64ValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, Int64ValueStore_->Values_p);
  } else {
    LOG_NO("%s SaInt64T: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

// SaFloatT
void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<SaFloatT>& num_values) {
  TRACE_ENTER();
  FloatValueStore_ =
      std::unique_ptr<FloatValueStore>(new FloatValueStore);
  for (auto& num_value : num_values) {
    FloatValueStore_->Values.push_back(num_value);
  }
  SaFloatT* v_ptr = FloatValueStore_->Values.data();
  for (size_t i = 0; i < FloatValueStore_->Values.size(); i++) {
    FloatValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, FloatValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, FloatValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, FloatValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, FloatValueStore_->Values_p);
  } else {
    LOG_NO("%s SaFloatT: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

// SaDoubleT
void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<SaDoubleT>& num_values) {
  TRACE_ENTER();
  DoubleValueStore_ =
      std::unique_ptr<DoubleValueStore>(new DoubleValueStore);
  for (auto& num_value : num_values) {
    DoubleValueStore_->Values.push_back(num_value);
  }
  SaDoubleT* v_ptr = DoubleValueStore_->Values.data();
  for (size_t i = 0; i < DoubleValueStore_->Values.size(); i++) {
    DoubleValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, DoubleValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, DoubleValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, DoubleValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, DoubleValueStore_->Values_p);
  } else {
    LOG_NO("%s SaDoubleT: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

// SaTimeT
// For C++ SaTimeT is the same type as SaInt64T but they are different IMM
// model types and must because of that be handled as different types when
// given to the creator API (see SetAttributeValue and TimeValueStore)
void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<CppSaTimeT>& time_values) {
  TRACE_ENTER();
  TimeValueStore_ =
      std::unique_ptr<TimeValueStore>(new TimeValueStore);
  for (auto& num_value : time_values) {
    TimeValueStore_->Values.push_back(num_value);
  }
  CppSaTimeT* v_ptr = TimeValueStore_->Values.data();
  for (size_t i = 0; i < TimeValueStore_->Values.size(); i++) {
    TimeValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, TimeValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, TimeValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, TimeValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, TimeValueStore_->Values_p);
  } else {
    LOG_NO("%s CppSaTimeT: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

void SetAttribute::SetAttributeValues(const std::string& name,
                                const std::vector<std::string>& str_values) {
  TRACE_ENTER();
  StringValueStore_ =
      std::unique_ptr<StringValueStore>(new StringValueStore);
  for (auto& str_value : str_values) {
    StringValueStore_->Values.push_back(str_value);
  }
  std::string* v_ptr = StringValueStore_->Values.data();
  for (size_t i = 0; i < StringValueStore_->Values.size(); i++) {
    StringValueStore_->Values_p.push_back(v_ptr++);
  }

  if (request_ == Request::kCreate) {
    creator_->SetAttributeValue(name, StringValueStore_->Values_p);
  } else if (request_ == Request::kModifyAdd) {
    modifier_->AddAttributeValue(name, StringValueStore_->Values_p);
  } else if (request_ == Request::kModifyReplace) {
    modifier_->ReplaceAttributeValue(name, StringValueStore_->Values_p);
  } else if (request_ == Request::kModifyDelete) {
    modifier_->DeleteAttributeValue(name, StringValueStore_->Values_p);
  } else {
    LOG_NO("%s string: Fail, The type of request is not set",
           __FUNCTION__);
  }
  TRACE_LEAVE();
}

}  // namespace modelmodify
