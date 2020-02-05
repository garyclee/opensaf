/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2017 The OpenSAF Foundation
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
 * Author(s): Genband
 *
 */

#ifndef LCK_APITEST_CLMTEST_H_
#define LCK_APITEST_CLMTEST_H_

#include "osaf/apitest/utest.h"
#include "osaf/apitest/util.h"

#define tet_printf printf

#define TET_PASS 0
#define TET_FAIL 1
#define TET_UNRESOLVED 2

#ifdef __cplusplus
SaAisErrorT lockResourceOpen(SaLckHandleT lckHandle,
                             const SaNameT& name,
                             SaLckResourceOpenFlagsT flags,
                             SaTimeT timeout,
                             SaLckResourceHandleT& lockResourceHandle);

SaAisErrorT lockResourceOpenAsync(SaLckHandleT lckHandle,
                                  SaInvocationT invocation,
                                  const SaNameT& name,
                                  SaLckResourceOpenFlagsT flags);

SaAisErrorT lockResourceClose(SaLckResourceHandleT);

SaAisErrorT lockResourceLockAsync(SaLckResourceHandleT lockResourceHandle,
                                  SaInvocationT invocation,
                                  SaLckLockIdT *lockId,
                                  SaLckLockModeT lockMode,
                                  SaLckLockFlagsT lockFlags,
                                  SaLckWaiterSignalT waiterSignal);

SaAisErrorT lockResourceLock(SaLckResourceHandleT lockResourceHandle,
                             SaLckLockIdT *lockId,
                             SaLckLockModeT lockMode,
                             SaLckLockFlagsT lockFlags,
                             SaLckWaiterSignalT waiterSignal,
                             SaTimeT timeout,
                             SaLckLockStatusT *lockStatus);

SaAisErrorT lockPurge(SaLckResourceHandleT lockResourceHandle);
#endif

#endif
