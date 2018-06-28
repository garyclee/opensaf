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

#include "smf/smfd/imm_modify_demo/common.h"

#include "smf/smfd/imm_modify_config/immccb.h"

using namespace std;

#if 1  // Low level OM interface
#include "smf/smfd/imm_om_ccapi/common/common.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_clear.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_handle.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_set.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_handle.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_create.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_delete.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_modify.h"
#include "smf/smfd/imm_om_ccapi/om_handle.h"
#endif

void PrintSaAnyT(SaAnyT *value) {
  std::cout << "PrintSaAnyT --------------" << std::endl;
  std::cout << "  bufferSize = " << value->bufferSize << std::endl;
  printf("  bufferAddr = %p\n", value->bufferAddr);
#if 1
  for (SaSizeT i = 0; i < value->bufferSize; i++) {
      printf("    %llu. bufval = 0x%x\n", i, value->bufferAddr[i]);
  }
#endif
  std::cout << "---------------------------" << std::endl;
}

void PrintAttributeValues(const modelmodify::AttributeDescriptor& attr) {
  for (auto& value : attr.values_as_strings) {
    cout << " * " << value << endl;
  }
}

void PrintCreateDescriptor(modelmodify::CreateDescriptor create) {
  cout << "class_name: " << create.class_name << endl;
  cout << "parent_name: " << create.parent_name << endl;
  for (auto& attribute : create.attributes) {
    cout << "attribute_name: " << attribute.attribute_name << endl;
    cout << "values:" << endl;
    PrintAttributeValues(attribute);
  }
}

void PrintCcbDescriptor(modelmodify::CcbDescriptor ccb) {
  int i = 1;
  for (auto& create_descriptor : ccb.create_descriptors) {
    cout << i++ << ". " << "CcbDescriptor" << endl;
    PrintCreateDescriptor(create_descriptor);
    cout << endl;
  }
}

void PrintName(SaNameT* name) {
  cout << "PrintName()" << endl;
  SaConstStringT test_string = osaf_extended_name_borrow(name);
  cout << "String length = " << osaf_extended_name_length(name) << endl;
  cout << "test_string = " << test_string << endl;
}

#if 0  // Working code using om API directly
int main() {
  cout << "Creating an object using 'immom' directly" << endl;
  cout << "Class: ImmTestValuesConfig" << endl;
  cout << "Creating: TestObj1=1,safApp=safSmfService" << endl << endl;

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

  immom::ImmOmAdminOwnerSet ao_setter(ao_handle.GetHandle());
  ao_setter.AddObjectName("safApp=safSmfService");  // Parent
  if (ao_setter.SetAdminOwner(SA_IMM_ONE) == false) {
    cout << "AO Set Fail" << endl;
    return -1;
  }

  immom::ImmOmCcbObjectCreate creator(ccb_handle.GetHandle());
  creator.SetClassName("ImmTestValuesConfig");
  creator.SetParentName("safApp=safSmfService");

  std::string obj_name = "TestObj1=1";
  std::vector<std::string*> obj_name_ps;
  obj_name_ps.push_back(&obj_name);
  creator.SetAttributeValue("immTestValuesCfg", obj_name_ps);

  // Setting values and pointers to the values
  std::vector<SaUint32T> uint32_values;
  std::vector<SaUint32T*> uint32_values_pointers;
  for (SaUint32T i = 0; i < 5; i++) {
    uint32_values.push_back(i);
  }
  SaUint32T* val_ptr = uint32_values.data();
  for (size_t i = 0; i < uint32_values.size(); i++) {
    uint32_values_pointers.push_back(val_ptr++);
  }

  std::vector<SaInt64T> int64_values;
  std::vector<SaInt64T*> int64_values_pointers;
  for (SaInt64T i = 10; i < 15; i++) {
    int64_values.push_back(i);
  }
  SaInt64T* int64_p = int64_values.data();
  for (size_t i = 0; i < int64_values.size(); i++) {
    int64_values_pointers.push_back(int64_p++);
  }

  CppSaTimeT time_value;
  std::vector<CppSaTimeT> time_values;
  std::vector<CppSaTimeT*> time_values_pointers;
  SaTimeT t = 100;
  for (size_t i = 100; i < 105; i++) {
    time_value = t++;
    time_values.push_back(time_value);
  }
  CppSaTimeT* time_p = time_values.data();
  for (size_t i = 0; i < time_values.size(); i++) {
    time_values_pointers.push_back(time_p++);
  }

  // SaNameT
  std::vector<SaNameT> name_values;
  std::vector<SaNameT*> name_values_pointers;
  SaNameT name_value;

  osaf_extended_name_lend("test_name1", &name_value);
  PrintName(&name_value);
  name_values.push_back(name_value);
  osaf_extended_name_lend("test_name2", &name_value);
  PrintName(&name_value);
  name_values.push_back(name_value);
  SaNameT* name_p = name_values.data();
  name_values_pointers.push_back(name_p++);
  name_values_pointers.push_back(name_p);

  // SaAnyT
  SaAnyT any_value1;
  SaUint8T buffer1[] = {1, 2, 3, 4, 5};
  any_value1.bufferAddr = buffer1;
  any_value1.bufferSize = sizeof(buffer1);
  SaAnyT any_value2;
  SaUint8T buffer2[] = {6, 7, 8, 9, 10};
  any_value2.bufferAddr = buffer2;
  any_value2.bufferSize = sizeof(buffer2);
  std::vector<SaAnyT> any_values;
  std::vector<SaAnyT*> any_values_pointers;

  any_values.push_back(any_value1);
  any_values.push_back(any_value2);
  SaAnyT* any_p = any_values.data();
  any_values_pointers.push_back(any_p++);
  any_values_pointers.push_back(any_p);

  creator.SetAttributeValue("SaUint32TValues", uint32_values_pointers);
  // creator.SetAttributeValue("SaInt64TValues", int64_values_pointers);
  // creator.SetAttributeValue("SaTimeTValues", time_values_pointers);
  creator.SetAttributeValue("SaNameTValues", name_values_pointers);
  creator.SetAttributeValue("SaAnyTValues", any_values_pointers);

  cout << "AddObjectCreateToCcb()" << endl;
  if (creator.AddObjectCreateToCcb() == false) {
    cout << "AddObjectCreateToCcb() Fail, " << creator.ais_error_string()
         << endl;
    return -1;
  }

  cout << "ApplyCcb()" << endl;
  if (ccb_handle.ApplyCcb() == false) {
    cout << "ApplyCcb() Fail" << endl;
    // return -1;
  }

  cout << "Clean up; Finalize OM handle" << endl;
  om_handle.FinalizeHandle();

  return 0;
}
#endif

