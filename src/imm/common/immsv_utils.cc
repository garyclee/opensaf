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

#include "imm/common/immsv_utils.h"

std::string ReverseDn(const std::string& input) {
  std::string result = "";
  size_t start_cut = 0;
  size_t comma_pos = 0;

  do {
    size_t start_search = start_cut;
    while ((comma_pos = input.find(",", start_search)) ==
           input.find("\\,", start_search) + 1)
      // Skip the "\," by shifting start position
      start_search = input.find(",", start_search) + 1;

    // Insert RDN to the begin of the result */
    if (!result.empty()) result.insert(0, ",");
    result.insert(0, input, start_cut, comma_pos - start_cut);

    // Next RDN
    start_cut = comma_pos + 1;
  } while (comma_pos != std::string::npos);

  return result;
}

// true if @name is one of system attributes.
bool is_system_attribute(const std::string& name) {
  const char* system_attributes[] = {
    "SaImmAttrClassName",
    "SaImmAttrAdminOwnerName",
    "SaImmAttrImplementerName"
  };

  for (const auto& s : system_attributes) {
    if (std::string {s} == name) return true;
  }
  return false;
}
