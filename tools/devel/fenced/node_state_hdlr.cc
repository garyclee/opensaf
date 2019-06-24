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

#include "node_state_hdlr.h"

#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

NodeStateHdlr::NodeStateHdlr() {
  int sockflags = SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC;
  int sfd[2];
  if (socketpair(AF_UNIX, sockflags, 0, sfd) != 0) {
    syslog(LOG_ERR, "failed to create socketpair: %s", strerror(errno));
    read_fd_ = -1;
    write_fd_ = -1;
  } else {
    read_fd_ = sfd[0];
    write_fd_ = sfd[1];
  }
}

NodeStateHdlr::~NodeStateHdlr() {
  close(read_fd_);
  close(write_fd_);
}

void NodeStateHdlr::send_update() {
  int rc {0};
  do {
    rc = send(write_fd_, &isolated_, sizeof(isolated_), MSG_DONTWAIT | MSG_NOSIGNAL);
  } while (rc == -1 && errno == EINTR);

  if (rc != sizeof(isolated_)) {
    syslog(LOG_ERR, "failed to send: %s", strerror(errno));
  }
}

