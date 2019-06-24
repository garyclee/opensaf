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

#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <syslog.h>
#include <sys/socket.h>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "command.h"
#include "service.h"
#include "timer.h"
#include "node_state_hdlr_factory.h"
#include "watchdog.h"
#include "node_state_file.h"

static NodeStateFile node_state_file;
static NodeStateHdlr *node_state_hdlr {nullptr};
static Timer tmr;
static Command cmd;
static Service svc;
static Watchdog wd;

static void sigterm_handler(int signum, siginfo_t *info, void *ptr) {
  (void) signum;
  (void) info;
  (void) ptr;

  exit(0);
}

static void set_rt_prio() {
  struct sched_param param {0};
  int policy {SCHED_RR};
  param.sched_priority = sched_get_priority_min(policy);

  if (sched_setscheduler(0, policy, &param) < 0) {
    syslog(LOG_ERR, "failed to set scheduling class: %s", strerror(errno));
  }
}

//
int main(int argc, char* argv[]) {
  (void) argc;

  NodeStateHdlr::NodeIsolationState isolated {NodeStateHdlr::NodeIsolationState::kUndefined};
  NodeStateHdlr::NodeIsolationState current_isolation_state {NodeStateHdlr::NodeIsolationState::kUndefined};
  struct sigaction act;

  enum {
    FD_TMRWD = 0,
    FD_EVT,
    NUM_FDS
  };
  struct pollfd fds[NUM_FDS];

  openlog(basename(argv[0]), LOG_PID, LOG_LOCAL0);

  sigemptyset(&act.sa_mask);
  act.sa_sigaction = sigterm_handler;
  act.sa_flags = SA_SIGINFO | SA_RESETHAND;
  if (sigaction(SIGTERM, &act, NULL) < 0) {
    syslog(LOG_ERR, "sigaction TERM failed: %s", strerror(errno));
  }

  set_rt_prio();

  svc.init();

  node_state_hdlr = NodeStateHdlrFactory::instance().create();

  if (node_state_hdlr->init() < 0) {
    exit(-1);
  }

  node_state_file.init();

  // use systemd watchdog time value divided by two to kick the watchdog
  int timer_fd = tmr.start(wd.interval_usec() / 2, Timer::Type::kPeriodic);
  syslog(LOG_NOTICE, "systemd watchdog interval us %ld", wd.interval_usec() / 2);

  fds[FD_EVT].fd = node_state_hdlr->get_event_fd();
  fds[FD_EVT].events = POLLIN;

  fds[FD_TMRWD].fd = timer_fd;
  fds[FD_TMRWD].events = POLLIN;

  while (true) {
    int rc = poll(fds, NUM_FDS, -1);
    if (rc == -1) {
      if (errno == EINTR) continue;
      syslog(LOG_ERR, "poll failed: %s", strerror(errno));
      break;
    }
    if (rc > 0) {
      if (fds[FD_EVT].revents == POLLIN) {
        for (;;) {
          if (recv(fds[FD_EVT].fd, &isolated, sizeof(isolated),
                   MSG_DONTWAIT) < 0) {
            if (errno == EINTR)
              continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            syslog(LOG_ERR, "recv failed: %s", strerror(errno));
            goto done;
          }
        }
        if (isolated != current_isolation_state) {
          current_isolation_state = isolated;
          node_state_file.update(isolated);
          if (isolated == NodeStateHdlr::NodeIsolationState::kIsolated) {
            for (auto &s : svc.services_) {
              if (cmd.is_active(s.first)) {
                syslog(LOG_NOTICE, "Node is isolated - %s will be stopped", s.first.c_str());
                s.second.fenced_stopped = true;
                cmd.stop(s.first);
              }
            }
          } else {
            for (auto &s : svc.services_) {
              if (s.second.fenced_stopped && !cmd.is_active(s.first)) {
                syslog(LOG_NOTICE, "Node is not isolated - %s will be started", s.first.c_str());
                s.second.fenced_stopped = false;
                cmd.start(s.first);
              }
            }
          }
        }
      }
      // timerfd
      if (fds[FD_TMRWD].revents == POLLIN) {
        uint64_t expirations = 0;
        if (read(tmr.get_fd(), &expirations, 8) != 8) {
          syslog(LOG_WARNING, "error reading timerfd value");
        } else {
          if (expirations != 1) {
            syslog(LOG_NOTICE, "timerfd expired %" PRIu64 " times", expirations);
          }
        }

        if (current_isolation_state == NodeStateHdlr::NodeIsolationState::kIsolated) {
          for (auto &s : svc.services_) {
            if (cmd.is_active(s.first)) {
              syslog(LOG_NOTICE, "%s is active, node is isolated - %s will be stopped",
                     s.first.c_str(), s.first.c_str());
              s.second.fenced_stopped = true;
              cmd.stop(s.first);
            }
          }
        }
        // kick systemd watchdog
        wd.kick();
      }
    }
  }
done:
  closelog();
  return -1;
}
