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

#include "node_state_hdlr_sc.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <syslog.h>
#include <sys/types.h>
#include <cerrno>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

NodeStateHdlrSc::NodeStateHdlrSc() {
  syslog(LOG_WARNING, "NodeStateHdlrSc not supported");
}

NodeStateHdlrSc::~NodeStateHdlrSc() {
}

int NodeStateHdlrSc::init() {
  return 0;
}
