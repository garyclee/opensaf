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

#include "om_ccb_object_delete.h"
#include "base/osaf_extended_name.h"
#include "base/logtrace.h"

namespace immom {

//------------------------------------------------------------------------------
// ImmOmCcbObjectDelete class
//------------------------------------------------------------------------------
ImmOmCcbObjectDelete::ImmOmCcbObjectDelete(const SaImmCcbHandleT& ccb_handle)
    : ImmBase(),
      ccb_handle_{ccb_handle} { }

ImmOmCcbObjectDelete::~ImmOmCcbObjectDelete() {}

bool ImmOmCcbObjectDelete::AddObjectDeleteToCcb(const std::string& dn) {
  TRACE_ENTER();

  if (dn.empty() == true) {
    TRACE_2("Incorrect object name");
    ais_error_ = SA_AIS_ERR_INVALID_PARAM;
    return false;
  }

  // Create the object name parameter
  SaNameT object_name;
  osaf_extended_name_lend(dn.c_str(), &object_name);

  base::Timer wtime(retry_ctrl_.timeout);
  while (wtime.is_timeout() == false) {
    ais_error_ = saImmOmCcbObjectDelete(
        ccb_handle_,
        &object_name);
    if (ais_error_ == SA_AIS_ERR_TRY_AGAIN) {
      base::Sleep(retry_ctrl_.interval);
      continue;
    }
    break;
  }
  return (ais_error_ == SA_AIS_OK);
}

}  // namespace immom
