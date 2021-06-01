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

#include "imm_attribute.h"
#include "base/logtrace.h"

void AttributeProperty::FormAttrValuesT_2(
    SaImmAttrValuesT_2* output) const {
  TRACE_ENTER();
  assert(output != nullptr);
  output->attrName = const_cast<char*>(attribute_name_.c_str());
  output->attrValueType = attribute_type_;
  output->attrValuesNumber = num_of_values_;
  output->attrValues = attribute_values_pointers_;
}

void AttributeProperty::FormSearchOneAttrT_2(
    SaImmSearchOneAttrT_2* output) const {
  TRACE_ENTER();
  assert(output != nullptr);
  output->attrName = const_cast<char*>(attribute_name_.c_str());
  output->attrValueType = attribute_type_;
  void* value = (attribute_values_pointers_ == nullptr) ? nullptr :
    attribute_values_pointers_[0];
  output->attrValue = value;
}

void AttributeProperty::FormAdminOperationParamsT_2(
    SaImmAdminOperationParamsT_2* output) const {
  TRACE_ENTER();
  assert(output != nullptr);
  output->paramName = const_cast<char*>(attribute_name_.c_str());
  output->paramType = attribute_type_;
  void* value = (attribute_values_pointers_ == nullptr) ? nullptr :
    attribute_values_pointers_[0];
  output->paramBuffer = value;
}

void AttributeDefinition::FormAttrDefinitionT_2(
    SaImmAttrDefinitionT_2* output) const {
  TRACE_ENTER();
  assert(output != nullptr);
  output->attrName = const_cast<char*>(attribute_name_.c_str());
  output->attrValueType = attribute_type_;
  output->attrFlags = attribute_flags_;
  void* value = (attribute_values_pointers_ == nullptr) ? nullptr :
    attribute_values_pointers_[0];
  output->attrDefaultValue = value;
}

void AttributeModification::FormAttrModificationT_2(
    SaImmAttrModificationT_2* output) const {
  TRACE_ENTER();
  assert(output != nullptr);
  output->modType = modification_type_;
  FormAttrValuesT_2(&output->modAttr);
}

void AttributeProperty::FreeMemory() {
  TRACE_ENTER();
  if (attribute_values_pointers_ == nullptr) return;
  switch (attribute_type_) {
    case SA_IMM_ATTR_SAINT32T:
      delete[] reinterpret_cast<SaInt32T**>(attribute_values_pointers_);
      break;

    case SA_IMM_ATTR_SAUINT32T:
      delete[] reinterpret_cast<SaUint32T**>(attribute_values_pointers_);
      break;

    case SA_IMM_ATTR_SAINT64T:
      delete[] reinterpret_cast<SaInt64T**>(attribute_values_pointers_);
      break;

    case SA_IMM_ATTR_SAUINT64T:
      delete[] reinterpret_cast<SaUint64T**>(attribute_values_pointers_);
      break;

    case SA_IMM_ATTR_SANAMET:
      delete[] reinterpret_cast<SaNameT**>(attribute_values_pointers_);
      break;

    case SA_IMM_ATTR_SAFLOATT:
      delete[] reinterpret_cast<SaFloatT**>(attribute_values_pointers_);
      break;

    case SA_IMM_ATTR_SADOUBLET:
      delete[] reinterpret_cast<SaDoubleT**>(attribute_values_pointers_);
      break;

    case SA_IMM_ATTR_SASTRINGT:
      delete[] reinterpret_cast<SaStringT**>(attribute_values_pointers_);
      break;

    default:
      delete[] reinterpret_cast<SaAnyT**>(attribute_values_pointers_);
      break;
  }

  attribute_values_pointers_ = nullptr;
}
