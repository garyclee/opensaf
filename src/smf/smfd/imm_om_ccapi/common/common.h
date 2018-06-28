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

#ifndef SMF_SMFD_IMM_OM_CCAPI_COMMON_COMMON_H_
#define SMF_SMFD_IMM_OM_CCAPI_COMMON_COMMON_H_

#include <string>
#include "ais/include/saImm.h"
#include "ais/include/saAis.h"
#include "base/time.h"
#include "base/saf_error.h"

// When getting SA_AIS_ERR_TRY_AGAIN, the retry loop will be run.
// and in default, the interval between retries is kDefaultIntervalMs(40ms)
// and the timeout of retry loop is kDefaultTimeoutMs(10 seconds).
// When timeout occurs, the retry loop will be exited.
// If user does not provide its interested default retry control value,
// these values will be used when constructing RetryControl object.
const timespec kDefaultIntervalMs      = base::kFourtyMilliseconds;
const uint64_t kDefaultTimeoutMs       = 10*1000;

// Default IMM version
const SaUint8T kDefaultImmReleaseCode  = 'A';
const SaUint8T kDefaultImmMajorVersion = 2;
const SaUint8T kDefaultImmMinorVersion = 11;

// As SaTimeT and SaInt64T are aliases to same data type,
// we can not distinguish if passing them to a template method.
// We do workaround by introducing a new data type for SaNameT.
//
// Usage example:
// CppSaTimeT time = 1234456;
//
// printf("%d", time());
// SetAttributeValue("timeattribute", &time);
//
struct CppSaTimeT {
  CppSaTimeT() { time = 0; }
  explicit CppSaTimeT(SaTimeT t) : time{t} {};
  // explicit CppSaTimeT(int t) : time{t} {};

  CppSaTimeT& operator=(SaTimeT t) {
    time = t;
    return *this;
  }

  SaTimeT& operator()() {
    return time;
  }

  bool operator==(SaTimeT t) {
    return (time == t);
  }

  SaTimeT time;
};

//>
// Base class of all IMM OI and OM abstractions
//
// The class holds retry loop information, returned AIS error code. Besides, the
// class provides static methods to change default IMM version, default retry
// loop control value and get returned AIS code.
//
// Example:
// ImmOmHandle h{version};
// ImmBase::RetryControl ctrl{base::kFiftyMilliseconds, 50*1000};
//
// h.SetRetryInfo(ctrl);  // Change default retry control
// h.InitializeHandle();  // Acquire IMM OM handle
//
// TRACE("AIS error code: ", h.ais_error());  // Get AIS error code
//<
class ImmBase {
 public:
  // Hold information for retry loop when C IMM API gets SA_AIS_ERR_TRY_AGAIN.
  struct RetryControl {
    // Sleep time b/w retry
    timespec interval;
    // Maximum time for retries (ms)
    uint64_t timeout;
    // Construct with default values.
    RetryControl();
    // Note: not using explicit here is intentional with the purpose of
    // initializing `default_retry_control` object.
    RetryControl(timespec i, uint64_t t) { interval = i; timeout = t; }
  };

  // Change default control setting to given setting.
  void SetRetryInfo(const RetryControl& info) {
    retry_ctrl_.interval = info.interval;
    retry_ctrl_.timeout  = info.timeout;
  }

  // Get AIS error code/error string which has been saved before.
  SaAisErrorT ais_error() const { return ais_error_; }
  const char* ais_error_string() const { return saf_error(ais_error_); }

  // Return the corressponding SaImmValueTypeT according to typename T.
  // For example, if T = SaUint64T, will get SA_IMM_ATTR_SAINT64T
  // or T = SaConstStringT, will get SA_IMM_ATTR_SASTRINGT
  template<typename T> static SaImmValueTypeT GetAttributeValueType();

  // Change default values.
  // NOTE: Should be called before using any abstraction methods.
  // and these methods are not thread-safe. User has to take care
  // of race condition when changing default values more than one places
  // in more than one thread.
  static void ChangeDefaultImmVersion(const SaVersionT& version);
  static void ChangeDefaultRetryControl(const RetryControl& ctrl);

  static const SaVersionT GetDefaultImmVersion();

 protected:
  ImmBase();
  explicit ImmBase(const RetryControl& ctrl);
  virtual ~ImmBase() {}

 protected:
  SaAisErrorT ais_error_;
  RetryControl retry_ctrl_;

  DELETE_COPY_AND_MOVE_OPERATORS(ImmBase);
};

template<typename T>
SaImmValueTypeT ImmBase::GetAttributeValueType() {
  // Depend on @T, will have different @attrValueType
  if (std::is_same<T, SaInt32T>::value) {
    return SA_IMM_ATTR_SAINT32T;
  } else if (std::is_same<T, SaUint32T>::value) {
    return SA_IMM_ATTR_SAUINT32T;
  } else if (std::is_same<T, SaInt64T>::value) {
    return SA_IMM_ATTR_SAINT64T;
  } else if (std::is_same<T, CppSaTimeT>::value) {
    return SA_IMM_ATTR_SATIMET;
  } else if (std::is_same<T, SaUint64T>::value) {
    return SA_IMM_ATTR_SAUINT64T;
  } else if (std::is_same<T, SaNameT>::value) {
    return SA_IMM_ATTR_SANAMET;
  } else if (std::is_same<T, SaFloatT>::value) {
    return SA_IMM_ATTR_SAFLOATT;
  } else if (std::is_same<T, SaDoubleT>::value) {
    return SA_IMM_ATTR_SADOUBLET;
  } else if ((std::is_same<T, SaStringT>::value)      ||
             (std::is_same<T, SaConstStringT>::value) ||
             (std::is_same<T, std::string>::value)) {
    return SA_IMM_ATTR_SASTRINGT;
  } else {
    return SA_IMM_ATTR_SAANYT;
  }
}

// Help macro to ensure data type passed to template methods
// MUST be SA AIS data type, or std::string,
// otherwise will get compile error.
#define SA_AIS_TYPE_CHECK(T)                    \
  static_assert(                                \
      std::is_same<T, SaInt32T>::value       || \
      std::is_same<T, SaInt64T>::value       || \
      std::is_same<T, CppSaTimeT>::value     || \
      std::is_same<T, SaUint32T>::value      || \
      std::is_same<T, SaUint32T>::value      || \
      std::is_same<T, SaUint64T>::value      || \
      std::is_same<T, SaFloatT>::value       || \
      std::is_same<T, SaDoubleT>::value      || \
      std::is_same<T, SaNameT>::value        || \
      std::is_same<T, SaStringT>::value      || \
      std::is_same<T, SaConstStringT>::value || \
      std::is_same<T, std::string>::value    || \
      std::is_same<T, SaTimeT>::value        || \
      std::is_same<T, SaAnyT>::value,           \
      "typename T is not supported");

#endif  // SMF_SMFD_IMM_OM_CCAPI_COMMON_COMMON_H_
