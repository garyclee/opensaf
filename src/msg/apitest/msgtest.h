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

#ifndef MSG_APITEST_CLMTEST_H_
#define MSG_APITEST_CLMTEST_H_

#include "osaf/apitest/utest.h"
#include "osaf/apitest/util.h"

#define tet_printf printf

#define TET_PASS 0
#define TET_FAIL 1
#define TET_UNRESOLVED 2

#ifdef __cplusplus
#include "ais/include/saMsg.h"
SaMsgQueueHandleT openQueue(SaMsgHandleT msgHandle);
#endif

#endif
