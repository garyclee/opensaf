/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
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

#include <string>
#include <vector>

#include "osaf/immtools/imm_modify_config/immccb.h"

#include "ais/include/saImm.h"
#include "ais/include/saAis.h"

// The functions in here are helper functions that wraps some of the IMM APIs
// in imm_om_ccapi directory adapting them to how the input data for CCBs are
// handled in the ObjectModification class

#ifndef OSAF_IMMTOOLS_IMM_MODIFY_CONFIG_ADD_OPERATION_TO_CCB_H_
#define OSAF_IMMTOOLS_IMM_MODIFY_CONFIG_ADD_OPERATION_TO_CCB_H_

namespace modelmodify {

// Get AIS error information. Has valid information if add operation returns
// kFail. See immccb.h ErrorInformation
void GetAddToCbbErrorInfo(ErrorInformation *error_info);

// Check if the reason for SA_AIS_ERR_FAILED_OPERATION is a "resorce abort"
// Can be used immediately after an add operation or an apply operation has
// returned SA_AIS_ERR_FAILED_OPERATION
// Note:  SA_AIS_ERR_FAILED_OPERATION can be returned for two reasons:
//        1.  Validation error.
//            This is a Fail. Cannot be recovered
//        2.  Resource abort.
bool IsResorceAbort(const SaImmCcbHandleT& ccbHandle);

// Add one create operation to a CCB
// Recovery:  BAD HANDLE; kRestartOm
//            FAILED OPERATION; kRestartOm or kFail
//            BUSY; An admin operation is ongoing on an object to be deleted
//                  We can try again to add the create
// return: Recovery information. See immccb.h
int AddCreateToCcb(const SaImmCcbHandleT& ccb_handle,
                         const CreateDescriptor&
                         create_descriptor);

// Add one delete operation to a CCB
// Recovery:  BAD HANDLE; kRestartOm
//            FAILED OPERATION; kRestartOm or kFail
//            BUSY; An admin operation is ongoing on an object to be deleted
//                  We can try again to add the create
// return: Recovery information. See immccb.h
int AddDeleteToCcb(const SaImmCcbHandleT& ccb_handle,
                         const DeleteDescriptor&
                         delete_descriptor);

// Add one modify operation to a CCB
// Recovery:  BAD HANDLE; kRestartOm
//            FAILED OPERATION; kRestartOm or kFail
//            BUSY; An admin operation is ongoing on an object to be modified
//                  We can try again to add the modify
// return: Recovery information. See immccb.h
int AddModifyToCcb(const SaImmCcbHandleT& ccb_handle,
                         const ModifyDescriptor&
                         modify_descriptor);

}  // namespace modelmodify

#endif // OSAF_IMMTOOLS_IMM_MODIFY_CONFIG_ADD_OPERATION_TO_CCB_H_
