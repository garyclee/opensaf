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

#ifndef COMMAND_H_
#define COMMAND_H_

#include <systemd/sd-bus.h>
#include <cstdint>
#include <string>

#include "cpp_macros.h"

class Command {
 public:
  Command();
  ~Command();
  int32_t start(const std::string& service) {return sd_start_stop_cmd("StartUnit", service);}
  int32_t stop(const std::string& service) {return sd_start_stop_cmd("StopUnit", service);}
  bool is_active(const std::string& service);

 protected:
 private:
  int32_t sd_start_stop_cmd(const std::string& method_name,
                            const std::string& service);
  sd_bus *bus_{NULL};
  DELETE_COPY_AND_MOVE_OPERATORS(Command);
};

#endif  // COMMAND_H_
