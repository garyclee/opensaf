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

#include "service.h"

#include <syslog.h>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <sstream>
#include <vector>

Service::Service() {
}

Service::~Service() {
}

void Service::init() {
  std::string str;
  char *p = getenv("SERVICES_TO_FENCE");

  if (p == NULL) {
    syslog(LOG_NOTICE, "no services to fence has been configured");
    return;
  } else {
    str = p;
  }

  // services separated by white space
  std::istringstream iss{str};
  std::vector<std::string> svc_to_fence(std::istream_iterator<std::string>{iss},
                                        std::istream_iterator<std::string>());

  syslog(LOG_NOTICE, "the following services are managed by fenced: %s", str.c_str());
  for (auto it : svc_to_fence) {
    services_[it] = {};
  }
}
