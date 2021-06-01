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

#include "om_admin_owner_set.h"
#include <string>
#include "base/osaf_extended_name.h"

namespace immom {

//-----------------------------------------------------------------------------
// ImmOmAdminOwnerSet class
//-----------------------------------------------------------------------------
ImmOmAdminOwnerSet::ImmOmAdminOwnerSet(
    const SaImmAdminOwnerHandleT& admin_owner_handle)
    : ImmBase(),
      scope_{SA_IMM_SUBTREE},
      is_set_admin_owner_invoked_{false},
      admin_owner_handle_{admin_owner_handle} { }

ImmOmAdminOwnerSet::~ImmOmAdminOwnerSet() {
  ReleaseAdminOwner();
}



bool ImmOmAdminOwnerSet::SetAdminOwner(SaImmScopeT scope) {
  TRACE_ENTER();
  const SaNameT** object_names = nullptr;

  scope_ = scope;
  SaNameT* sanamets = nullptr;
  size_t object_name_size = object_names_.size();
  if (object_name_size != 0) {
    sanamets = new SaNameT[object_name_size];
    assert(sanamets != nullptr);
    object_names = new const SaNameT*[object_name_size + 1]();
    assert(object_names != nullptr);
    unsigned i = 0;
    for (const auto& name : object_names_) {
      osaf_extended_name_lend(name.c_str(), &sanamets[i]);
      object_names[i] = &sanamets[i];
      i++;
    }
    object_names[i] = nullptr;
  }

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmAdminOwnerSet(admin_owner_handle_, object_names, scope);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }

  delete[] object_names;
  delete[] sanamets;

  if (ais_error_ == SA_AIS_OK) is_set_admin_owner_invoked_ = true;
  return (ais_error_ == SA_AIS_OK);
}

bool ImmOmAdminOwnerSet::ReleaseAdminOwner() {
  TRACE_ENTER();
  // Ensure no trouble when the object of this class runs out of its scope
  // while SetAdminOwner() not yet called.
  ais_error_ = SA_AIS_OK;
  if (is_set_admin_owner_invoked_ == false) return true;

  const SaNameT** object_names = nullptr;
  SaNameT* sanamets = nullptr;
  size_t object_name_size = object_names_.size();
  if (object_name_size != 0) {
    sanamets = new SaNameT[object_name_size];
    assert(sanamets != nullptr);
    object_names = new const SaNameT*[object_name_size + 1]();
    assert(object_names != nullptr);
    unsigned i = 0;
    for (const auto& name : object_names_) {
      osaf_extended_name_lend(name.c_str(), &sanamets[i]);
      object_names[i] = &sanamets[i];
      i++;
    }
    object_names[i] = nullptr;
  }

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmAdminOwnerRelease(
        admin_owner_handle_,
        object_names,
        scope_);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }

  delete[] object_names;
  delete[] sanamets;

  if (ais_error_ == SA_AIS_OK) is_set_admin_owner_invoked_ = false;
  return (ais_error_ == SA_AIS_OK);
}

}  // namespace immom
