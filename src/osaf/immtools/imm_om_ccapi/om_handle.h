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

#ifndef OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_HANDLE_H_
#define OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_HANDLE_H_

#include "ais/include/saImmOm.h"
#include "common/common.h"
#include "base/logtrace.h"

namespace immom {
//>
// Manage IMM OM handle - the handle provides access to the Object Management
// APIs of IMM service. So, the method InitializeHandle() must be called
// before using any other IMM OM abstracts.
//
// The handle can be explicitly finalized using FinalizeHandle()
// but it is also finalized when object goes out of scope.
//
// Acquire IMM OM handle in two ways:
// 1) Implicitly invoke InitializeHandle(), then get the handle via GetHandle().
// 2) Invoke GetHandle(), then the handle is implicitly acquired.
//
// Example:
// Initialize and get an Object Manager handle
//
// SaVersionT version = { 'A', 2, 11 };
// ImmOmHandle omhandleobj{version};
// if (omhandleobj.InitializeHandle() == false) {
//   // ERR handling
//}
//
// SaImmHandleT omhandle = omhandleobj.GetHandle();
//<
class ImmOmHandle : public ImmBase {
 public:
  // Construct the object with default IMM version and given callbacks
  explicit ImmOmHandle(const SaImmCallbacksT* callbacks = nullptr);
  // Construct the object with given IMM version and given callbacks
  explicit ImmOmHandle(const SaVersionT& version,
                       const SaImmCallbacksT* callbacks = nullptr);
  ~ImmOmHandle();

// Return true if IMM OM handle is successfully initialized/finalized.
  // Use ais_error() to get AIS return code.
  bool InitializeHandle() {
    FinalizeHandle();
    om_handle_ = 0;
    return OmInitialize();
  }
  bool OmInitialize();
  bool FinalizeHandle();

  // Fetch the operating system handle associated with the handle OM handle.
  // Use ais_error() to get AIS return code.
  bool FetchSelectionObject();

  bool DispatchOne();
  bool DispatchAll();
  bool DispatchBlocking();

  // Get the IMM OM handle and initialize the handle if not yet initialized.
  // Use ais_error() to get AIS return code.
  SaImmHandleT GetHandle();

  // Get selection object and implicitly acquire the selection object from IMM
  // if needed.
  SaSelectionObjectT fd();

  // Get the IMM OM APIs version actually supported by the IMM
  // Should call this method in case of getting SA_AIS_ERR_VERSION
  // to know what version is supported by IMM.
  const SaVersionT GetSupportedVersion() const { return supported_version_; }

 private:
  bool Dispatch(SaDispatchFlagsT flag);

 private:
  SaSelectionObjectT fd_;
  const SaImmCallbacksT* callbacks_;
  SaVersionT version_;
  SaVersionT supported_version_;
  SaImmHandleT om_handle_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmOmHandle);
};

}  // namespace immom

#endif  // OSAF_IMMTOOLS_IMM_OM_CCAPI_OM_HANDLE_H_
