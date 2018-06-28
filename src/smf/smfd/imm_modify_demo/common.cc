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

#include "smf/smfd/imm_modify_demo/common.h"

#include <stdlib.h>

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <iostream>

// This file is a place to put functions etc. used in more than one program in
// this directory

using namespace std;

bool ExecuteCommand(const char* command) {
  bool function_rc = true;

  int rc = system(command);
  if (rc == -1) {
    cout << "system() retuned -1 FAIL" << endl;
    function_rc = false;
  } else {
    rc = WEXITSTATUS(rc);
    if (rc != 0) {
      cout << "Command '" << command << "' ,FAIL" << endl;
      function_rc = false;
    }
  }

  return function_rc;
}

bool EnableImmLongDn(void) {
  const char *cmd_str = "immcfg -a longDnsAllowed=1 "
                "opensafImm=opensafImm,safApp=safImmService";
  return ExecuteCommand(cmd_str);
}

bool InstallDemoClass(void) {
  const char *cmd_str = "immcfg -f /usr/local/share/opensaf/immxml/services/"
                    "democlass.xml";
  bool rc = ExecuteCommand(cmd_str);
  if (rc == false) {
    cout << "Installation of democlass.xml Failed" << endl;
    cout << "Check: Is build configured with --enable-immxml?" << endl;
  }

  return rc;
}
