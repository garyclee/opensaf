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

#ifndef MDS_TIPC_RECVQ_STATS_IMPL_
#define MDS_TIPC_RECVQ_STATS_IMPL_

class TipcRecvqStatsImpl {
 public:
  int init(int sd);
  void start();
 private:
  int get_tipc_recvq_used(int *optval);
  void tipc_recvq_stats_bg();
  int create_timer();
  int start_timer();
  int stop_timer();

  int sd_{-1};
  double recvq_size_{0};
  int timer_fd_{-1};
  long log_freq_{0};
};

#endif  // MDS_TIPC_RECVQ_STATS_IMPL_
