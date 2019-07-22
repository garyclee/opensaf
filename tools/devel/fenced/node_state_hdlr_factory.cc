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

#include "node_state_hdlr_factory.h"

#include <syslog.h>
#include <algorithm>
#include <string>

#include "node_state_hdlr_sc.h"
#include "node_state_hdlr_pl.h"

// case insensitive string compare
static bool str_equal(std::string &str1, std::string &str2) {
  return ((str1.size() == str2.size()) &&
          std::equal(str1.begin(), str1.end(), str2.begin(), [] (char &c1, char &c2) {
              return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
            }));
}

NodeStateHdlrFactory::NodeStateHdlrFactory() {
  char *p = getenv("NODE_TYPE");
  std::string str;
  std::string ctl {"controller"};
  is_controller_ = false;

  if (p == NULL) {
    syslog(LOG_INFO, "no \"NODE_TYPE\" has been configured, defaults to payload node");
  } else {
    str = p;
    if (str_equal(str, ctl)) {
      is_controller_ = true;
    }
  }
}

NodeStateHdlrFactory::~NodeStateHdlrFactory() {
}

NodeStateHdlrFactory& NodeStateHdlrFactory::instance() {
  // in C++11 this method is thread safe, see §6.7 [stmt.dcl] p4
  static NodeStateHdlrFactory n;
  return n;
}

NodeStateHdlr* NodeStateHdlrFactory::create() {
  if (is_controller_) {
    return new NodeStateHdlrSc;
  } else {
    return new NodeStateHdlrPl;
  }
}
