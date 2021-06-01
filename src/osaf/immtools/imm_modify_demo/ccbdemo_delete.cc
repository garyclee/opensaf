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

#if 0  // Low level OM interface
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
// Note: Before running this code an object to delete must have been created
//       This can be done by running ccbdemo_create
int main() {
  cout << "Deleting an object using 'immom' directly. Object must exist" <<
      endl;
  cout << "Class: ImmTestValuesConfig" << endl;
  cout << "Delete: TestObj1=1,safApp=safSmfService" << endl << endl;

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

  cout << "Become AO of the object and sub tree to be deleted" << endl;
  immom::ImmOmAdminOwnerSet ao_setter(ao_handle.GetHandle());
  ao_setter.AddObjectName("TestObj1=1,safApp=safSmfService");  // Object
  if (ao_setter.SetAdminOwner(SA_IMM_SUBTREE) == false) {
    cout << "AO Set Fail, " << ao_setter.ais_error_string() << endl;
    return -1;
  }

  cout << "Add delete operation to CCB" << endl;
  immom::ImmOmCcbObjectDelete deleter(ccb_handle.GetHandle());
  if (deleter.AddObjectDeleteToCcb("TestObj1=1,safApp=safSmfService")
                                   == false) {
    cout << "AddObjectDeleteToCcb() Fail, " << deleter.ais_error_string()
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

#if 1  // Using ccb handler to delete an object
int main() {
  cout << "Deleting 2 objects using 'modelmodify'" << endl;
  cout << "Delete: TestObj1=1,safApp=safSmfService" << endl;
  cout << "Delete root object: TestObj1=2" << endl << endl;

#if 1  // Enable trace
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

#if 0
  // Prepare/enable extended name
  // ----------------------------
  setenv("SA_ENABLE_EXTENDED_NAMES", "1", 1);
  osaf_extended_name_init();
  // Note: Long DN must be configured in IMM configuration object before
  // running ccbdemo_...
  // DN for that object is opensafImm=opensafImm,safApp=safImmService
#endif

  modelmodify::CcbDescriptor test_ccb;
  modelmodify::DeleteDescriptor delete_object;

  delete_object.object_name = "TestObj1=1,safApp=safSmfService";
  test_ccb.AddDelete(delete_object);

  // Here we test to reuse the Delete Descriptor and to delete a root object
  delete_object.object_name = "TestObj2=1";
  test_ccb.AddDelete(delete_object);

  modelmodify::ModelModification model_modifier;
  model_modifier.SetCcbFlags(0);  // No OI for this class/objects of this class
  if (model_modifier.DoModelModification(test_ccb) == false) {
    cout << "DoModification() Fail" << endl;
  }

  cout << endl << "ccbdemo_delete Done" << endl;

  return 0;
}

#endif
