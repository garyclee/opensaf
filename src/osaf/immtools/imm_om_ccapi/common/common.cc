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
 * Author(s): Ericsson AB
 *
 */

#include "common.h"
#include "base/mutex.h"

// Variables hold current default retry loop control and default IMM version.
// If user does not provide its own interested values, the abstraction
// uses its own default ones.
static ImmBase::RetryControl default_retry_control = {
  kDefaultIntervalMs,
  kDefaultTimeoutMs
};
static SaVersionT default_imm_version = {
  kDefaultImmReleaseCode,
  kDefaultImmMajorVersion,
  kDefaultImmMinorVersion
};

ImmBase::RetryControl::RetryControl() {
  timeout  = default_retry_control.timeout;
  interval = default_retry_control.interval;
}

ImmBase::ImmBase() : ais_error_{SA_AIS_OK} {}

ImmBase::ImmBase(const RetryControl& ctrl) : ais_error_{SA_AIS_OK} {
  SetRetryInfo(ctrl);
}

void ImmBase::ChangeDefaultImmVersion(const SaVersionT& version) {
  default_imm_version = version;
}

void ImmBase::ChangeDefaultRetryControl(const RetryControl& ctrl) {
  default_retry_control.interval = ctrl.interval;
  default_retry_control.timeout  = ctrl.timeout;
}

const SaVersionT ImmBase::GetDefaultImmVersion() {
  return default_imm_version;
}
