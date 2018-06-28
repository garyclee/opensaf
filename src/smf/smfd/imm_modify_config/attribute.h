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
#include <memory>

#include "ais/include/saImm.h"
#include "ais/include/saAis.h"

#include "smf/smfd/imm_modify_config/immccb.h"

#include "smf/smfd/imm_om_ccapi/common/common.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_create.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_modify.h"

#ifndef SMF_SMFD_IMM_MODIFY_CONFIG_ATTRIBUTE_H_
#define SMF_SMFD_IMM_MODIFY_CONFIG_ATTRIBUTE_H_

// Convert std::string to SaNameT or SaAnyT.
// Shall be used with AttributeDescriptor object where SaNametToString or
// SaAnytToString has been used to convert the data to std::string
// Note: There are no rules about what content these types may hold so there
//       is no validation
static inline void StringToSaNameT(const std::string& str_namet,
                                   SaNameT* namet_value) {
  osaf_extended_name_lend(str_namet.c_str(), namet_value);
}

// Note: This function allocates memory for a buffer that can hold the data in
//       the string and writes the address in the SaAnyT buffer pointer.
//       This means that this memory must be freed after usage using:
//       free(<my_anyt>.bufferAddr);
static inline void StringToSaAnyT(const std::string& str_anyt, SaAnyT* anyt_value) {
  size_t anyt_size = str_anyt.size();
  SaUint8T* anyt_buffer = static_cast<SaUint8T*>(malloc(anyt_size));
  if (anyt_buffer == nullptr) {
    LOG_ER("%s: malloc() Fail", __FUNCTION__);
    osafassert(0);
  }
  size_t i = 0;
  for (auto& char_data : str_anyt) {
    anyt_buffer[i++] = char_data;
  }
  anyt_value->bufferSize = anyt_size;
  anyt_value->bufferAddr = anyt_buffer;
}

namespace modelmodify {

// Store values of different types
struct Int32ValueStore {
  std::vector<SaInt32T> Values;
  std::vector<SaInt32T*> Values_p;
};
struct Uint32ValueStore {
  std::vector<SaUint32T> Values;
  std::vector<SaUint32T*> Values_p;
};
struct Int64ValueStore {
  std::vector<SaInt64T> Values;
  std::vector<SaInt64T*> Values_p;
};
struct Uint64ValueStore {
  std::vector<SaUint64T> Values;
  std::vector<SaUint64T*> Values_p;
};
struct TimeValueStore {
  std::vector<CppSaTimeT> Values;
  std::vector<CppSaTimeT*> Values_p;
};
struct FloatValueStore {
  std::vector<SaFloatT> Values;
  std::vector<SaFloatT*> Values_p;
};
struct DoubleValueStore {
  std::vector<SaDoubleT> Values;
  std::vector<SaDoubleT*> Values_p;
};
struct StringValueStore {
  std::vector<std::string> Values;
  std::vector<std::string*> Values_p;
};
struct NameValueStore {
  std::vector<SaNameT> Values;
  std::vector<SaNameT*> Values_p;
};
struct AnyValueStore {
  // Note1: For this set of values a buffer that contains the actual value is
  //        allocated (C allocation is used). A pointer to this buffer can be
  //        found as a parameter in the SaAnyT. When this storage goes out of
  //        scope this buffer must be freed
  // Note2: The StringToSaAnyT() function is used to create a SaAnyT value.
  //        This is where memory for the buffer is allocated
  ~AnyValueStore() {
    if (Values.empty() == false) {
      for (auto& value : Values) {
        if (value.bufferAddr != nullptr) free(value.bufferAddr);
      }
    }
  }

  std::vector<SaAnyT> Values;
  std::vector<SaAnyT*> Values_p;
};

// Attributes are used with different types of operations
enum class Request {
  kNotSet,
  kCreate,
  kModifyAdd,
  kModifyReplace,
  kModifyDelete
};

// Adds one attribute to a creator
// Used by class Attribute. Class SAttribute must see to that all objects of
// this class does not go out of scope until an object of class Attribute goes
// out of scope
class SetAttribute {
 public:
  explicit SetAttribute(immom::ImmOmCcbObjectCreate* creator) : SetAttribute() {
    creator_ = creator;
    request_ = Request::kCreate;
  }
  explicit SetAttribute(immom::ImmOmCcbObjectModify* modifier, Request request)
      : SetAttribute() {
    modifier_ = modifier;
    request_ = request;
  }

