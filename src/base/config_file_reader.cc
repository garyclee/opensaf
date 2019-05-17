/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2018 - All Rights Reserved.
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
 */

#include <algorithm>
#include <fstream>
#include <string>
#include "base/config_file_reader.h"

// @todo move into a string library class
static void left_trim(std::string& str) {
  str.erase(str.begin(), std::find_if(str.begin(), str.end(),
                                      [](int c) { return !std::isspace(c); }));
}

static void right_trim(std::string& str) {
  str.erase(std::find_if(str.rbegin(), str.rend(),
                         [](int c) { return !std::isspace(c); })
                .base(),
            str.end());
}

static void trim(std::string& str) {
  left_trim(str);
  right_trim(str);
}

static void strip_quotes(std::string& str) {
  // trim leading and trailing quotes
  if (str.front() == '"' ||
      str.front() == '\'') {
    str.erase(0, 1);  // delete first char
  }
  if (str.back() == '"' ||
    str.back() == '\'') {
    str.pop_back();  // delete last char
  }
}

ConfigFileReader::SettingsMap ConfigFileReader::ParseFile(
    const std::string& filename) {
  const std::string prefix("export");
  SettingsMap map;
  std::ifstream file(filename);
  std::string line;

  if (file.is_open()) {
    while (getline(file, line)) {
      // go through each line of the config file
      // only process "export key=value" and ignore trailing comments
      // note: value may be surrounded with quotes

      // trim leading / trailing spaces
      trim(line);

      // must begin with 'export'
      if (line.find(prefix) != 0) {
        continue;
      }

      // remove 'export'
      line.erase(0, prefix.length());

      // remove any trailing comments
      size_t hash = line.find('#');
      if (hash != std::string::npos) {
        line.erase(hash, std::string::npos);
      }

      trim(line);

      size_t equal = line.find('=');
      if (equal == std::string::npos ||
          equal == 0) {
        // must look like "key=value"
        continue;
      }

      std::string key = line.substr(0, equal);
      trim(key);
      std::string value = line.substr(equal + 1);
      trim(value);

      strip_quotes(key);
      strip_quotes(value);

      map[key] = value;
    }
    file.close();
  }
  return map;
}
