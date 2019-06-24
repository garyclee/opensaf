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

/*
  for more info, see:
   man sd_bus_message_append

  some busctl commands:
   busctl introspect org.freedesktop.systemd1 /org/freedesktop/systemd1 org.freedesktop.systemd1.Manager
   busctl get-property org.freedesktop.systemd1 /org/freedesktop/systemd1/unit/opensafd_2eservice org.freedesktop.systemd1.Unit ActiveState
   busctl call org.freedesktop.systemd1 /org/freedesktop/systemd1 org.freedesktop.systemd1.Manager StartUnit "ss" "opensafd.service" "replace"
*/

#include "command.h"

#include <syslog.h>
#include <cstdio>
#include <sstream>
#include <string>

namespace {
const char SERVICE_NAME[] = "org.freedesktop.systemd1";
const char OBJECT_PATH[] = "/org/freedesktop/systemd1";
const char INTERFACE[] = "org.freedesktop.systemd1.Manager";
const char JOB_MODE[] = "replace";
}

Command::Command() {
}

Command::~Command() {
  sd_bus_unref(bus_);
}

int32_t Command::sd_start_stop_cmd(const std::string& method_name,
                                   const std::string& service) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *msg {NULL};
  const char *path {NULL};

  std::string no_esc_service = service + ".service";
  // connect to the system bus
  int rc = sd_bus_default_system(&bus_);
  if (rc < 0) {
    syslog(LOG_WARNING, "sd_bus_default_system failed: %s", strerror(-rc));
    goto leave;
  }

  // Issue the method call and store the response message
  rc = sd_bus_call_method(bus_,
                          SERVICE_NAME,
                          OBJECT_PATH,
                          INTERFACE,
                          method_name.c_str(),
                          &error,
                          &msg,
                          "ss",
                          no_esc_service.c_str(),
                          JOB_MODE);
  if (rc < 0) {
    syslog(LOG_WARNING, "sd_bus_call_method failed: %s", strerror(-rc));
    goto done;
  }

  // Parse the response message
  rc = sd_bus_message_read(msg, "o", &path);
  if (rc < 0) {
    syslog(LOG_WARNING, "sd_bus_message_read failed: %s", strerror(-rc));
    goto done;
  }
  syslog(LOG_INFO, "fenced::  %s", path);
  sd_bus_error_free(&error);
done:
  sd_bus_message_unref(msg);
  sd_bus_unref(bus_);
leave:
  return rc;
}

bool Command::is_active(const std::string& service) {
  sd_bus_error error {SD_BUS_ERROR_NULL};
  char *buf {NULL};
  bool is_active {false};

  // escaped service name
  std::string esc_service = service + "_2eservice";
  std::string object_path = OBJECT_PATH;
  object_path += "/unit/" + esc_service;

  // Connect to the system bus
  int rc = sd_bus_default_system(&bus_);
  if (rc < 0) {
    syslog(LOG_WARNING, "sd_bus_default_system failed: %s", strerror(-rc));
    goto leave;
  }

  // Get service active property
  rc = sd_bus_get_property_string(bus_,
                                  SERVICE_NAME,
                                  object_path.c_str(),
                                  "org.freedesktop.systemd1.Unit",
                                  "ActiveState",
                                  &error,
                                  &buf);
  if (rc < 0) {
    syslog(LOG_WARNING, "sd_bus_get_property failed: %s", strerror(-rc));
    goto done;
  }

  if (strncmp(buf, "active", 6) == 0) {
    is_active = true;
  }

  free(buf);
  sd_bus_error_free(&error);
 done:
  sd_bus_unref(bus_);
 leave:
  return is_active;
}
