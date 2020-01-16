/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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

#include "log/apitest/log_server.h"
#include "base/unix_server_socket.h"

static base::UnixServerSocket* server = nullptr;

void StartUnixServer() {
  server = new base::UnixServerSocket("/tmp/test.sock",
                                      base::UnixSocket::kBlocking, 0777);
  server->fd();
}

bool FindPRI(const char* pri_field) {
  char buf[1024];
  memset(buf, 0, sizeof(buf));
  size_t len = 0;
  do {
    len = server->Recv(buf + len, 1024);
  } while (len < strlen(pri_field));
  return strncmp(buf, pri_field, strlen(pri_field)) == 0;
}

void StopUnixServer() { delete server; }
