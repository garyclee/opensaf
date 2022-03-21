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

#ifndef MDS_DOWN_TIMER_OP_H_
#define MDS_DOWN_TIMER_OP_H_

#include "mds/mds_core.h"

void mds_adest_op_down_timer_start(MDS_DEST adest, MDS_SVC_ID svc_id);
void mds_adest_op_down_timer_stop(MDS_ADEST_INFO *adest_info);
void mds_adest_op_list_cleanup(void);

#endif // MDS_DOWN_TIMER_OP_H_
