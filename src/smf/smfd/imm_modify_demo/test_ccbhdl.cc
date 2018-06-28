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
#include "base/saf_error.h"
#include "osaf/configmake.h"

#include "smf/smfd/imm_modify_demo/common.h"

#include "smf/smfd/imm_modify_config/immccb.h"

#include "smf/smfd/imm_om_ccapi/common/common.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_clear.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_handle.h"
#include "smf/smfd/imm_om_ccapi/om_admin_owner_set.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_handle.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_create.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_delete.h"
#include "smf/smfd/imm_om_ccapi/om_ccb_object_modify.h"
#include "smf/smfd/imm_om_ccapi/om_handle.h"

using namespace std;

// Before this program can be used the class defined in democlass.xml has to be
// installed.

// Create one object. The whole procedure is done here including applying the
// CCB
// DN = Test1=1,safApp=safSmfService
// Several attributes are set
void CreateOneObject(void) {
  // Create an object description
  // ============================
  modelmodify::AttributeDescriptor attribute;
  modelmodify::CreateDescriptor imm_object;
  std::string object_name = "Test1=1";

  // Define the object
  // -----------------
  imm_object.class_name = "ImmTestValuesConfig";
  imm_object.parent_name = "safApp=safSmfService";
  // Set IMM Object name
  attribute.attribute_name = "immTestValuesCfg";
  attribute.value_type = SA_IMM_ATTR_SASTRINGT;
  attribute.AddValue(object_name);
  imm_object.AddAttribute(attribute);

  // Set some attributes
  // -------------------

  // SA_UINT32_T multi-value attribute
  attribute.attribute_name = "SaUint32TValues";
  attribute.value_type = SA_IMM_ATTR_SAUINT32T;
  // We are reusing so remove "old" values and add new ones
  attribute.values_as_strings.clear();
  attribute.AddValue(std::to_string(1));
  attribute.AddValue(std::to_string(2));
  // Add the attribute to the object
  imm_object.AddAttribute(attribute);

  // SA_INT32_T multi-value attribute
  attribute.attribute_name = "SaInt32TValues";
  attribute.value_type = SA_IMM_ATTR_SAINT32T;
  attribute.values_as_strings.clear();
  attribute.AddValue(std::to_string(3));
  attribute.AddValue(std::to_string(4));
  // Add the attribute to the object
  imm_object.AddAttribute(attribute);

  // SA_NAME_T multi-value attribute
  SaNameT a_name;
  attribute.attribute_name = "SaNameTValues";
  attribute.value_type = SA_IMM_ATTR_SANAMET;
  attribute.values_as_strings.clear();
  // Add two short names
  osaf_extended_name_lend("a_name1", &a_name);
  attribute.AddValue(modelmodify::SaNametToString(&a_name));
  osaf_extended_name_lend("a_name2", &a_name);
  attribute.AddValue(modelmodify::SaNametToString(&a_name));
  // Add a long name and a third short name
  char long_name[300];
  for (size_t i = 0; i < 299; i++) {
    long_name[i] = 'a';
  }
  long_name[299] = '\0';
  osaf_extended_name_lend(long_name, &a_name);
  attribute.AddValue(modelmodify::SaNametToString(&a_name));
  osaf_extended_name_lend("a_name3", &a_name);
  attribute.AddValue(modelmodify::SaNametToString(&a_name));
  imm_object.AddAttribute(attribute);

  // SA_UINT32_T single-value attribute
  attribute.attribute_name = "SaUint32TValue";
  attribute.value_type = SA_IMM_ATTR_SAUINT32T;
  attribute.values_as_strings.clear();
  // Note: Single value attribute so only one value can be added
  attribute.AddValue(std::to_string(10));
  imm_object.AddAttribute(attribute);

  // Create a CCB descriptor and add the description of the object to create
  // =======================================================================
  modelmodify::CcbDescriptor CcbDescriptor;

  CcbDescriptor.AddCreate(imm_object);

  // Create a modification handler and use it to create the object as defined
  // above
  // ========================================================================

  modelmodify::ModelModification ModelModifier;

  // Set CCB flag to inform IMM that there is no Object Implementer
  ModelModifier.SetCcbFlags(0);

  // Create the IMM object
  if (ModelModifier.DoModelModification(CcbDescriptor) == false) {
    cout << "Creation of '" << object_name << "' FAIL" << endl;
  } else {
    cout << "Creation of '" << object_name << "' SUCCESS" << endl;
  }
}

