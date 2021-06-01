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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_DELETE_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_DELETE_H_


#include <string>

#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "base/logtrace.h"

namespace immom {

//>
// Add an IMM configuration object delete request to a CCB
//
// The object and the entire subtree of objects rooted at the object will be
// deleted
//
// Example:
// Delete the IMM object with name (a complete dn must be given)
// "safLgStrCfg=myLogStream,safApp=safLogService"
// and commit the modification. Note that more modification requests can be
// added to the CCB before committing.
//
// ImmOmCcbHandle ccbhandleobj{adminhandle};
// SaImmCcbHandleT ccbhandle = ccbhandleobj.GetHandle();
//
// ImmOmCcbObjectDelete deleteobj{ccbhandle};
//
// std::string objname = "safLgStrCfg=myLogStream,safApp=safLogService"
// if (deleteobj.AddObjectDeleteToCcb(objname) == false) {
//   // Error handling...
//   return;
// }
//
// if (ccbhandleobj.ApplyCcb() ==false) {
//   // Error handling...
// }
//<
class ImmOmCcbObjectDelete : public ImmBase {
 public:
  explicit ImmOmCcbObjectDelete(const SaImmCcbHandleT& ccb_handle);
  ~ImmOmCcbObjectDelete();

  // Set a new CCB handle. Needed when recovering from BAD_HANDLE
  void UpdateHandle(const SaImmCcbHandleT& ccb_handle) {
    ccb_handle_ = ccb_handle;
  }

  // Returns false if failing
  // Use ais_error() to get returned AIS code.
  bool AddObjectDeleteToCcb(const std::string& object_name);

 private:
  SaImmCcbHandleT ccb_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmCcbObjectDelete);
};

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_OBJECT_DELETE_H_
