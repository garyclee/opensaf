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

#ifndef NODE_STATE_HDLR_PL_H_
#define NODE_STATE_HDLR_PL_H_

#include <linux/tipc.h>
#include <cstdint>
#include <map>
#include "cpp_macros.h"
#include "timer.h"
#include "node_state_hdlr.h"

class NodeStateHdlrPl : public NodeStateHdlr {
 public:
  NodeStateHdlrPl();
  ~NodeStateHdlrPl();
  int init();

 private:
  const uint32_t DFLT_FMD_FENCED_SVC_TYPE = 19888;
  const uint32_t DFLT_FMD_FENCED_SVC_TYPE_ACTIVE = 19889;
  const uint32_t DFLT_AWAIT_ACTIVE_TIME_SEC = 10;

  struct NodeState {
    bool fmdIsActive {false};
    bool isController {false};
    bool nodeIsUp {false};
  };
  using NodeId = uint32_t;
  std::map<NodeId, NodeState*> node_map_;
  void handle_event(const tipc_event &evt);
  void receive();
  void check_isolation();
  int sd_;
  int read_fd_;
  uint32_t fmd_fenced_svc_type_ {DFLT_FMD_FENCED_SVC_TYPE};
  uint32_t fmd_fenced_svc_type_active_ {DFLT_FMD_FENCED_SVC_TYPE_ACTIVE};
  uint64_t isolation_status_check_time_us_;
  uint64_t await_active_time_us_;
  Timer await_act_tmr_;
  AwaitActiveTmrStatus await_act_tmr_status_ {AwaitActiveTmrStatus::kNotRunning};
  DELETE_COPY_AND_MOVE_OPERATORS(NodeStateHdlrPl);
};

#endif  // TIPC_CLIENT_H_
