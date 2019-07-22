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

#include "watchdog.h"

#include <syslog.h>
#include <systemd/sd-daemon.h>
#include <cstring>

Watchdog::Watchdog() {
  interval_usec_ = DFLT_WATCHDOG_TIMEOUT;
  int rc = sd_watchdog_enabled(0, &interval_usec_);
  if (rc < 0) {
    syslog(LOG_WARNING, "sd_watchdog_enabled failed: %s", strerror(-rc));
  }
}

void Watchdog::kick() {
  int rc = sd_notify(0, "WATCHDOG=1");
  if (rc < 0) {
    syslog(LOG_WARNING, "sd_notify failed: %s", strerror(-rc));
  }
}
