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

#include "mds_tipc_recvq_stats_impl.h"
#include <errno.h>
#include <linux/tipc.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <thread>
#include "base/osaf_time.h"
#include "base/statistics.h"
#include "base/logtrace.h"

void TipcRecvqStatsImpl::start() {
  std::thread(&TipcRecvqStatsImpl::tipc_recvq_stats_bg, this).detach();
}

int TipcRecvqStatsImpl::init(int sd) {
  int optval{0};
  socklen_t optlen = sizeof(optval);
  long log_freq_sec{0};

  sd_ = sd;
  recvq_size_ = 0;
  char *val;

  if ((val = getenv("MDS_RECVQ_STATS_LOG_FREQ_SEC")) != NULL) {
    log_freq_sec = strtol(val, NULL, 0);
    log_freq_ = log_freq_sec * 10;
    if (log_freq_sec < 1) {
      LOG_NO("MDS_RECVQ_STATS_LOG_FREQ_SEC value is set too low: %ld seconds", log_freq_sec);
      return -1;
    }
  } else {
    return -1;
  }

  if (getsockopt(sd_, SOL_SOCKET, SO_RCVBUF, &optval, &optlen) < 0) {
    LOG_NO("TIPC getsockopt failed: %s", strerror(errno));
    return -1;
  }

  recvq_size_ = optval;

  return get_tipc_recvq_used(&optval);
}

int TipcRecvqStatsImpl::get_tipc_recvq_used(int *optval) {
#ifndef TIPC_SOCK_RECVQ_USED
#define TIPC_SOCK_RECVQ_USED 137
#endif

  socklen_t optlen = sizeof(optval);

  if (getsockopt(sd_, SOL_TIPC, TIPC_SOCK_RECVQ_USED, optval, &optlen) == 0) {
    return 0;
  } else {
    LOG_NO("TIPC getsockopt failed: %s", strerror(errno));
    return -1;
  }
}

int TipcRecvqStatsImpl::create_timer() {
  if ((timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)) < 0) {
    LOG_NO("timerfd_create failed: %s", strerror(errno));
    return -1;
  }
  return 0;
}

int TipcRecvqStatsImpl::start_timer() {
  uint64_t interval_msec{100};
  struct itimerspec spec;
  spec.it_interval.tv_sec = interval_msec / kMillisPerSec;
  spec.it_interval.tv_nsec =  (interval_msec % kMillisPerSec) * (kNanosPerSec / kMillisPerSec);
  spec.it_value.tv_sec = spec.it_interval.tv_sec;
  spec.it_value.tv_nsec = spec.it_interval.tv_nsec;

  if (timerfd_settime(timer_fd_, 0, &spec, NULL) < 0) {
    LOG_NO("timerfd_settime failed: %s", strerror(errno));
    return -1;
  }
  return 0;
}

int TipcRecvqStatsImpl::stop_timer() {
  struct itimerspec spec;

  spec.it_interval.tv_sec = 0;
  spec.it_interval.tv_nsec = 0;
  spec.it_value.tv_sec = 0;
  spec.it_value.tv_nsec = 0;

  if (timerfd_settime(timer_fd_, 0, &spec, NULL) < 0) {
    LOG_NO("timerfd_settime failed: %s", strerror(errno));
    return -1;
  }
  return 0;
}

void TipcRecvqStatsImpl::tipc_recvq_stats_bg() {
  base::Statistics stats;
  int optval;
  int ticks{0};

  enum {
    FD_TMR = 0,
    NUM_FDS
  };

  create_timer();

  struct pollfd fds[NUM_FDS];
  fds[FD_TMR].fd = timer_fd_;
  fds[FD_TMR].events = POLLIN;

  start_timer();

  LOG_NO("TIPC receive queue statistics started");

  while (true) {
    int rc = poll(fds, NUM_FDS, -1);
    if (rc == -1) {
      if (errno == EINTR) continue;
      LOG_IN("poll failed: %s", strerror(errno));
      break;
    }
    if (rc > 0) {
      if (fds[FD_TMR].revents == POLLIN) {
        uint64_t expirations = 0;
        if (read(timer_fd_, &expirations, 8) != 8) {
          LOG_NO("error reading timerfd value: %s", strerror(errno));
          return;
        } else {
          if (expirations != 1) {
            LOG_NO("timerfd expired %" PRIu64 " times", expirations);
          }
        }
        if (get_tipc_recvq_used(&optval) == 0) {
          ++ticks;
          stats.push((optval / recvq_size_) * 100.0);
          if (ticks >= log_freq_) {
            LOG_NO("TIPC receive queue utilization (in %%): min: %2.2f max: %2.2f mean: %2.2f std dev: %2.2f",
                   stats.min(), stats.max(), stats.mean(), stats.std_dev());
            ticks = 0;
            stats.clear();
          }
        } else {
          break;
        }
      }
    }
  }
  return;
}

