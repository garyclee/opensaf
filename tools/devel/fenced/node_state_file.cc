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

#include "node_state_file.h"

#include <syslog.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>

NodeStateFile::NodeStateFile() {
}

NodeStateFile::~NodeStateFile() {
  unlink(node_is_isolated_.c_str());
  unlink(node_is_not_isolated_.c_str());
}

bool NodeStateFile::exists(const std::string& file) {
  return (access(file.c_str(), F_OK ) != -1);
}

int NodeStateFile::create(const std::string file) {
  std::ofstream outfile(file);
  if (outfile.good()) {
    syslog(LOG_NOTICE, "file \"%s\" created", file.c_str());
    return -1;
  } else {
    syslog(LOG_ERR, "failed to create file \"%s\"", file.c_str());
    return 0;
  }
}

void NodeStateFile::update(NodeStateHdlr::NodeIsolationState isolated) {
  if (isolated == NodeStateHdlr::NodeIsolationState::kIsolated) {
    if (exists(node_is_not_isolated_)) {
      if (std::rename(node_is_not_isolated_.c_str(),
                        node_is_isolated_.c_str()) != 0) {
        syslog(LOG_ERR, "failed to rename \"%s\" to \"%s\"",
               node_is_not_isolated_.c_str(), node_is_isolated_.c_str());
      } else {
        syslog(LOG_NOTICE, "file \"%s\" renamed to \"%s\"",
               node_is_not_isolated_.c_str(), node_is_isolated_.c_str());
      }
    } else if (!exists(node_is_isolated_)) {
      create(node_is_isolated_);
    }
  } else {
    if (exists(node_is_isolated_)) {
      if (std::rename(node_is_isolated_.c_str(), node_is_not_isolated_.c_str()) != 0) {
        syslog(LOG_ERR, "failed to rename \"%s\" to \"%s\"",
               node_is_isolated_.c_str(), node_is_not_isolated_.c_str());
      } else {
        syslog(LOG_NOTICE, "file \"%s\" renamed to \"%s\"", node_is_isolated_.c_str(), node_is_not_isolated_.c_str());
      }
    } else if (!exists(node_is_not_isolated_)) {
      create(node_is_not_isolated_);
    }
  }
}

void NodeStateFile::init() {
  char *p = getenv("NODE_STATE_FILE_DIR");
  std::string str;

  if (p == NULL) {
    syslog(LOG_ERR, "no \"NODE_STATE_FILE_DIR\" has been configured");
    return;
  }
  str = p;
  node_is_isolated_ = str + "/" + "osaf_node_is_isolated";
  node_is_not_isolated_ = str + "/" + "osaf_node_is_not_isolated";
}