  // Store one attribute of a given type and add it to a creator or a modifier
  // The values are stored in a vector and another vector with pointers to the
  // values is created. It is the pointer vector that is the in parameter for
  // the creator
  void SetAttributeValues(const std::string& name,
                          const std::vector<SaUint32T>& num_values);
  void SetAttributeValues(const std::string& name,
                          const std::vector<SaInt32T>& num_values);
  void SetAttributeValues(const std::string& name,
                          const std::vector<SaUint64T>& num_values);
  void SetAttributeValues(const std::string& name,
                          const std::vector<SaInt64T>& num_values);
  void SetAttributeValues(const std::string& name,
                          const std::vector<SaFloatT>& num_values);
  void SetAttributeValues(const std::string& name,
                          const std::vector<SaDoubleT>& num_values);

  void SetAttributeValues(const std::string& name,
                          const std::vector<std::string>& str_values);

  void SetAttributeValues(const std::string& name,
                          const std::vector<CppSaTimeT>& time_values);

  void SetAttributeValues(const std::string& name,
                          const std::vector<SaNameT>& name_values);

  void SetAttributeValues(const std::string& name,
                          const std::vector<SaAnyT>& any_values);

 private:
  // Constructor for initializing variables only
  SetAttribute()
    : Uint32ValueStore_(nullptr),
      Int32ValueStore_(nullptr),
      Int64ValueStore_(nullptr),
      Uint64ValueStore_(nullptr),
      TimeValueStore_(nullptr),
      FloatValueStore_(nullptr),
      DoubleValueStore_(nullptr),
      StringValueStore_(nullptr),
      NameValueStore_(nullptr),
      AnyValueStore_(nullptr),
      creator_(nullptr),
      modifier_(nullptr),
      request_(Request::kNotSet) {}

  // Numeric
  std::unique_ptr<Uint32ValueStore> Uint32ValueStore_;
  std::unique_ptr<Int32ValueStore> Int32ValueStore_;
  std::unique_ptr<Int64ValueStore> Int64ValueStore_;
  std::unique_ptr<Uint64ValueStore> Uint64ValueStore_;
  std::unique_ptr<TimeValueStore> TimeValueStore_;
  std::unique_ptr<FloatValueStore> FloatValueStore_;
  std::unique_ptr<DoubleValueStore> DoubleValueStore_;

  // Is used for SaStringT. See om_ccb_object_create
  std::unique_ptr<StringValueStore> StringValueStore_;

  // Special SA types SaNameT and SaAnyT
  std::unique_ptr<NameValueStore> NameValueStore_;
  std::unique_ptr<AnyValueStore> AnyValueStore_;


  immom::ImmOmCcbObjectCreate* creator_;
  immom::ImmOmCcbObjectModify* modifier_;
  Request request_;

  DELETE_COPY_AND_MOVE_OPERATORS(SetAttribute);
};

// Adds all attributes in a create descriptor to a creator
// A creator is the handler that adds an IMM object create request to a CCB
// A creator is represented by the API defined in om_ccb_object_create.h
// This API requires that:
//  - Numeric values are given as a vector of pointers to values of the wanted
//    type.
//  - String values must be given as a vector of std::string
// Also, the values and the pointer must not go out of scope or be deleted until
// AddObjectCreateToCcb() is called.
//
// This class handles the attributes in a create descriptor and adds the
// attributes to the a creator. The values are valid as long as an object of
// this class is in scope.
//
class AttributeHandler {
 public:
  explicit AttributeHandler(immom::ImmOmCcbObjectCreate* creator)
    : creator_(creator), modifier_(nullptr) {}
  explicit AttributeHandler(immom::ImmOmCcbObjectModify* modifier)
    : creator_(nullptr), modifier_(modifier) {}

  // Add all attributes in a create descriptor to a creator
  bool AddAttributesForObjectCreate(const CreateDescriptor& create_descriptor);

  // Add all attributes in a modify descriptor
  // Note that each attribute has an AttributeModifyDescriptor which contains
  // Information about modification type an an AttributeDescriptor
  bool AddAttributesForModification(const ModifyDescriptor& modify_descriptor);

 private:
  bool AddAttribute(const AttributeDescriptor& attribute, Request request);

  template<typename T>
  bool StoreNumericAttribute(const AttributeDescriptor& attribute,
                             Request request);

  void StoreSaNametAttribute(const AttributeDescriptor& attribute,
                             Request request);

  void StoreSaAnytAttribute(const AttributeDescriptor& attribute,
                            Request request);

  void StoreStringAttribute(const AttributeDescriptor& attribute,
                            Request request);

  std::vector<std::unique_ptr<SetAttribute>> set_attributesp_;
  immom::ImmOmCcbObjectCreate* creator_;
  immom::ImmOmCcbObjectModify* modifier_;

  DELETE_COPY_AND_MOVE_OPERATORS(AttributeHandler);
};

}  // namespace modelmodify
#endif /* SMF_SMFD_IMM_MODIFY_CONFIG_ATTRIBUTE_H_ */
