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

#ifndef TIPC_SERVER_H_
#define TIPC_SERVER_H_

#include <linux/tipc.h>
#include <cinttypes>
#include "base/macros.h"

class TIPCServer {
 public:
  const uint32_t FMD_FENCED_SVC_TYPE = 19888;
  const uint32_t FMD_FENCED_SVC_TYPE_ACTIVE = 19889;

  TIPCServer();
  ~TIPCServer();
  int init();
  void publish();
  void unpublish();
 protected:
 private:
  void get_node_id();
  int tipc_bind(uint32_t service_type, signed char scope);
  bool published_ {false};
  uint32_t node_id_;
  int sd_;
  DELETE_COPY_AND_MOVE_OPERATORS(TIPCServer);
};

#endif  // TIPC_CLIENT_H_
