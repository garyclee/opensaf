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

#include "node_state_hdlr_pl.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <cinttypes>
#include <cstring>
#include <thread>
#include <utility>

NodeStateHdlrPl::NodeStateHdlrPl() {
  syslog(LOG_INFO, "NodeStateHdlrPl instantiated");
}

NodeStateHdlrPl::~NodeStateHdlrPl() {
  close(sd_);

  for (auto &kv : node_map_) {
    delete kv.second;
  }
}

int NodeStateHdlrPl::init() {
  sockaddr_tipc topsrv {};
  tipc_subscr subscr {};
  char *val {nullptr};
  uint32_t isolation_status_check_time_sec {DFLT_ISOLATION_STATUS_CHECK_TIME_SEC};
  uint32_t await_active_time_sec {DFLT_AWAIT_ACTIVE_TIME_SEC};

  if ((val = getenv("FMD_FENCED_SVC_TYPE")) != NULL) {
    fmd_fenced_svc_type_ = strtol(val, nullptr, 0);
  }

  if ((val = getenv("FMD_FENCED_SVC_TYPE_ACTIVE")) != NULL) {
    fmd_fenced_svc_type_active_ = strtol(val, nullptr, 0);
  }

  if ((val = getenv("ISOLATION_STATUS_CHECK_TIME_SEC")) != NULL) {
    isolation_status_check_time_sec = strtol(val, nullptr, 0);
  }

  if ((val = getenv("AWAIT_ACTIVE_TIME_SEC")) != NULL) {
    await_active_time_sec = strtol(val, nullptr, 0);
  }

  // seconds to microseconds
  isolation_status_check_time_us_ = isolation_status_check_time_sec * kMicrosPerSec;
  await_active_time_us_ = await_active_time_sec * kMicrosPerSec;

  // connect to topology server
  memset(&topsrv, 0, sizeof(topsrv));
  topsrv.family = AF_TIPC;
  topsrv.addrtype = TIPC_ADDR_NAME;
  topsrv.addr.name.name.type = TIPC_TOP_SRV;
  topsrv.addr.name.name.instance = TIPC_TOP_SRV;

  sd_ = socket(AF_TIPC, SOCK_SEQPACKET, 0);

  if (connect(sd_, reinterpret_cast<sockaddr *>(&topsrv), sizeof(topsrv)) < 0) {
    syslog(LOG_ERR, "failed to connect to topology server: %s", strerror(errno));
    return -1;
  }

  // subscribe to FMD
  subscr.seq.type  = htonl(fmd_fenced_svc_type_);
  subscr.seq.lower = htonl(0);
  subscr.seq.upper = htonl(~0);
  subscr.timeout   = htonl(TIPC_WAIT_FOREVER);
  subscr.filter    = htonl(TIPC_SUB_SERVICE);
  subscr.usr_handle[0] = static_cast<char>(2);

  if (send(sd_, &subscr, sizeof(subscr), 0) != sizeof(subscr)) {
    syslog(LOG_ERR, "failed to subscribe to service event: %s", strerror(errno));
    return -1;
  }

  // subscribe to Active FMD
  subscr.seq.type  = htonl(fmd_fenced_svc_type_active_);
  subscr.seq.lower = htonl(0);
  subscr.seq.upper = htonl(~0);
  subscr.timeout   = htonl(TIPC_WAIT_FOREVER);
  subscr.filter    = htonl(TIPC_SUB_SERVICE);
  subscr.usr_handle[0] = static_cast<char>(2);

  if (send(sd_, &subscr, sizeof(subscr), 0) != sizeof(subscr)) {
    syslog(LOG_ERR, "failed to subscribe to service event: %s", strerror(errno));
    return -1;
  }

  // subscribe to node events
  subscr.seq.type  = htonl(0);
  subscr.seq.lower = htonl(0);
  subscr.seq.upper = htonl(~0);
  subscr.timeout   = htonl(TIPC_WAIT_FOREVER);
  subscr.filter    = htonl(TIPC_SUB_PORTS);
  subscr.usr_handle[0] = static_cast<char>(3);

  if (send(sd_, &subscr, sizeof(subscr), 0) != sizeof(subscr)) {
    syslog(LOG_ERR, "failed to subscribe to node events: %s", strerror(errno));
    return -1;
  }

  // create receive thread
  std::thread(&NodeStateHdlrPl::receive, this).detach();
  return 0;
}

