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

#include "om_ccb_handle.h"

namespace immom {

//------------------------------------------------------------------------------
// ImmOmCcbHandle class
//------------------------------------------------------------------------------
ImmOmCcbHandle::ImmOmCcbHandle(
    const SaImmAdminOwnerHandleT& admin_owner_handle,
    SaImmCcbFlagsT flags)
    : ImmBase(),
      admin_owner_handle_{admin_owner_handle},
      ccb_flags_{flags},
      ccb_handle_{0} { }

ImmOmCcbHandle::ImmOmCcbHandle(
    const SaImmAdminOwnerHandleT& admin_owner_handle)
    : ImmOmCcbHandle{admin_owner_handle, SA_IMM_CCB_REGISTERED_OI} {}

ImmOmCcbHandle::~ImmOmCcbHandle() {
  FinalizeHandle();
}

bool ImmOmCcbHandle::InitializeHandle() {
  TRACE_ENTER();
  ais_error_ = SA_AIS_OK;

  // The admin_owner_handle is already initialized.
  if (ccb_handle_ > 0) return true;

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmCcbInitialize(
        admin_owner_handle_,
        ccb_flags_,
        &ccb_handle_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }
  return (ais_error_ == SA_AIS_OK);
}

bool ImmOmCcbHandle::FinalizeHandle() {
  TRACE_ENTER();
  ais_error_ = SA_AIS_OK;
  if (ccb_handle_ == 0) return true;

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmCcbFinalize(ccb_handle_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }

  if (ais_error_ == SA_AIS_OK) ccb_handle_ = 0;
  return (ais_error_ == SA_AIS_OK);
}

bool ImmOmCcbHandle::ApplyCcb() {
  TRACE_ENTER();
  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmCcbApply(ccb_handle_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }
  return (ais_error_ == SA_AIS_OK);
}

SaImmCcbHandleT ImmOmCcbHandle::GetHandle() {
  TRACE_ENTER();
  // Implicitly initialize handle
  if (ccb_handle_ == 0) InitializeHandle();
  return ccb_handle_;
}

}  // namespace immom
