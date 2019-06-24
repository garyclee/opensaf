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

#ifndef NODE_STATE_HDLR_FACTORY_H_
#define NODE_STATE_HDLR_FACTORY_H_

#include "node_state_hdlr.h"

class NodeStateHdlrFactory {
 public:
  NodeStateHdlr *create();
  static NodeStateHdlrFactory& instance();
 private:
  bool is_controller_ {false};
  NodeStateHdlrFactory();
  ~NodeStateHdlrFactory();

  DELETE_COPY_AND_MOVE_OPERATORS(NodeStateHdlrFactory);
};

#endif  // NODE_STATE_HDLR_FACTORY_H_
