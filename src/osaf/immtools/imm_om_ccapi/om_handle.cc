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

#include "om_handle.h"

namespace immom {

//------------------------------------------------------------------------------
// ImmOmHandle class
//------------------------------------------------------------------------------
ImmOmHandle::ImmOmHandle(const SaImmCallbacksT* callbacks)
    : ImmOmHandle{ImmBase::GetDefaultImmVersion(), callbacks} {}

ImmOmHandle::ImmOmHandle(const SaVersionT& version,
                         const SaImmCallbacksT* callbacks)
    : ImmBase(),
      fd_{0},
      callbacks_{callbacks},
      om_handle_{0} {
  version_ = supported_version_ = version;
}

ImmOmHandle::~ImmOmHandle() {
  TRACE_ENTER();
  FinalizeHandle();
}

// Initialize @om_handle_ from IMM
bool ImmOmHandle::OmInitialize() {
  TRACE_ENTER();
  SaVersionT version = version_;
  ais_error_ = SA_AIS_OK;

  // The IMM OM handle is already initialized.
  if (om_handle_ > 0) return true;

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmInitialize(&om_handle_, callbacks_, &version);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      version = version_;
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }

  supported_version_ = version;
  return (ais_error_ == SA_AIS_OK);
}

bool ImmOmHandle::FinalizeHandle() {
  TRACE_ENTER();
  ais_error_ = SA_AIS_OK;
  if (om_handle_ == 0) return true;

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmFinalize(om_handle_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }

  if (ais_error_ == SA_AIS_OK) fd_ = om_handle_ = 0;
  return (ais_error_ == SA_AIS_OK);
}

SaImmHandleT ImmOmHandle::GetHandle() {
  // Implicitly initialize handle
  if (om_handle_ == 0) OmInitialize();
  return om_handle_;
}

bool ImmOmHandle::FetchSelectionObject() {
  ais_error_ = SA_AIS_OK;
  if (fd_ > 0) return true;

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmSelectionObjectGet(om_handle_, &fd_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }
  return (ais_error_ == SA_AIS_OK);
}

SaSelectionObjectT ImmOmHandle::fd() {
  // Implicitly initialize the selection object.
  if (fd_ == 0) FetchSelectionObject();
  return fd_;
}

bool ImmOmHandle::Dispatch(SaDispatchFlagsT flag) {
  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmDispatch(om_handle_, flag);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }
  return (ais_error_ == SA_AIS_OK);
}

bool ImmOmHandle::DispatchOne() {
  return Dispatch(SA_DISPATCH_ONE);
}

bool ImmOmHandle::DispatchAll() {
  return Dispatch(SA_DISPATCH_ALL);
}

bool ImmOmHandle::DispatchBlocking() {
  return Dispatch(SA_DISPATCH_BLOCKING);
}

}  // namespace immom