#if 1  // Using modelmofify immccb
int main(void) {
  cout << "I am ccbdemo1 creating 2 objects using 'modelmodify'" << endl;
  cout << "Class name: ImmTestValuesConfig" << endl;
  cout << "Creating: TestObj1=1,safApp=safSmfService" << endl;
  cout << "Creating root object: TestObj2=1" << endl << endl;

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

  // Prepare/enable extended name
  // ----------------------------
  if (EnableImmLongDn() == false) return -1;
  setenv("SA_ENABLE_EXTENDED_NAMES", "1", 1);
  osaf_extended_name_init();
  // Note: Long DN must be configured in IMM configuration object before
  // running ccbdemo_...
  // DN for that object is opensafImm=opensafImm,safApp=safImmService


  // Create an object TestObj1=1 of class ImmTestValuesConfig
  // Parent is safApp=safSmfService
  // Name of rdn attribute is immTestValuesCfg
  // Set attribute SaUint32TValues with 2 values of type SA_IMM_ATTR_SAUINT32T;
  // 1 and 2

  modelmodify::CcbDescriptor test_ccb;
  modelmodify::CreateDescriptor test_object_create;
  modelmodify::AttributeDescriptor object_name;
  modelmodify::AttributeDescriptor uint32_values;
  modelmodify::AttributeDescriptor uint32_value;
  modelmodify::AttributeDescriptor int32_values;
  modelmodify::AttributeDescriptor uint64_values;
  modelmodify::AttributeDescriptor int64_values;
  modelmodify::AttributeDescriptor time_values;
  modelmodify::AttributeDescriptor float_values;
  modelmodify::AttributeDescriptor double_values;
  modelmodify::AttributeDescriptor name_values;
  modelmodify::AttributeDescriptor any_values;
  modelmodify::AttributeDescriptor string_values;

  cout << "Construct attribute descriptors" << endl;
  // Construct the attributes:
  // -------------------------
  // SA_IMM_ATTR_SASTRINGT The object name
  object_name.attribute_name = "immTestValuesCfg";
  object_name.value_type = SA_IMM_ATTR_SASTRINGT;
  object_name.AddValue("TestObj1=1");

  // SA_IMM_ATTR_SAUINT32T
  // Multi value
  uint32_values.attribute_name = "SaUint32TValues";
  uint32_values.value_type = SA_IMM_ATTR_SAUINT32T;
  uint32_values.AddValue(std::to_string(1));
  uint32_values.AddValue(std::to_string(2));
  // Single value
  uint32_value.attribute_name = "SaUint32TValue";
  uint32_value.value_type = SA_IMM_ATTR_SAUINT32T;
  uint32_value.AddValue(std::to_string(9));

  // SA_IMM_ATTR_SAINT32T
  int32_values.attribute_name = "SaInt32TValues";
  int32_values.value_type = SA_IMM_ATTR_SAINT32T;
  int32_values.AddValue(std::to_string(3));
  int32_values.AddValue(std::to_string(4));

  // SA_IMM_ATTR_SAUINT64T
  uint64_values.attribute_name = "SaUint64TValues";
  uint64_values.value_type = SA_IMM_ATTR_SAUINT64T;
  uint64_values.AddValue(std::to_string(5));
  uint64_values.AddValue(std::to_string(6));

  // SA_IMM_ATTR_SAINT64T
  int64_values.attribute_name = "SaInt64TValues";
  int64_values.value_type = SA_IMM_ATTR_SAINT64T;
  int64_values.AddValue(std::to_string(7));
  int64_values.AddValue(std::to_string(8));

  // SA_IMM_ATTR_SATIMET
  time_values.attribute_name = "SaTimeTValues";
  time_values.value_type = SA_IMM_ATTR_SATIMET;
  time_values.AddValue(std::to_string(100));
  time_values.AddValue(std::to_string(200));

  // SA_IMM_ATTR_SAFLOATT
  float_values.attribute_name = "SaFloatValues";
  float_values.value_type = SA_IMM_ATTR_SAFLOATT;
  float_values.AddValue(std::to_string(1.1));
  float_values.AddValue(std::to_string(1.2));

  // SA_IMM_ATTR_SADOUBLET
  double_values.attribute_name = "SaDoubleValues";
  double_values.value_type = SA_IMM_ATTR_SADOUBLET;
  double_values.AddValue(std::to_string(2.1));
  double_values.AddValue(std::to_string(2.2));

  // SA_IMM_ATTR_SANAMET
  SaNameT a_name;
  name_values.attribute_name = "SaNameTValues";
  name_values.value_type = SA_IMM_ATTR_SANAMET;
  // Short name
  osaf_extended_name_lend("a_name1", &a_name);
  name_values.AddValue(modelmodify::SaNametToString(&a_name));
  osaf_extended_name_lend("a_name2", &a_name);
  name_values.AddValue(modelmodify::SaNametToString(&a_name));
  // Long name
  char long_name[300];
  for (size_t i = 0; i < 299; i++) {
    long_name[i] = 'a';
  }
  long_name[299] = '\0';
  osaf_extended_name_lend(long_name, &a_name);
  name_values.AddValue(modelmodify::SaNametToString(&a_name));
  osaf_extended_name_lend("a_name3", &a_name);
  name_values.AddValue(modelmodify::SaNametToString(&a_name));



  // SA_IMM_ATTR_SAANYT
  any_values.attribute_name = "SaAnyTValues";
  any_values.value_type = SA_IMM_ATTR_SAANYT;

  SaAnyT any_value1;
  SaUint8T buffer1[] = {1, 2, 3, 4, 5};
  any_value1.bufferAddr = buffer1;
  any_value1.bufferSize = sizeof(buffer1);
  any_values.AddValue(modelmodify::SaAnytToString(&any_value1));

  SaAnyT any_value2;
  SaUint8T buffer2[] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  any_value2.bufferAddr = buffer2;
  any_value2.bufferSize = sizeof(buffer2);
  any_values.AddValue(modelmodify::SaAnytToString(&any_value2));

  // SA_IMM_ATTR_SASTRINGT
  string_values.attribute_name = "SaStringValues";
  string_values.value_type = SA_IMM_ATTR_SASTRINGT;
  string_values.AddValue("str_value1");
  string_values.AddValue("str_value2");

  cout << "Construct one create descriptor" << endl;
  // Construct one create request:
  // -----------------------------
  test_object_create.class_name = "ImmTestValuesConfig";
  test_object_create.parent_name = "safApp=safSmfService";
  test_object_create.AddAttribute(object_name);
  test_object_create.AddAttribute(uint32_values);
  test_object_create.AddAttribute(uint32_value);
  test_object_create.AddAttribute(int32_values);
  test_object_create.AddAttribute(uint64_values);
  test_object_create.AddAttribute(int64_values);
  test_object_create.AddAttribute(time_values);
  test_object_create.AddAttribute(float_values);
  test_object_create.AddAttribute(double_values);

  test_object_create.AddAttribute(name_values);
  test_object_create.AddAttribute(any_values);
  test_object_create.AddAttribute(string_values);

  // =======================================
  // A second create request.
  // Parent and class is the same
  // Reuse some attribute descriptors
  // =======================================
  modelmodify::CreateDescriptor test_object_create2;
  test_object_create2.class_name = "ImmTestValuesConfig";
  // If the following line is commented out a root object will be created
  // test_object_create2.parent_name = "safApp=safSmfService";

  // SA_IMM_ATTR_SASTRINGT The object name. Structure reused and value modified

  // Modify by deleting the old value and add a new value for object name
  object_name.values_as_strings.clear();
  object_name.AddValue("TestObj2=1");

  // SA_IMM_ATTR_SAUINT32T. Structure is reused without modifications

  test_object_create2.AddAttribute(object_name);
  test_object_create2.AddAttribute(uint32_values);

  cout << "Construct a CCB descriptor" << endl;
  // Construct a CCB descriptor:
  // ---------------------------
  test_ccb.AddCreate(test_object_create);
  test_ccb.AddCreate(test_object_create2);

  cout << "CCB descriptor for test_ccb created" << endl;
  // PrintCcbDescriptor(test_ccb);
  cout << endl;

  modelmodify::ModelModification model_modifier;
  model_modifier.SetCcbFlags(0);
  if (model_modifier.DoModelModification(test_ccb) == false) {
    cout << "DoModification() Fail" << endl;
  }

  cout << endl << "ccbdemo_create Done" << endl;
  return 0;
}
#endif
