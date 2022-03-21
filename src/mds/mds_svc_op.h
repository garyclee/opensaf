/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2022 The OpenSAF Foundation
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

#ifndef MDS_SVC_OP_H_
#define MDS_SVC_OP_H_

#include "mds/mds_dt2c.h"
#include "mds/mds_papi.h"

uint32_t mds_svc_op_install(NCSMDS_INFO *info);
uint32_t mds_svc_op_uninstall(const NCSMDS_INFO *info);
uint32_t mds_svc_op_subscribe(const NCSMDS_INFO *info);
uint32_t mds_svc_op_unsubscribe(const NCSMDS_INFO *info);
uint32_t mds_svc_op_up(const MCM_SVC_UP_INFO *up_info);
uint32_t mds_svc_op_down(const MCM_SVC_DOWN_INFO *down_info);

#endif // MDS_SVC_OP_H_
