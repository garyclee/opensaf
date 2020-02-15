/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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

#ifndef LOG_APITEST_LOG_SERVER_H_
#define LOG_APITEST_LOG_SERVER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void StartUnixServer();
bool FindPRI(const char* pri_field);
void StopUnixServer();

#ifdef __cplusplus
}  // closing brace for extern "C"
#endif

#endif  // LOG_APITEST_LOG_SERVER_H_
