/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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
 * Reference: Serial Number Arithmetic from RFC1982
 *
 */

#ifndef BASE_SNA_H_
#define BASE_SNA_H_

#include <cassert>
#include <typeinfo>
#include <stdexcept>

#define SNA16_MAX   (1UL << 16)
#define SNA16_SPACE (SNA16_MAX/2)
#define SNA32_MAX   (1ULL << 32)
#define SNA32_SPACE (SNA32_MAX/2)

template <class T>
class SerialNumber {
 public:
  SerialNumber(): value_(0) {}
  SerialNumber(const SerialNumber &t) {
    value_ = t.value_;
  }
  explicit SerialNumber(const uint64_t &n) {
    if ((n < 0) || (n > (max()-1)))
      assert(0 && "Invalid initial value");
    value_ = n;
  }
  SerialNumber& operator=(const SerialNumber &t) {
    // check for self-assignment
    if (&t == this)
      return *this;
    value_ = t.value_;
    return *this;
  }
  T v() const {
    return value_;
  }
  SerialNumber& operator+=(const uint64_t& n) {
    if ((n < 0) || (n > (space() - 1)))
      throw std::out_of_range("Invalid addition value");
    value_ = (value_ + n) % max();
    return *this;
  }
  friend SerialNumber operator+(SerialNumber m, const uint64_t& n) {
    m += n;
    return m;
  }
  // prefix ++
  SerialNumber& operator++() {
    *this += 1;
    return *this;
  }
  // postfix ++
  SerialNumber operator++(int) {
    SerialNumber tmp(*this);
    operator++();
    return tmp;
  }
  bool operator==(const SerialNumber& rhs) {
    return value_ == rhs.value_;
  }
  bool operator==(const uint32_t val) {
    return value_ == val;
  }
  bool operator!=(const SerialNumber& rhs) {
    return value_ != rhs.value_;
  }
  bool operator<(const SerialNumber& rhs) {
    return (value_ < rhs.value_ && rhs.value_ - value_ < space()) || \
          (value_ > rhs.value_ && value_ - rhs.value_ > space());
  }
  bool operator<=(const SerialNumber& rhs) {
    return *this == rhs || *this < rhs;
  }
  bool operator>(const SerialNumber& rhs) {
    return (value_ < rhs.value_ && rhs.value_ - value_ > space()) || \
          (value_ > rhs.value_ && value_ - rhs.value_ < space());
  }
  bool operator>=(const SerialNumber& rhs) {
    return *this == rhs || *this > rhs;
  }
  int64_t operator-(const SerialNumber& rhs) {
    if (*this >= rhs) {
      if (value_ >= rhs.value_) {
        return value_ - rhs.value_;
      } else {
        return (value_ + max()) - rhs.value_;
      }
    } else {
      if (value_ < rhs.value_) {
        return value_ - rhs.value_;
      } else {
        return value_ - (rhs.value_ + max());
      }
    }
  }

 private:
  T value_;
  uint64_t max() {
    if (typeid(T) == typeid(uint64_t)) {
      return SNA32_MAX;
    }
    if (typeid(T) == typeid(uint32_t)) {
      return SNA16_MAX;
    }
    assert(0 && "Invalid data type");
    return 0;
  }
  uint64_t space() {
    if (typeid(T) == typeid(uint64_t)) {
      return SNA32_SPACE;
    }
    if (typeid(T) == typeid(uint32_t)) {
      return SNA16_SPACE;
    }
    assert(0 && "Invalid data type");
    return 0;
  }
};

using Seq16 = SerialNumber<uint32_t>;
using Seq32 = SerialNumber<uint64_t>;

#endif  // BASE_SNA_H_
