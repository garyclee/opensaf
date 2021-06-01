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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_HANDLE_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_HANDLE_H_

#include <string>
#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "base/logtrace.h"

namespace immom {

//>
// Manage Administration Ownership handle
// The handle is implicitly finalized when running out of its scope.
//
// Set an admin owner name and get a related admin owner handle
// Before initializing is done it can be decided whether the admin ownership
// shall be released or not when the handle is finalized.
// Default behavior is that the ownership is released.
//
// Example:
// Get an admin owner handle for admin owner name "MyAdmin". Admin ownership
// shall be kept when handle is finalized
//
// ImmOmAdminOwnerHandle adminhandle{omhandle, "MyAdmin"};
// adminhandle.KeepOwnershipOnFinalize();
//
// if (adminhandle.InitializeHandle() == false) {
//   // ERR handling
// }
//
// SaImmAdminOwnerHandleT ownerhandle = adminhandle.GetHandle();
//<
class ImmOmAdminOwnerHandle : public ImmBase {
 public:
  // Construct the object with given admin owner name
  explicit ImmOmAdminOwnerHandle(const SaImmHandleT& om_handle,
                                 const std::string& admin_owner_name);
  ~ImmOmAdminOwnerHandle();


  // Use before InitializeHandle() if admin ownership shall be kept when handle
  // is finalized
  ImmOmAdminOwnerHandle& KeepOwnershipOnFinalize() {
    release_ownership_on_finalize_ = SA_FALSE;
    return *this;
  }

  // Use before InitializeHandle() to explicitly set that admin ownership shall
  // be released at finalize. This is the implicit behavior if no other setting
  // is done
  ImmOmAdminOwnerHandle& ReleaseOwnershipOnFinalize() {
    release_ownership_on_finalize_ = SA_TRUE;
    return *this;
  }

  // Return true if IMM Admin Owner handle is successfully initialized
  // or finalized, false otherwise.
  // Use ais_error() to get AIS return code.
  bool InitializeHandle();
  // Initialize with a new OM handle. Needed for recovery
  bool InitializeHandle(const SaImmHandleT& om_handle) {
    FinalizeHandle();
    om_handle_ = om_handle;
    admin_owner_handle_ = 0;
    return InitializeHandle();
  }
  bool FinalizeHandle();

  // Get the IMM Admin Owner handle and implicitly do initializing if the handle
  // has not been yet initialized.  Return invalid handle (0) if initializing
  // handle fails. Use ais_error() to get AIS return code.
  SaImmAdminOwnerHandleT GetHandle();

 private:
  SaImmHandleT om_handle_;
  std::string admin_owner_name_;
  SaBoolT release_ownership_on_finalize_;
  SaImmAdminOwnerHandleT admin_owner_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmAdminOwnerHandle);
};

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_HANDLE_H_
