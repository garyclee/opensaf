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

#ifndef NODE_STATE_FILE_H_
#define NODE_STATE_FILE_H_

#include <string>

#include "cpp_macros.h"
#include "node_state_hdlr.h"

class NodeStateFile {
 public:
  NodeStateFile();
  ~NodeStateFile();
  void update(NodeStateHdlr::NodeIsolationState isolated);
  void init();
 private:
  bool exists(const std::string& file);
  int create(const std::string file);

  std::string node_is_isolated_;
  std::string node_is_not_isolated_;
  DELETE_COPY_AND_MOVE_OPERATORS(NodeStateFile);
};

#endif  // NODE_STATE_FILE_H_