// Remove all modifications from a CCB descriptor so it can be reused
void CcbDescriptorCleaner(modelmodify::CcbDescriptor& ccb_descriptor) {
  ccb_descriptor.create_descriptors.clear();
  ccb_descriptor.delete_descriptors.clear();
  ccb_descriptor.modify_descriptors.clear();
}

// Fill in a create descriptor.
// The created object will be given the name in 'object_name'
// Class name and parent is filled in here
// Values set are:
// SaInt32TValues: 1, 2, 3
// SaUint64TValues: 100, 200
// SaTimeTValues: 10000
void SetupObjectCreate1(const std::string object_name,
                        modelmodify::CreateDescriptor& create_desc) {
  modelmodify::AttributeDescriptor attribute;
  // Prepare the create descriptor
  create_desc.class_name = "ImmTestValuesConfig";
  create_desc.parent_name = "safApp=safSmfService";
  create_desc.attributes.clear();

  // Set object name
  attribute.attribute_name = "immTestValuesCfg";
  attribute.value_type = SA_IMM_ATTR_SASTRINGT;
  attribute.AddValue(object_name);
  create_desc.AddAttribute(attribute);

  // Set SaInt32TValues
  attribute.values_as_strings.clear();
  attribute.attribute_name = "SaInt32TValues";
  attribute.value_type = SA_IMM_ATTR_SAINT32T;
  attribute.AddValue(std::to_string(1));
  attribute.AddValue(std::to_string(2));
  attribute.AddValue(std::to_string(3));
  create_desc.AddAttribute(attribute);

  // Set SaUint64TValues
  attribute.values_as_strings.clear();
  attribute.attribute_name = "SaUint64TValues";
  attribute.value_type = SA_IMM_ATTR_SAUINT64T;
  attribute.AddValue(std::to_string(100));
  attribute.AddValue(std::to_string(200));
  create_desc.AddAttribute(attribute);

  // Set SaTimeTValues
  attribute.values_as_strings.clear();
  attribute.attribute_name = "SaTimeTValues";
  attribute.value_type = SA_IMM_ATTR_SATIMET;
  attribute.AddValue(std::to_string(10000));
  create_desc.AddAttribute(attribute);
}

void WaitForUserAction(const std::string& message) {
  cout << message << endl;
  cout << endl << "Press Enter to continue" << endl << endl;
  getchar();
}

