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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_HANDLE_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_HANDLE_H_

#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "base/logtrace.h"

namespace immom {

//>
// Manage (acquire/finalize/apply) CCB handle
//
// The handle is created with default CCB flag SA_IMM_CCB_REGISTERED_OI. If
// validation by OI is not wanted, construct the object with flags = 0.
//
// Once a CCB handle has been initialized, modification requests can be added
// to a CCB, A modification can be object create, object delete or object
// modify.
// When all modifications has been added to the CCB, the CCB is committed
// (ApplyCcb())
// The CCB is applied with all-or-nothing semantics (either all
// modification requests are applied or none). The modification requests are
// applied in the order they have been added to the CCB.
//
// The CCB handle is implicitly finalized when this object goes out of scope.
//
// Example:
// Initialize and get a CCB handle
//
// ImmOmCcbHandle ccbhandleobj{ownerhandle};
// if (ccbhandleobj.InitializeHandle() == false) {
//   // ERR handling
// }
//
// SaImmCcbHandleT ccbhandle = ccbhandleobj.GetHandle();
//<
class ImmOmCcbHandle : public ImmBase {
 public:
  explicit ImmOmCcbHandle(const SaImmAdminOwnerHandleT& admin_owner_handle,
                          SaImmCcbFlagsT flags);
  explicit ImmOmCcbHandle(const SaImmAdminOwnerHandleT& admin_owner_handle);
  ~ImmOmCcbHandle();

  // Returns false on error.
  // Use ais_error() to get AIS return code.
  bool InitializeHandle();
  // Initialize with a new AO handle. Needed for recovery
  bool InitializeHandle(const SaImmAdminOwnerHandleT& admin_owner_handle) {
    FinalizeHandle();
    admin_owner_handle_ = admin_owner_handle;
    ccb_handle_ = 0;
    return InitializeHandle();
  }
  bool FinalizeHandle();
  bool ApplyCcb();

  // Get the IMM OM handle and initialize the handle if not yet initialized.
  // Return 0 (invalid handle) if initializing handle fail.
  // Use ais_error() to get AIS return code.
  SaImmCcbHandleT GetHandle();

 private:
  SaImmAdminOwnerHandleT admin_owner_handle_;
  SaImmCcbFlagsT ccb_flags_;
  SaImmCcbHandleT ccb_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmCcbHandle);
};

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_CCB_HANDLE_H_
