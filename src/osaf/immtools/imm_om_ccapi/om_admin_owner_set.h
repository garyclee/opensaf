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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_SET_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_SET_H_

#include <vector>
#include <string>

#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "base/logtrace.h"

namespace immom {

//>
// Set Administration Owner to given object names.
// The default operation scope is SA_IMM_SUBTREE level.
//
// When the object goes out of scope, the administrative ownership is released
// on registered objects using the scope used when admin ownership was set.
//
// Use `ImmOmAdminOwnerRelease` to release admin ownership on specific object(s)
// or on different admin handle. Refer to ~/om_admin_owner_release.h
//
// Example:
// Become admin owner of two objects do something and explicitly release the
// admin ownership again
//
// ImmOmAdminOwnerSet adminset{adminhandle};
// adminset.SetObjectNames({dn1, dn2});
// if (adminset.SetAdminOwner() == false) {
//   // ERR handling
// }
//
// Do something...
//
// adminset.ReleaseAdminOwner();
//<
class ImmOmAdminOwnerSet : public ImmBase {
 public:
  // Construct the object with one object name or list of object names
  explicit ImmOmAdminOwnerSet(const SaImmAdminOwnerHandleT& admin_owner_handle);
  ~ImmOmAdminOwnerSet();

  // Add an object name to a list of objects to become admin owner of
  void AddObjectName(const std::string& object_name) {
    object_names_.push_back(object_name);
  }

  // Set a new OM handle. Needed when recovering from BAD_HANDLE
  void UpdateHandle(const SaImmAdminOwnerHandleT& admin_owner_handle) {
    admin_owner_handle_ = admin_owner_handle;
    is_set_admin_owner_invoked_ = false;
  }

  // Clear the list of object names
  // Note: this will not release the admin owner
  void ClearObjectNames(void) {
    object_names_.clear();
  }

  // Set Administrative owner to object names on given scope.
  // Returns false on error
  bool SetAdminOwner(SaImmScopeT scope = SA_IMM_SUBTREE);

  // Release administrative owner for registered objects with SAME scope
  // which has been set previously by SetAdminOwner().
  // Returns false on error
  bool ReleaseAdminOwner();

 private:
  std::vector<std::string> object_names_;
  SaImmScopeT scope_;
  bool is_set_admin_owner_invoked_;
  SaImmAdminOwnerHandleT admin_owner_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmAdminOwnerSet);
};

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_SET_H_
