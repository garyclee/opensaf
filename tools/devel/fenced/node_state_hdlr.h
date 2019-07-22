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

#ifndef NODE_STATE_HDLR_H_
#define NODE_STATE_HDLR_H_

#include <cinttypes>

#include "cpp_macros.h"

class NodeStateHdlr {
 public:
  enum class AwaitActiveTmrStatus {kNotRunning, kRunning, kExpired};
  enum class NodeIsolationState {kUndefined, kIsolated, kNotIsolated};

  NodeStateHdlr();
  virtual ~NodeStateHdlr();
  virtual int init() = 0;
  int get_event_fd() const {return read_fd_;}

 protected:
  const uint32_t DFLT_ISOLATION_STATUS_CHECK_TIME_SEC = 3;
  void send_update();
  NodeIsolationState isolated_ {NodeIsolationState::kIsolated};
 private:
  int write_fd_;
  int read_fd_;
  DELETE_COPY_AND_MOVE_OPERATORS(NodeStateHdlr);
};

#endif  // NODE_STATE_HDLR_
