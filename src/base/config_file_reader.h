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

#ifndef BASE_CONFIG_FILE_READER_H_
#define BASE_CONFIG_FILE_READER_H_

#include <map>
#include <string>

class ConfigFileReader {
 public:
  using SettingsMap = std::map<std::string, std::string>;

  // parses OpenSAF 'config' files and return key-value pairs in a map
  SettingsMap ParseFile(const std::string& filename);
};

#endif /* BASE_CONFIG_FILE_READER_H_ */
