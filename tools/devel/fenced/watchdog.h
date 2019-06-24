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

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include <cstdint>

#include "cpp_macros.h"
#include "timer.h"

class Watchdog {
 public:
  const uint64_t DFLT_WATCHDOG_TIMEOUT = 10 * kMicrosPerSec;
  Watchdog();
  ~Watchdog() {}
  void kick();
  uint64_t interval_usec() const {return interval_usec_;}
 protected:
 private:
  uint64_t interval_usec_;
  DELETE_COPY_AND_MOVE_OPERATORS(Watchdog);
};

#endif  // WATCHDOG_H_
