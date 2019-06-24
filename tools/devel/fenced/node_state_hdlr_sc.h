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

#ifndef NODE_STATE_HDLR_SC_H_
#define NODE_STATE_HDLR_SC_H_

#include <linux/tipc.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <atomic>
#include <cstdint>

#include "cpp_macros.h"
#include "node_state_hdlr.h"
#include "timer.h"

class NodeStateHdlrSc : public NodeStateHdlr {
 public:
  NodeStateHdlrSc();
  ~NodeStateHdlrSc();
  int init();
 private:
  DELETE_COPY_AND_MOVE_OPERATORS(NodeStateHdlrSc);
};

#endif  // NODE_STATE_HDLR_SC_H_
