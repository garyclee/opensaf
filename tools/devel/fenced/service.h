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

#ifndef SERVICE_H_
#define SERVICE_H_

#include <map>
#include <string>

#include "cpp_macros.h"

class Service {
 public:
  Service();
  ~Service();
  void init();

  struct SvcInfo {
    bool fenced_stopped {false};
  };

  std::map<std::string, SvcInfo> services_;

 private:
  DELETE_COPY_AND_MOVE_OPERATORS(Service);
};

#endif  // COMMAND_H_
