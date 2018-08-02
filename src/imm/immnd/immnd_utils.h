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
 */

#ifndef IMM_IMMND_IMMND_UTILS_H_
#define IMM_IMMND_IMMND_UTILS_H_

#include "imm/immnd/immnd.h"
#include <saAis.h>

#ifdef __cplusplus
extern "C" {
#endif

// There is an enhancement from ticket #2898 which adds 2 functions:
//    - SyslogRecentFevs
//    - CollectRecentFevsLogData
// These functions are used to save information about the latest sync
// requests received via FEVS and write the information to the syslog
// if the IMNND goes down.
void SyslogRecentFevs();
void CollectRecentFevsLogData(const IMMND_EVT *evt, SaUint64T msg_no);

#ifdef __cplusplus
}
#endif

#endif  // IMM_IMMND_IMMND_UTILS_H_
