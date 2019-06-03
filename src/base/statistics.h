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

#ifndef STATISTICS_H_
#define STATISTICS_H_

#include <cmath>

namespace base {

class Statistics {
 public:
  void clear() {
    n_ = 0;
  }

  void push(double x) {
    n_++;

    // See Knuth, Art Of Computer Programming, Volume 2. The Seminumerical Algorithms, 4.2.2. Accuracy of Floating Point Arithmetic,
    // using the recurrence formulas:
    // M1 = x1, Mk = Mk-1 + (xk - Mk-1) / k  (15)
    // S1 = 0, Sk = Sk-1 + (xk - Mk-1) * (xk - Mk)  (16)
    // for 2 <= k <= n, sqrt(Sn/(n-1))
    if (n_ == 1) {
      prev_m_ = current_m_ = x;
      prev_s_ = 0;
      min_ = x;
      max_ = x;
    } else {
      current_m_ = prev_m_ + (x - prev_m_) / n_;
      current_s_ =  prev_s_ + (x - prev_m_) * (x - current_m_);

      if (x > max_) max_ = x;
      if (x < min_) min_ = x;
      prev_m_ = current_m_;
      prev_s_ = current_s_;
    }
  }

  double mean() const {
    return (n_ > 0) ?  current_m_ : 0;
  }

  double variance() const {
    return (n_ > 1) ? current_s_ / (n_ - 1) : 0;
  }

  double std_dev() const {
    return sqrt(variance());
  }

  double min() const {
    return min_;
  }
  double max() const {
    return max_;
  }

 private:
  int n_{0};
  double prev_m_{0};
  double current_m_{0};
  double prev_s_{0};
  double current_s_{0};
  double min_{0};
  double max_{0};
};

}  // namespace base

#endif  // STATISTICS_H_

