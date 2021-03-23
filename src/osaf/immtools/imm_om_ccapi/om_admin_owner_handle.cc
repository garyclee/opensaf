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

#include "om_admin_owner_handle.h"

namespace immom {

//------------------------------------------------------------------------------
// ImmOmAdminOwnerHandle class
//------------------------------------------------------------------------------
ImmOmAdminOwnerHandle::ImmOmAdminOwnerHandle(
    const SaImmHandleT& om_handle, const std::string& admin_owner_name)
    : ImmBase(),
      om_handle_{om_handle},
      admin_owner_name_{admin_owner_name},
      release_ownership_on_finalize_{SA_TRUE},
      admin_owner_handle_{0} { }

ImmOmAdminOwnerHandle::~ImmOmAdminOwnerHandle() {
  TRACE_ENTER();
  FinalizeHandle();
}

bool ImmOmAdminOwnerHandle::InitializeHandle() {
  TRACE_ENTER();
  ais_error_ = SA_AIS_OK;

  // The handle is already initialized.
  if (admin_owner_handle_ > 0) return true;

  char* admin_owner_name = const_cast<char*>(admin_owner_name_.c_str());
  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmAdminOwnerInitialize(
        om_handle_,
        admin_owner_name,
        release_ownership_on_finalize_,
        &admin_owner_handle_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }
  return (ais_error_ == SA_AIS_OK);
}

bool ImmOmAdminOwnerHandle::FinalizeHandle() {
  TRACE_ENTER();
  ais_error_ = SA_AIS_OK;
  if (admin_owner_handle_ == 0) return true;

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmAdminOwnerFinalize(admin_owner_handle_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }

  if (ais_error_ != SA_AIS_OK) admin_owner_handle_ = 0;
  return (ais_error_ == SA_AIS_OK);
}

SaImmAdminOwnerHandleT ImmOmAdminOwnerHandle::GetHandle() {
  TRACE_ENTER();
  // Handle has not been explicitly initialized
  if (admin_owner_handle_ == 0) InitializeHandle();
  return admin_owner_handle_;
}

}  // namespace immom
