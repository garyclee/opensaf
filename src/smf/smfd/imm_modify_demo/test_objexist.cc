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

#include <limits.h>

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <iostream>

#include "ais/include/saImm.h"
#include "ais/include/saAis.h"
#include "base/osaf_extended_name.h"

#include "smf/smfd/SmfUtils_ObjExist.h"

#include "smf/smfd/imm_modify_config/immccb.h"

using namespace std;

int main() {
  CheckObjectExist e_check;
  CheckObjectExist::ReturnCode exist_rc;

  printf("Testing");

  cout << "Find an object test:" << endl;
  modelmodify::CreateDescriptor existing_object;
  modelmodify::AttributeDescriptor log_config;
  log_config.attribute_name = "logConfig";
  log_config.value_type = SA_IMM_ATTR_SASTRINGT;
  log_config.AddValue("logConfig=1");
  existing_object.class_name = "OpenSafLogConfig";
  existing_object.parent_name = "safApp=safLogService";
  existing_object.AddAttribute(log_config);

  std::string object_dn = "logConfig=1,safApp=safLogService";
  cout << "Object to check '" << object_dn << "'" << endl;
  exist_rc = e_check.IsExisting(existing_object);
  if (exist_rc == e_check.kFail) {
    cout << "Check failed" << endl;
  } else if (exist_rc == e_check.kOk) {
    cout << "Object exist" << endl;
  } else if (exist_rc == e_check.kNotExist) {
    cout << "Object does not exist" << endl;
  } else {
    cout << "Fail, unknown return code" << endl;
  }
  cout << endl;


  object_dn = "Kalle";
  cout << "Object to check '" << object_dn << "'" << endl;
  log_config.values_as_strings.clear();
  log_config.AddValue("Kalle");
  existing_object.attributes.clear();
  existing_object.AddAttribute(log_config);
  exist_rc = e_check.IsExisting(existing_object);
  if (exist_rc == e_check.kFail) {
    cout << "Check failed" << endl;
  } else if (exist_rc == e_check.kOk) {
    cout << "Object exist" << endl;
  } else if (exist_rc == e_check.kNotExist) {
    cout << "Object does not exist" << endl;
  } else {
    cout << "Fail, unknown return code" << endl;
  }
  cout << endl;


  object_dn = "logConfig=1";
  cout << "Object to check '" << object_dn << "'" << endl;
  log_config.values_as_strings.clear();
  log_config.AddValue("logConfig=1");
  existing_object.attributes.clear();
  existing_object.parent_name.clear();
  existing_object.AddAttribute(log_config);
  exist_rc = e_check.IsExisting(existing_object);
  if (exist_rc == e_check.kFail) {
    cout << "Check failed" << endl;
  } else if (exist_rc == e_check.kOk) {
    cout << "Object exist" << endl;
  } else if (exist_rc == e_check.kNotExist) {
    cout << "Object does not exist" << endl;
  } else {
    cout << "Fail, unknown return code" << endl;
  }
  cout << endl;


  object_dn = "";
  cout << "Object to check '" << object_dn << "'" << endl;
  log_config.values_as_strings.clear();
  //log_config.AddValue("logConfig=1");
  existing_object.attributes.clear();
  existing_object.parent_name.clear();
  existing_object.AddAttribute(log_config);
  exist_rc = e_check.IsExisting(existing_object);
  if (exist_rc == e_check.kFail) {
    cout << "Check failed" << endl;
  } else if (exist_rc == e_check.kOk) {
    cout << "Object exist" << endl;
  } else if (exist_rc == e_check.kNotExist) {
    cout << "Object does not exist" << endl;
  } else {
    cout << "Fail, unknown return code" << endl;
  }
  cout << endl;


  return 0;
}
