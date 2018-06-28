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

#ifndef SMFUTILS_OBJEXIST_H
#define SMFUTILS_OBJEXIST_H

#include <string>

#include "ais/include/saAis.h"
#include "ais/include/saImmOm.h"

#include "smf/smfd/imm_modify_config/immccb.h"

// Check if an IMM object exists.
// Return: kOk if object exist, kNotExist if not exist and kFail if failing to
//         get the information
// Note1: An IMM access operation is used. An IMM access operation is fast and
//        done locally on the node.
// Note2: To make many checks (in a loop) more efficient the handles are
//        created once.
class CheckObjectExist {
 public:
  enum ReturnCode { kOk, kRestartOm, kFail, kNotExist };

  CheckObjectExist()
    : is_initialized_(false), om_handle_(0), accessor_handle_(0) {}
  ~CheckObjectExist() { saImmOmFinalize(om_handle_); }

  // Check if the object exist. If fail use recovery possibilities
  // The Object DN is created from class and parent name
  ReturnCode IsExisting(modelmodify::CreateDescriptor& create_descriptor);

 private:
  // Update om_handle_
  ReturnCode OmInitialize();

  // Update accessor_handle_
  ReturnCode AccesorInitialize();

  // Check object existence
  ReturnCode AccessorGet(std::string object_dn);

  // Update object_rdn_
  ReturnCode GetObjectRdn(modelmodify::CreateDescriptor& create_descriptor );

  std::string object_rdn_;
  bool is_initialized_;
  SaImmHandleT om_handle_;
  SaImmAccessorHandleT accessor_handle_;
  const SaVersionT kImmVersion = {'A', 2, 17};
};

#endif /* SMFUTILS_OBJEXIST_H */