void NodeStateHdlrPl::check_isolation() {
  int no_of_active = 0;
  int no_of_sc = 0;

  for (auto &kv : node_map_) {
    if (kv.second->fmdIsActive) {
      no_of_active++;
    }
    if (kv.second->isController && kv.second->nodeIsUp) {
      no_of_sc++;
    }
  }

  if (no_of_sc == 0) {
    if (await_act_tmr_status_ == NodeStateHdlrPl::AwaitActiveTmrStatus::kRunning) {
      await_act_tmr_.stop();
    }
    await_act_tmr_status_ = NodeStateHdlrPl::AwaitActiveTmrStatus::kNotRunning;
    isolated_ = NodeIsolationState::kIsolated;
    goto notify;
  }

  if (no_of_active == 0) {
    if (await_act_tmr_status_ == NodeStateHdlrPl::AwaitActiveTmrStatus::kNotRunning) {
      await_act_tmr_status_ = NodeStateHdlrPl::AwaitActiveTmrStatus::kRunning;
      await_act_tmr_.start(await_active_time_us_, Timer::Type::kOneShot);
      syslog(LOG_NOTICE, "await active timer started");
      goto done;
    } else if (await_act_tmr_status_ == NodeStateHdlrPl::AwaitActiveTmrStatus::kRunning) {
      goto done;
    } else { /* timer expired */
      isolated_ = NodeIsolationState::kIsolated;
      await_act_tmr_status_ = NodeStateHdlrPl::AwaitActiveTmrStatus::kNotRunning;
      syslog(LOG_NOTICE, "no active controller detected");
    }
  } else {
    if (await_act_tmr_status_ == NodeStateHdlrPl::AwaitActiveTmrStatus::kRunning) {
      await_act_tmr_.stop();
    }
    await_act_tmr_status_ = NodeStateHdlrPl::AwaitActiveTmrStatus::kNotRunning;

    if (no_of_active == 1) {
      isolated_ = NodeIsolationState::kNotIsolated;
      syslog(LOG_NOTICE, "one active controller detected");
    } else {
      isolated_ = NodeIsolationState::kIsolated;
      syslog(LOG_NOTICE, "%d active controllers detected, split brain", no_of_active);
    }
  }
notify:
  send_update();
done:
  return;
}

void NodeStateHdlrPl::handle_event(const tipc_event &evt) {
  NodeState* node_state;

  auto map = node_map_.find(ntohl(evt.port.node));

  if (map == node_map_.end()) {
    node_state = new NodeState;
    node_map_.insert(std::make_pair(ntohl(evt.port.node), node_state));
  } else {
    node_state = map->second;
  }

  if (evt.event == htonl(TIPC_PUBLISHED)) {
    syslog(LOG_NOTICE, "TIPC Published: svc %d port: %d", ntohl(evt.s.seq.type), ntohl(evt.port.node));
    if (ntohl(evt.s.seq.type) == fmd_fenced_svc_type_) {
      node_state->isController = true;
    } else if (ntohl(evt.s.seq.type) == fmd_fenced_svc_type_active_) {
      node_state->fmdIsActive = true;
    } else if (ntohl(evt.s.seq.type) == 0) {
      node_state->nodeIsUp = true;
    }
  } else if (evt.event == htonl(TIPC_WITHDRAWN)) {
    syslog(LOG_NOTICE, "TIPC Withdraw: svc %d port: %d", ntohl(evt.s.seq.type), ntohl(evt.port.node));
    if (ntohl(evt.s.seq.type) == fmd_fenced_svc_type_active_) {
      node_state->fmdIsActive = false;
    } else if (ntohl(evt.s.seq.type) == 0) {
      node_state->nodeIsUp = false;
      node_state->isController = false;
    }
  }
}

//
void NodeStateHdlrPl::receive() {
  Timer tmr;
  tipc_event evt;

  enum {
    FD_TMR = 0,
    FD_TIPC,
    FD_AWAIT_ACTIVE_TMR,
    NUM_FDS
  };
  struct pollfd fds[NUM_FDS];
  int await_act_tmr_fd = await_act_tmr_.get_fd();
  int timer_fd = tmr.start(isolation_status_check_time_us_, Timer::Type::kOneShot);

  fds[FD_TIPC].fd = sd_;
  fds[FD_TIPC].events = POLLIN;

  fds[FD_TMR].fd = timer_fd;
  fds[FD_TMR].events = POLLIN;

  fds[FD_AWAIT_ACTIVE_TMR].fd = await_act_tmr_.get_fd();
  fds[FD_AWAIT_ACTIVE_TMR].events = POLLIN;

  while (true) {
    int rc = poll(fds, NUM_FDS, -1);
    if (rc == -1) {
      if (errno == EINTR) continue;
      syslog(LOG_ERR, "poll failed: %s", strerror(errno));
      break;
    }
    if (rc > 0) {
      if (fds[FD_TIPC].revents == POLLIN) {
        for (;;) {
          if (recv(sd_, &evt, sizeof(evt),
                   MSG_DONTWAIT) < 0) {
            if (errno == EINTR)
              continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            syslog(LOG_ERR, "recv failed: %s", strerror(errno));
            return;
          }
          handle_event(evt);
        }
      }
      if (fds[FD_TMR].revents == POLLIN) {
        uint64_t expirations = 0;
        if (read(timer_fd, &expirations, 8) != 8) {
          syslog(LOG_WARNING, "error reading timerfd value");
        } else {
          if (expirations != 1) {
            syslog(LOG_NOTICE, "timerfd expired %" PRIu64 " times", expirations);
          }
        }
        if (isolated_ == NodeIsolationState::kIsolated) {
          send_update();
        }
      }
      if (fds[FD_AWAIT_ACTIVE_TMR].revents == POLLIN) {
        uint64_t expirations = 0;
        if (read(await_act_tmr_fd, &expirations, 8) != 8) {
          syslog(LOG_WARNING, "error reading timerfd value");
        } else {
          if (expirations != 1) {
            syslog(LOG_NOTICE, "timerfd expired %" PRIu64 " times", expirations);
          }
        }
        // await active timer expired, check if any active is detected
        await_act_tmr_status_ = NodeStateHdlrPl::AwaitActiveTmrStatus::kExpired;
      }
    }
    check_isolation();
  }
}
