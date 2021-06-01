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

#include <limits.h>

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <iostream>

#include "ais/include/saImm.h"
#include "ais/include/saAis.h"
#include "base/osaf_extended_name.h"
#include "osaf/configmake.h"

#include "osaf/immtools/imm_modify_demo/common.h"

#include "osaf/immtools/imm_modify_config/immccb.h"

#if 1  // Low level OM interface
#include "osaf/immtools/imm_om_ccapi/common/common.h"
#include "osaf/immtools/imm_om_ccapi/om_admin_owner_clear.h"
#include "osaf/immtools/imm_om_ccapi/om_admin_owner_handle.h"
#include "osaf/immtools/imm_om_ccapi/om_admin_owner_set.h"
#include "osaf/immtools/imm_om_ccapi/om_ccb_handle.h"
#include "osaf/immtools/imm_om_ccapi/om_ccb_object_create.h"
#include "osaf/immtools/imm_om_ccapi/om_ccb_object_delete.h"
#include "osaf/immtools/imm_om_ccapi/om_ccb_object_modify.h"
#include "osaf/immtools/imm_om_ccapi/om_handle.h"
#endif

using namespace std;
#if 0  // Working code using om API directly
// Note: Before running this code an object to modify must have been created
//       This can be done by running ccbdemo_create
int main() {
  cout << "Modifying an object using 'immom' directly. Object must exist" <<
      endl;
  cout << "Class: ImmTestValuesConfig" << endl;
  cout << "Modify: TestObj1=1,safApp=safSmfService" << endl << endl;

  // Prepare extended name
  setenv("SA_ENABLE_EXTENDED_NAMES", "1", 1);
  osaf_extended_name_init();

  immom::ImmOmHandle om_handle;
  if (om_handle.InitializeHandle() == false) {
    cout << "OM handle Fail" << endl;
    return -1;
  }

  immom::ImmOmAdminOwnerHandle ao_handle(om_handle.GetHandle(), "ccbdemo1");
  if (ao_handle.InitializeHandle() == false) {
    cout << "AO handle Fail" << endl;
    return -1;
  }

  immom::ImmOmCcbHandle ccb_handle(ao_handle.GetHandle(), 0);
  if (ccb_handle.InitializeHandle() == false) {
    cout << "CCB handle Fail" << endl;
    return -1;
  }

  cout << "Become AO of the object to be modified" << endl;
  immom::ImmOmAdminOwnerSet ao_setter(ao_handle.GetHandle());
  ao_setter.AddObjectName("TestObj1=1,safApp=safSmfService");  // Object
  if (ao_setter.SetAdminOwner(SA_IMM_ONE) == false) {
    cout << "AO Set Fail, " << ao_setter.ais_error_string() << endl;
    return -1;
  }

  cout << "Setup modification(s)" << endl;


  cout << "Add modify operation(s) to CCB" << endl;
  std::string object1 = "TestObj1=1,safApp=safSmfService";
  // std::string object2 = "TestObj2=1";

  immom::ImmOmCcbObjectModify modifier(ccb_handle.GetHandle(), object1);

  // Add 10 and 20 to existing SaUint32TValues
  std::vector<SaUint32T> uint32_values{10, 20};
  std::vector<SaUint32T*> uint32_values_pointers;
  SaUint32T* val_p = uint32_values.data();
  uint32_values_pointers.push_back(val_p++);
  uint32_values_pointers.push_back(val_p);

  // Replace the SaUint32TValue with 90
  std::vector<SaUint32T> uint32_values1{90};
  std::vector<SaUint32T*> uint32_values_pointers1;
  val_p = uint32_values1.data();
  uint32_values_pointers1.push_back(val_p);


  modifier.AddAttributeValue("SaUint32TValues", uint32_values_pointers);
  modifier.ReplaceAttributeValue("SaUint32TValue", uint32_values_pointers1);
  if (modifier.AddObjectModifyToCcb() == false) {
    cout << "AddObjectModifyToCcb() Fail, " << modifier.ais_error_string()
         << endl;
  }

  cout << "ApplyCcb()" << endl;
  if (ccb_handle.ApplyCcb() == false) {
    cout << "ApplyCcb() Fail" << endl;
    return -1;
  }

  cout << "Clean up; Finalize OM handle" << endl;
  om_handle.FinalizeHandle();

  return 0;
}
#endif

