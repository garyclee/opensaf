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

#include <cerrno>
#include <cstring>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "tipc_server.h"

TIPCServer::TIPCServer() {
}

TIPCServer::~TIPCServer() {
  close(sd_);
}

int TIPCServer::init() {
  if ((sd_ = socket(AF_TIPC, SOCK_RDM, 0)) < 0) {
    syslog(LOG_ERR, "Failed to create TIPC socket for fenced svc: %s", strerror(errno));
    return -1;
  }
  get_node_id();
  tipc_bind(FMD_FENCED_SVC_TYPE, TIPC_CLUSTER_SCOPE);
  return 0;
}

void TIPCServer::get_node_id() {
  struct sockaddr_tipc addr;
  socklen_t sz = sizeof(addr);

  memset(&addr, 0, sizeof(addr));
  node_id_ = 0;

  if (getsockname(sd_, (struct sockaddr *)&addr, &sz) < 0) {
    syslog(LOG_ERR, "Failed to get socket name: %s", strerror(errno));
  }

  node_id_ = addr.addr.id.node;
}

int TIPCServer::tipc_bind(uint32_t service_type, signed char scope) {
  struct sockaddr_tipc server_addr;

  server_addr.family = AF_TIPC;
  server_addr.addrtype = TIPC_ADDR_NAMESEQ;
  server_addr.addr.nameseq.type = service_type;
  server_addr.scope = scope;

  server_addr.addr.nameseq.lower = node_id_;
  server_addr.addr.nameseq.upper = node_id_;

  if (bind(sd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    syslog(LOG_ERR, "Failed to bind TIPC port for fenced svc: %s", strerror(errno));
    return -1;
  } else {
    syslog(LOG_NOTICE, "Fenced svc %s port {%u,%u,%u} scope: %d", (scope < 0) ? "unbound" : "bound",
           server_addr.addr.nameseq.type, server_addr.addr.nameseq.lower,
           server_addr.addr.nameseq.upper, scope);
  }
  return 0;
}

void TIPCServer::publish() {
  if (!published_) {
    if (tipc_bind(FMD_FENCED_SVC_TYPE_ACTIVE, TIPC_CLUSTER_SCOPE) == 0) {
      published_ = true;
    }
  }
}

void TIPCServer::unpublish() {
  if (published_) {
    if (tipc_bind(FMD_FENCED_SVC_TYPE_ACTIVE, -TIPC_CLUSTER_SCOPE) == 0) {
      published_ = false;
    }
  }
}
