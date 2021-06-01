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

#include "om_admin_owner_clear.h"
#include <string>
#include "base/osaf_extended_name.h"

namespace immom {

//-----------------------------------------------------------------------------
// ImmOmAdminOwnerClear class
//-----------------------------------------------------------------------------
ImmOmAdminOwnerClear::ImmOmAdminOwnerClear(
    const SaImmHandleT& om_handle,
    const std::vector<std::string>& list_of_object_names)
    : ImmBase(),
      object_names_{list_of_object_names},
      om_handle_{om_handle} { }

ImmOmAdminOwnerClear::ImmOmAdminOwnerClear(
    const SaImmHandleT& om_handle,
    const std::string& object_name)
    : ImmOmAdminOwnerClear(om_handle, std::vector<std::string>{object_name}) {}

ImmOmAdminOwnerClear::~ImmOmAdminOwnerClear() { }

bool ImmOmAdminOwnerClear::ClearAdminOwner(SaImmScopeT scope) {
  TRACE_ENTER();
  const SaNameT** objectnames = nullptr;
  SaNameT* sanamets = nullptr;

  size_t size = object_names_.size();
  if (size > 0) {
    sanamets = new SaNameT[size];
    assert(sanamets != nullptr);
    objectnames = new const SaNameT*[size + 1]();
    assert(objectnames != nullptr);
    unsigned i = 0;
    for (const auto& name : object_names_) {
      osaf_extended_name_lend(name.c_str(), &sanamets[i]);
      objectnames[i] = &sanamets[i];
      i++;
    }
    objectnames[i] = nullptr;
  }

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmAdminOwnerClear(
        om_handle_,
        objectnames,
        scope);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }

  delete[] objectnames;
  delete[] sanamets;

  return (ais_error_ == SA_AIS_OK);
}

}  // namespace immom