int main() {
  cout << "test_ccbhdl" << endl;
  cout << "IMM class used for test: ImmTestValuesConfig" << endl;

#if 0 //  Enable trace
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

#if 1
  // An IMM demo class is used. Is installed here
  // --------------------------------------------
  if (InstallDemoClass() == false) return -1;
#endif

#if 1
  // Prepare/enable extended name
  // ----------------------------
  setenv("SA_ENABLE_EXTENDED_NAMES", "1", 1);
  
  if (EnableImmLongDn() == false) return -1;

  // Note: Long DN must be configured in IMM configuration object before
  // running ccbdemo_...
  // DN for that object is opensafImm=opensafImm,safApp=safImmService
#endif

  // ==================== TEST START ===========================================

  // Create a first object
  // ¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤
  // This is done in a separate function where also the CCB is applied
  cout << "Creating: Test1=1,safApp=safSmfService" << endl;
  CreateOneObject();
  cout << endl;

  // Wait for the user to verify in another node and press Enter in this node
  WaitForUserAction("Verify creation of Test1=1,safApp=safSmfService");

  // Create a modifier to be used here in main() and set CCB flag to handle
  // that there is no Object Implementer for objects handled here
  // ----------------------------------------------------------------------
  modelmodify::ModelModification modifier;
  modifier.SetCcbFlags(0);
  // A CCB descriptor for the modifier
  modelmodify::CcbDescriptor ccb_descriptor;

  // We also need some model modification descriptors
  modelmodify::CreateDescriptor create_descriptor;
  modelmodify::ModifyDescriptor modify_descriptor;
  modelmodify::DeleteDescriptor delete_descriptor;
  // an attribute modify descriptor
  modelmodify::AttributeDescriptor attribute_descriptor;
  // and an attribute modify descriptor
  modelmodify::AttributeModifyDescriptor attribute_modify_descriptor;

  // Create and apply a CCB that creates two more objects
  // ¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤

  SetupObjectCreate1("Test1=2", create_descriptor);
  ccb_descriptor.AddCreate(create_descriptor);
  SetupObjectCreate1("Test1=3", create_descriptor);
  ccb_descriptor.AddCreate(create_descriptor);
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Create two more objects in the same CCB, FAIL" << endl;
  } else {
    cout << "Create two more objects in the same CCB, SUCCESS" << endl;
  }
  cout << endl;

  WaitForUserAction("Verify creation of Test1=2 and Test1=3 objects "
                    "(,safApp=safSmfService)");

  // Modify the model using a new CCB with mixed modifications
  // ¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤

  // Create one new object "Test1=4"
  // Delete "Test1=2" and "Test1=3" parent "Test1=2,safApp=safSmfService"
  // Modify "Test1=1":
  //  Add 4 to SaUint32TValues
  //  Delete "a_name1" from SaNameTValues
  //  Replace SaInt32TValues 3 and 4 with 5, 6, 7
  // All is done in the same CCB

  // We are reusing the CCB descriptor so:
  // Clean the CCB descriptor from the previous object creation
  CcbDescriptorCleaner(ccb_descriptor);

  // Add a new object to the CBB. The new object is created within a function
  // that uses an attribute descriptor that is local to this function. This will
  // test that data is correctly handed over to the create descriptor
  SetupObjectCreate1("Test1=4", create_descriptor);
  ccb_descriptor.AddCreate(create_descriptor);

  // Add two objects deletions
  delete_descriptor.object_name = "Test1=2,safApp=safSmfService";
  ccb_descriptor.AddDelete(delete_descriptor);
  delete_descriptor.object_name = "Test1=3,safApp=safSmfService";
  ccb_descriptor.AddDelete(delete_descriptor);

  // Setup the modifications for the "test1=1" object:
  modify_descriptor.object_name = "Test1=1,safApp=safSmfService";
  // Add a value to SaUint32TValues
  // Clean attribute descriptor from any possible previous usage
  attribute_descriptor.values_as_strings.clear();
  attribute_descriptor.attribute_name = "SaUint32TValues";
  attribute_descriptor.value_type = SA_IMM_ATTR_SAUINT32T;
  attribute_descriptor.AddValue(std::to_string(4));
  attribute_modify_descriptor.modification_type = SA_IMM_ATTR_VALUES_ADD;
  attribute_modify_descriptor.attribute_descriptor = attribute_descriptor;
  modify_descriptor.AddAttributeModification(attribute_modify_descriptor);
  // Delete a value from SaNameTValues
  attribute_descriptor.values_as_strings.clear();
  SaNameT a_name;  // Use a real SaNameT
  osaf_extended_name_lend("a_name1", &a_name);
  attribute_descriptor.attribute_name = "SaNameTValues";
  attribute_descriptor.value_type = SA_IMM_ATTR_SANAMET;
  attribute_descriptor.AddValue(modelmodify::SaNametToString(&a_name));
  attribute_modify_descriptor.modification_type = SA_IMM_ATTR_VALUES_DELETE;
  attribute_modify_descriptor.attribute_descriptor = attribute_descriptor;
  modify_descriptor.AddAttributeModification(attribute_modify_descriptor);
  // Replace the values in SaInt32TValues
  attribute_descriptor.attribute_name = "SaInt32TValues";
  attribute_descriptor.value_type = SA_IMM_ATTR_SAINT32T;
  attribute_descriptor.values_as_strings.clear();
  attribute_descriptor.AddValue(std::to_string(5));
  attribute_descriptor.AddValue(std::to_string(6));
  attribute_descriptor.AddValue(std::to_string(7));
  attribute_modify_descriptor.modification_type = SA_IMM_ATTR_VALUES_REPLACE;
  attribute_modify_descriptor.attribute_descriptor = attribute_descriptor;
  modify_descriptor.AddAttributeModification(attribute_modify_descriptor);

  // Add the modifications to the CCB
  ccb_descriptor.AddModify(modify_descriptor);

  cout << "Modify the model using a mixed CCB START" << endl;

  // The modifier object is used here for the second time
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Modify the model using a mixed CCB, FAIL" << endl;
  } else {
    cout << "Modify the model using a mixed CCB, SUCCESS" << endl;
  }

  cout << endl;
  cout << "Verify that:" << endl;
  cout << "  - The Test1=2 and Test1=3 objects are deleted" << endl;
  cout << "  - A new Test1=4 object is created" << endl;
  cout << "  - Test1=1 is modified:" << endl;
  cout << "       SaUint32TValues: Added '4'" << endl;
  cout << "       SaNameTValues:   Deleted 'a_name1'" << endl;
  cout << "       SaInt32TValues:  Values '3, 4' "
                                  "replaced by '5, 6, 7'" << endl << endl;

  WaitForUserAction("After pressing Enter the remaining objects "
      "will be deleted");

  cout << "Cleanup by deleting the remaining Test1=1 and Test1=4 objects"
       << endl;

  CcbDescriptorCleaner(ccb_descriptor);
  delete_descriptor.object_name = "Test1=1,safApp=safSmfService";
  ccb_descriptor.AddDelete(delete_descriptor);
  delete_descriptor.object_name = "Test1=4,safApp=safSmfService";
  ccb_descriptor.AddDelete(delete_descriptor);

  // The modifier object is used here for the third time
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Delete Test1=1 and Test1=4, FAIL" << endl;
  } else {
    cout << "Delete Test1=1 and Test1=4, SUCCESS" << endl;
  }

  cout << endl;


  // Try to create an object using an attribute descriptor that has the wrong
  // value_type. Use SA_IMM_ATTR_SAUINT32T where  SA_IMM_ATTR_SASTRINGT is the
  // correct type. The modification shall fail but there should not cause any
  // crash e.g. segv
  // Create an object description
  // ============================
  // Reuse the create descriptor
  create_descriptor.attributes.clear();
  std::string object_name = "Test12=1";

  // Define the object
  // -----------------
  create_descriptor.class_name = "ImmTestValuesConfig";
  create_descriptor.parent_name = "safApp=safSmfService";
  // Set IMM Object name
  attribute_descriptor.attribute_name = "immTestValuesCfg";
  attribute_descriptor.value_type = SA_IMM_ATTR_SASTRINGT;
  // We are reusing so remove "old" values and add new ones
  attribute_descriptor.values_as_strings.clear();
  attribute_descriptor.AddValue(object_name);
  create_descriptor.AddAttribute(attribute_descriptor);

  // Set some attribute_descriptors
  // -------------------

  // SA_UINT32_T multi-value attribute_descriptor
  attribute_descriptor.attribute_name = "SaUint32TValues";
  attribute_descriptor.value_type = SA_IMM_ATTR_SAUINT32T;
  attribute_descriptor.values_as_strings.clear();
  attribute_descriptor.AddValue(std::to_string(1));
  attribute_descriptor.AddValue(std::to_string(2));
  // Add the attribute_descriptor to the object
  create_descriptor.AddAttribute(attribute_descriptor);

  // SA_INT32_T multi-value attribute_descriptor
  attribute_descriptor.attribute_name = "SaInt32TValues";
  attribute_descriptor.value_type = SA_IMM_ATTR_SAINT32T;
  attribute_descriptor.values_as_strings.clear();
  attribute_descriptor.AddValue(std::to_string(3));
  attribute_descriptor.AddValue(std::to_string(4));
  // Add the attribute_descriptor to the object
  create_descriptor.AddAttribute(attribute_descriptor);

  // SA_NAME_T multi-value attribute_descriptor
  SaNameT a_name1;
  attribute_descriptor.attribute_name = "SaNameTValues";
  attribute_descriptor.value_type = SA_IMM_ATTR_SANAMET;
  attribute_descriptor.values_as_strings.clear();
  // Add two short names
  osaf_extended_name_lend("a_name11", &a_name1);
  attribute_descriptor.AddValue(modelmodify::SaNametToString(&a_name1));
  osaf_extended_name_lend("a_name12", &a_name1);
  attribute_descriptor.AddValue(modelmodify::SaNametToString(&a_name1));
  // Add a long name and a third short name
  char long_name[300];
  for (size_t i = 0; i < 299; i++) {
    long_name[i] = 'a';
  }
  long_name[299] = '\0';
  osaf_extended_name_lend(long_name, &a_name1);
  attribute_descriptor.AddValue(modelmodify::SaNametToString(&a_name1));
  osaf_extended_name_lend("a_name13", &a_name1);
  attribute_descriptor.AddValue(modelmodify::SaNametToString(&a_name1));
  create_descriptor.AddAttribute(attribute_descriptor);

  // SA_UINT32_T single-value attribute_descriptor
  attribute_descriptor.attribute_name = "SaUint32TValue";
  attribute_descriptor.value_type = SA_IMM_ATTR_SAUINT32T;
  attribute_descriptor.values_as_strings.clear();
  // Note: Single value attribute_descriptor so only one value can be added
  attribute_descriptor.AddValue(std::to_string(10));
  create_descriptor.AddAttribute(attribute_descriptor);


  // SA_IMM_ATTR_SASTRINGT multi value attribute_descriptor
  // SA_IMM_ATTR_SAUINT32T will be incorrectly used here
  attribute_descriptor.attribute_name = "SaStringValues";
  attribute_descriptor.value_type = SA_IMM_ATTR_SAUINT32T;
  attribute_descriptor.values_as_strings.clear();
  attribute_descriptor.AddValue("A_string_value=1");
  attribute_descriptor.AddValue("A_string_value=2");
  attribute_descriptor.AddValue("A_string_value=3");
  attribute_descriptor.AddValue("A_string_value=4");
  create_descriptor.AddAttribute(attribute_descriptor);

  // Reuse the ccb_descriptor and add the object creation
  CcbDescriptorCleaner(ccb_descriptor);
  ccb_descriptor.AddCreate(create_descriptor);

  // Do the modification
  // Set CCB flag to inform IMM that there is no Object Implementer
  modifier.SetCcbFlags(0);

  cout <<
  "In the request to create a Test12=1,safApp=safSmfService object " << endl <<
  "a string attribute is incorrectly given SA_IMM_ATTR_SAUINT32T as " << endl <<
  "IMM type. This shall fail in a controlled way" << endl;

  // Create the IMM object
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Creation of '" << object_name << "' FAIL" << endl;
    cout << "This fail is intentional so if we get here the test is SUCCESS"
         << endl << endl;

    cout << "Error information:" << endl;
    modelmodify::ErrorInformation error_info;
    modifier.GetErrorInformation(error_info);
    cout << "Failing API: '" << error_info.api_name << "' " << endl;
    cout << "AIS error code: " << saf_error(error_info.ais_error) << endl;
    cout << "This is an internal error so there shall be no valid AIS error "
            "code or AIS API-name" << endl;
    cout << "If 'Failng API:' is empty then test is SUCCESS" << endl;
  } else {
    cout << "Creation of '" << object_name << "' SUCCESS" << endl;
    cout << "Test case FAIL" << endl;
  }

  // cout << endl;
  cout << "END Test creation of Test12=1,safApp=safSmfService object that "
          "shall fail" << endl;


  // Test ignore_ais_err_exist flag in the
  // Try to create the same object twice.
  // First with ignore_ais_err_exist flag set to false. Shall Fail.
  // The with ignore_ais_err_exist flag set to true. Shall succeed.
  // ==============================================================
  cout << endl;
  cout << "Test creating an object twice:" << endl;
  cout << "First with ignore_ais_err_exist = false, shall Fail" << endl;
  cout << "Then with ignore_ais_err_exist = true, shall succeed. However "
          "no new object is created" << endl << endl;


  // Reuse the create descriptor
  create_descriptor.attributes.clear();
  object_name = "Test12=2";

  // Define the object
  // Note: No values other than the object name is given
  // -----------------
  create_descriptor.class_name = "ImmTestValuesConfig";
  create_descriptor.parent_name = "safApp=safSmfService";
  // Set IMM Object name
  attribute_descriptor.attribute_name = "immTestValuesCfg";
  attribute_descriptor.value_type = SA_IMM_ATTR_SASTRINGT;
  // We are reusing so remove "old" values and add new ones
  attribute_descriptor.values_as_strings.clear();
  attribute_descriptor.AddValue(object_name);
  create_descriptor.AddAttribute(attribute_descriptor);

  // Reuse the ccb_descriptor and add the object creation
  CcbDescriptorCleaner(ccb_descriptor);
  ccb_descriptor.AddCreate(create_descriptor);

  // Do the modification
  // Set CCB flag to inform IMM that there is no Object Implementer
  modifier.SetCcbFlags(0);

  cout << "1. First Create. Shall succeed" << endl;
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Creation of '" << object_name << "' FAIL" << endl;

    cout << "Error information:" << endl;
    modelmodify::ErrorInformation error_info;
    modifier.GetErrorInformation(error_info);
    cout << "Failing API: '" << error_info.api_name << "' " << endl;
    cout << "AIS error code: " << saf_error(error_info.ais_error) << endl;
    cout << endl;

    cout << "First object shall be created with no FAIL" << endl;
    cout << "Test FAIL" << endl;
  } else {
    cout << "Object created. Test SUCCESS" << endl;
  }
  cout << endl;

  cout << "2. Second create of same object shall fail. "
          "Object already exist" << endl;
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Creation of '" << object_name << "' FAIL" << endl;

    cout << "Error information:" << endl;
    modelmodify::ErrorInformation error_info;
    modifier.GetErrorInformation(error_info);
    cout << "Failing API: '" << error_info.api_name << "' " << endl;
    cout << "AIS error code: " << saf_error(error_info.ais_error) << endl;

    cout << "Second creation of same object shall FAIL. Test SUCCESS" << endl;
  } else {
    cout << "Second creation of same object did not FAIL. Test FAIL" << endl;
  }
  cout << endl;

  cout << "3. Third create of same object shall succeed." << endl;
  cout << "   Set ignore_ais_err_exist = true and try again." << endl;
  cout << "   This time the modification shall not fail" << endl;
  create_descriptor.ignore_ais_err_exist = true;
  ccb_descriptor.create_descriptors.clear();
  ccb_descriptor.AddCreate(create_descriptor);

  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Creation of '" << object_name << "' FAIL" << endl;

    cout << "Error information:" << endl;
    modelmodify::ErrorInformation error_info;
    modifier.GetErrorInformation(error_info);
    cout << "Failing API: '" << error_info.api_name << "' " << endl;
    cout << "AIS error code: " << saf_error(error_info.ais_error) << endl;

    cout << "Third creation of same object failed. Test FAIL" << endl;
  } else {
    cout << "Third creation of same object did not fail. Test SUCCESS" << endl;
  }
  cout << endl;

  // Try deleting an object that does not exist;
  // ignore_ais_err_not_exist = true
  // Shall SUCCEED but no object is deleted
  std::string object_name_not_exist = "Test12=100";
  cout << "Deleting object that does not exist. Flag ignore_ais_err_not_exist "
      "= true (default)" << endl;
  delete_descriptor.object_name = object_name_not_exist + "," +
                                  create_descriptor.parent_name;
  CcbDescriptorCleaner(ccb_descriptor);
  ccb_descriptor.AddDelete(delete_descriptor);
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Deleting object '" << delete_descriptor.object_name << "' FAIL"
         << endl;
    cout << "Test FAIL" << endl;
  } else {
    cout << "Test SUCCESS" << endl;
  }

  // Cleanup by deleting the test object
  delete_descriptor.object_name = object_name + "," +
                                  create_descriptor.parent_name;
  delete_descriptor.ignore_ais_err_not_exist = true;
  CcbDescriptorCleaner(ccb_descriptor);
  ccb_descriptor.AddDelete(delete_descriptor);
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Deleting object '" << delete_descriptor.object_name << "' FAIL"
         << endl;
  }

  // Try deleting an object that does not exist;
  // ignore_ais_err_not_exist = false
  cout << "Deleting object that does not exist. Flag ignore_ais_err_not_exist "
      "= false" << endl;
  delete_descriptor.object_name = object_name_not_exist + "," +
                                  create_descriptor.parent_name;
  delete_descriptor.ignore_ais_err_not_exist = false;
  CcbDescriptorCleaner(ccb_descriptor);
  ccb_descriptor.AddDelete(delete_descriptor);
  if (modifier.DoModelModification(ccb_descriptor) == false) {
    cout << "Deleting object '" << delete_descriptor.object_name << "' FAIL"
         << endl;
    cout << "Test SUCCESS" << endl;
  } else {
    cout << "Test FAIL" << endl;
  }

  return 0;
}
