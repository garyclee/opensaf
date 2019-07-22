/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2019 - All Rights Reserved.
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

#include "mds_tipc_recvq_stats.h"
#include "mds_tipc_recvq_stats_impl.h"


void mds_tipc_recvq_stats(int sd) {
  static TipcRecvqStatsImpl tipc_recvq_stats;

  if (tipc_recvq_stats.init(sd) == 0) {
    tipc_recvq_stats.start();
  }
}
