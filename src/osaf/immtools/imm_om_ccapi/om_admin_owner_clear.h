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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_CLEAR_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_CLEAR_H_

#include <vector>
#include <string>
#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "base/logtrace.h"

namespace immom {

//>
// Clear Administration Owner of given object names.
// The default operation scope is SUBTREE level.
//
// Note: Normally "release" is used to give up admin ownership. Read
// "Object Administration Ownership" section in AIS for more information about
// when to use admin owner clear
//
// Example:
// Clear admin ownership of two objects
//
// ImmOmAdminOwnerClear adminclear{omhandle, {dn1, dn2}};
// if (adminclear.ClearAdminOwner() == false) {
//   // ERR handling
// }
//
//<
class ImmOmAdminOwnerClear : public ImmBase {
 public:
  // Construct the object with given object name or list of object names
  explicit ImmOmAdminOwnerClear(
      const SaImmHandleT& om_handle,
      const std::string& object_name);
  explicit ImmOmAdminOwnerClear(
      const SaImmHandleT& om_handle,
      const std::vector<std::string>& list_of_object_names);

  ~ImmOmAdminOwnerClear();

  // Set a new OM handle. Needed when recovering from BAD_HANDLE
  void UpdateHandle(const SaImmHandleT& om_handle) {
    om_handle_ = om_handle;
  }

  // Set new objects to clear
  void SetObjectList(const std::vector<std::string>& list_of_object_names) {
    object_names_.clear();
    object_names_ = list_of_object_names;
  }

  // Clear admin owner of object names on given scope.
  // Use ais_error() to get returned AIS code.
  bool ClearAdminOwner(SaImmScopeT scope = SA_IMM_SUBTREE);

 private:
  std::vector<std::string> object_names_;
  SaImmHandleT om_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmAdminOwnerClear);
};

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_ADMIN_OWNER_CLEAR_H_
