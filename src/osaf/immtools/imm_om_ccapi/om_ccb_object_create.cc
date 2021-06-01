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

#include "om_ccb_object_create.h"
#include <string.h>
#include <vector>
#include "base/saf_error.h"
#include "base/osaf_extended_name.h"

namespace immom {

//-----------------------------------------------------------------------------
// ImmOmCcbObjectCreate class
//-----------------------------------------------------------------------------
ImmOmCcbObjectCreate::ImmOmCcbObjectCreate(
    const SaImmCcbHandleT& ccb_handle)
    : ImmBase(),
      class_name_{},
      parent_object_{},
      list_of_attribute_properties_{},
      ccb_handle_{ccb_handle} { }

ImmOmCcbObjectCreate::~ImmOmCcbObjectCreate() {
  FreeAllocatedMemory();
}

bool ImmOmCcbObjectCreate::AddObjectCreateToCcb() {
  TRACE_ENTER();
  SaNameT name;
  size_t size = list_of_attribute_properties_.size();
  SaImmAttrValuesT_2 temp[size];
  const SaImmAttrValuesT_2 *attribute_values[size + 1];

  if (parent_object_.empty() != true) {
    osaf_extended_name_lend(parent_object_.c_str(), &name);
  }

  unsigned i = 0;
  for (auto& attribute : list_of_attribute_properties_) {
    attribute_values[i] = &temp[i];
    attribute->FormAttrValuesT_2(&temp[i++]);
  }
  attribute_values[i] = nullptr;

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmCcbObjectCreate_2(
        ccb_handle_,
        const_cast<char*>(class_name_.c_str()),
        parent_object_.empty() ? nullptr : &name,
        attribute_values);

    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }
  return (ais_error_ == SA_AIS_OK);
}

void ImmOmCcbObjectCreate::FreeAllocatedMemory() {
  TRACE_ENTER();
  for (auto& attribute : list_of_attribute_properties_) {
    if (attribute == nullptr) continue;
    delete attribute;
    attribute = nullptr;
  }
}

}  // namespace immom