#if 1  // Modifying object using modelmodify
void PrintValues(modelmodify::ModifyDescriptor modifier) {
  cout << "Printing a modifier:" << endl;

  cout << "modifier.object_name = " << modifier.object_name << endl;

  cout << "Modifications:" << endl;
  for (auto& modification : modifier.modifications) {
    cout << "modification.modification_type = " <<
        modification.modification_type << endl;
    cout << "modification.attribute_descriptor.attribute_name = " <<
        modification.attribute_descriptor.attribute_name << endl;
    cout << "modification.attribute_descriptor.value_type = " <<
        modification.attribute_descriptor.value_type << endl;
    cout << "Values:" << endl;
    for (auto& str_value :
         modification.attribute_descriptor.values_as_strings) {
      cout << "str_value = " << str_value << endl;
    }
  }
  cout << endl;
}

int main() {
  cout << "Modifying an object using 'modelmodify'. Object must exist" <<
      endl;
  cout << "Class: ImmTestValuesConfig" << endl;
  cout << "Modify: TestObj1=1,safApp=safSmfService" << endl << endl;

#if 0  // Enable trace
  unsigned int category_mask = 0xffffffff;
  const char* logPath = PKGLOGDIR "/osafccbdemo1";
  if (logtrace_init("ccbdemo1", logPath, category_mask) == -1) {
    syslog(LOG_ERR, "osafntfimcnd logtrace_init FAILED");
    /* We allow to execute anyway. */
    cout << "logtrace_init() Fail" << endl;
  } else {
    // cout << "logtrace enabled" << endl;
  }
#endif
  // An IMM demo class is used. Is installed here
  // --------------------------------------------
  if (InstallDemoClass() == false) return -1;

  // Prepare extended name
  // ---------------------
  EnableImmLongDn();
  setenv("SA_ENABLE_EXTENDED_NAMES", "1", 1);
  osaf_extended_name_init();

  // ===========================================================================
  // Add 10 and 20 to existing 'SaUint32TValues' attribute
  // ===========================================================================

  // 1. Attribute descriptor
  modelmodify::AttributeDescriptor uint32_multivalue_attribute;
  uint32_multivalue_attribute.attribute_name = "SaUint32TValues";
  uint32_multivalue_attribute.value_type = SA_IMM_ATTR_SAUINT32T;
  uint32_multivalue_attribute.AddValue(std::to_string(10));
  uint32_multivalue_attribute.AddValue(std::to_string(20));

  // 1. Attribute modify descriptor
  modelmodify::AttributeModifyDescriptor uint32_multivalue_modify;
  uint32_multivalue_modify.modification_type = SA_IMM_ATTR_VALUES_ADD;
  uint32_multivalue_modify.attribute_descriptor = uint32_multivalue_attribute;

  // 2. Attribute descriptor
  modelmodify::AttributeDescriptor uint32_attribute;
  uint32_attribute.attribute_name = "SaUint32TValue";
  uint32_attribute.value_type = SA_IMM_ATTR_SAUINT32T;
  uint32_attribute.AddValue(to_string(90));

  // 2. Attribute modify descriptor
  modelmodify::AttributeModifyDescriptor uint32_modify;
  uint32_modify.modification_type = SA_IMM_ATTR_VALUES_REPLACE;
  uint32_modify.attribute_descriptor = uint32_attribute;

  // Add modification to the attribute modification to the modify descriptor
  modelmodify::ModifyDescriptor modification;
  modification.object_name = "TestObj1=1,safApp=safSmfService";
  modification.AddAttributeModification(uint32_multivalue_modify);
  modification.AddAttributeModification(uint32_modify);

  // Print modifier content
  PrintValues(modification);

  // Add the modify descriptor to the ccb descriptor
  modelmodify::CcbDescriptor model_configuration_change;
  model_configuration_change.AddModify(modification);

  // Create a modifier object and do the modification described in the
  // ccb descriptor 'model_configuration_change'
  modelmodify::ModelModification model_changer;
  model_changer.SetCcbFlags(0);
  if (model_changer.DoModelModification(model_configuration_change) == false) {
    cout << "DoModelModification() Fail" << endl;
  } else {
    cout << "DoModelModification() Success" << endl;
  }

  return 0;
}
#endif
