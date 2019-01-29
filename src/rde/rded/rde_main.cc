/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
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
 *            Wind River Systems
 *
 */

#include <limits.h>
#include <saAmf.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include "base/conf.h"
#include "base/daemon.h"
#include "base/logtrace.h"
#include "base/ncs_main_papi.h"
#include "base/osaf_poll.h"
#include "mds/mds_papi.h"
#include "nid/agent/nid_api.h"
#include "osaf/consensus/consensus.h"
#include "rde/rded/rde_cb.h"
#include "rde/rded/role.h"
#include "rde_cb.h"

#define RDA_MAX_CLIENTS 32

enum { FD_TERM = 0, FD_AMF = 1, FD_MBX, FD_RDA_SERVER, FD_CLIENT_START };

static void SendPeerInfoResp(MDS_DEST mds_dest);
static void CheckForSplitBrain(const rde_msg *msg);

const char *rde_msg_name[] = {"-",
                              "RDE_MSG_PEER_UP(1)",
                              "RDE_MSG_PEER_DOWN(2)",
                              "RDE_MSG_PEER_INFO_REQ(3)",
                              "RDE_MSG_PEER_INFO_RESP(4)",
                              "RDE_MSG_NEW_ACTIVE_CALLBACK(5)"
                              "RDE_MSG_NODE_UP(6)",
                              "RDE_MSG_NODE_DOWN(7)",
                              "RDE_MSG_TAKEOVER_REQUEST_CALLBACK(8)",
                              "RDE_MSG_ACTIVE_PROMOTION_SUCCESS(9)",
                              "RDE_MSG_CONTROLLER_UP(10)",
                              "RDE_MSG_CONTROLLER_DOWN(11)"};

static RDE_CONTROL_BLOCK _rde_cb;
static RDE_CONTROL_BLOCK *rde_cb = &_rde_cb;
static NCS_SEL_OBJ usr1_sel_obj;
static NODE_ID own_node_id;
static Role *role;

RDE_CONTROL_BLOCK *rde_get_control_block() { return rde_cb; }

/**
 * USR1 signal is used when AMF wants instantiate us as a
 * component. Wake up the main thread so it can register with
 * AMF.
 *
 * @param i_sig_num
 */
static void sigusr1_handler(int sig) {
  (void)sig;
  signal(SIGUSR1, SIG_IGN);
  ncs_sel_obj_ind(&usr1_sel_obj);
}

static int fd_to_client_ixd(int fd) {
  int i;
  RDE_RDA_CB *rde_rda_cb = &rde_cb->rde_rda_cb;

  for (i = 0; i < rde_rda_cb->client_count; i++)
    if (fd == rde_rda_cb->clients[i].fd) break;

  assert(i < MAX_RDA_CLIENTS);

  return i;
}

static void handle_mbx_event() {
  rde_msg *msg;

  TRACE_ENTER();

  msg = reinterpret_cast<rde_msg *>(ncs_ipc_non_blk_recv(&rde_cb->mbx));

  switch (msg->type) {
    case RDE_MSG_PEER_INFO_REQ:
    case RDE_MSG_PEER_INFO_RESP: {
      LOG_NO("Got peer info %s from node 0x%x with role %s",
             msg->type == RDE_MSG_PEER_INFO_RESP ? "response" : "request",
             msg->fr_node_id, Role::to_string(msg->info.peer_info.ha_role));
      CheckForSplitBrain(msg);
      role->SetPeerState(msg->info.peer_info.ha_role, msg->fr_node_id);
      break;
    }
    case RDE_MSG_PEER_UP: {
      if (msg->fr_node_id != own_node_id) {
        LOG_NO("Peer up on node 0x%x", msg->fr_node_id);
        SendPeerInfoResp(msg->fr_dest);
        role->AddPeer(msg->fr_node_id);
      }
      break;
    }
    case RDE_MSG_PEER_DOWN:
      LOG_NO("Peer down on node 0x%x", msg->fr_node_id);
      break;
    case RDE_MSG_NEW_ACTIVE_CALLBACK: {
      const std::string my_node = base::Conf::NodeName();
      rde_cb->monitor_lock_thread_running = false;

      // get current active controller
      Consensus consensus_service;
      std::string active_controller = consensus_service.CurrentActive();

      LOG_NO("New active controller notification from consensus service");

      if (role->role() == PCS_RDA_ACTIVE) {
        if (my_node.compare(active_controller) != 0 &&
            active_controller.empty() == false) {
          // we are meant to be active, but consensus service doesn't think so
          LOG_WA("Role does not match consensus service. New controller: %s",
                 active_controller.c_str());
          if (consensus_service.IsRemoteFencingEnabled() == false) {
            LOG_ER("Probable split-brain. Rebooting this node");
            opensaf_quick_reboot("Split-brain detected by consensus service");
          }
        }

        // register for callback
        rde_cb->monitor_lock_thread_running = true;
        consensus_service.MonitorLock(Role::MonitorCallback, rde_cb->mbx);
      }
      break;
    }
    case RDE_MSG_NODE_UP:
      rde_cb->cluster_members.insert(msg->fr_node_id);
      TRACE("cluster_size %zu", rde_cb->cluster_members.size());
      break;
    case RDE_MSG_NODE_DOWN:
      rde_cb->cluster_members.erase(msg->fr_node_id);
      TRACE("cluster_size %zu", rde_cb->cluster_members.size());
      break;
    case RDE_MSG_CONTROLLER_UP:
      if (msg->fr_node_id != own_node_id) {
        rde_cb->peer_controllers.insert(msg->fr_node_id);
        TRACE("peer_controllers: size %zu", rde_cb->peer_controllers.size());
        if (rde_cb->state == State::kNotActive) {
          TRACE("Set state to kNotActiveSeenPeer");
          rde_cb->state = State::kNotActiveSeenPeer;
        } else if (rde_cb->state == State::kActiveElected) {
          TRACE("Set state to kActiveElectedSeenPeer");
          rde_cb->state = State::kActiveElectedSeenPeer;
        }
      }
      break;
    case RDE_MSG_CONTROLLER_DOWN:
      rde_cb->peer_controllers.erase(msg->fr_node_id);
      TRACE("peer_controllers: size %zu", rde_cb->peer_controllers.size());
      break;
    case RDE_MSG_TAKEOVER_REQUEST_CALLBACK: {
      rde_cb->monitor_takeover_req_thread_running = false;

      if (role->role() == PCS_RDA_ACTIVE) {
        LOG_NO("Received takeover request '%s'. Our network size is %zu",
                msg->info.takeover_request,
               rde_cb->cluster_members.size());

        Consensus consensus_service;
        Consensus::TakeoverState state =
            consensus_service.HandleTakeoverRequest(
                rde_cb->cluster_members.size(),
                msg->info.takeover_request);
        delete[] msg->info.takeover_request;

        if (state == Consensus::TakeoverState::ACCEPTED) {
          LOG_NO("Accepted takeover request");
          if (consensus_service.IsRemoteFencingEnabled() == false) {
            opensaf_reboot(0, nullptr,
                           "Another controller is taking over the active role. "
                           "Rebooting this node");
          }
        } else if (state == Consensus::TakeoverState::UNDEFINED) {
          bool fencing_required = true;

          // differentiate when this occurs after election or
          // rde has been set active due to failover
          if (consensus_service.IsRelaxedNodePromotionEnabled() == true) {
              if (rde_cb->state == State::kActiveElected) {
                TRACE("Relaxed mode is enabled");
                TRACE(" No peer SC yet seen, ignore consensus service failure");
                // if relaxed node promotion is enabled, and we have yet to see
                // a peer SC after being promoted, tolerate consensus service
                // not working
                fencing_required = false;
              } else if ((rde_cb->state == State::kActiveElectedSeenPeer ||
                         rde_cb->state == State::kActiveFailover) &&
                         role->IsPeerPresent() == true) {
                TRACE("Relaxed mode is enabled");
                TRACE("Peer SC can be seen, ignore consensus service failure");
                // we have seen the peer, and peer is still connected, tolerate
                // consensus service not working
                fencing_required = false;
              }
          }
          if (fencing_required == true) {
            LOG_NO("Lost connectivity to consensus service");
            if (consensus_service.IsRemoteFencingEnabled() == false) {
                opensaf_reboot(0, nullptr,
                               "Lost connectivity to consensus service. "
                               "Rebooting this node");
            }
          }
        }

        LOG_NO("Rejected takeover request");

        rde_cb->monitor_takeover_req_thread_running = true;
        consensus_service.MonitorTakeoverRequest(Role::MonitorCallback,
                                                 rde_cb->mbx);
      } else {
        LOG_WA("Received takeover request when not active");
      }
    } break;
    case RDE_MSG_ACTIVE_PROMOTION_SUCCESS:
      role->NodePromoted();
      break;
    default:
      LOG_ER("%s: discarding unknown message type %u", __FUNCTION__, msg->type);
      break;
  }

  free(msg);

  TRACE_LEAVE();
}

static void CheckForSplitBrain(const rde_msg *msg) {
  PCS_RDA_ROLE own_role = role->role();
  PCS_RDA_ROLE other_role = msg->info.peer_info.ha_role;
  if (own_role == PCS_RDA_ACTIVE && other_role == PCS_RDA_ACTIVE) {
    opensaf_quick_reboot("Split-brain detected");
  }
}

static void SendPeerInfoResp(MDS_DEST mds_dest) {
  rde_msg peer_info_req;
  peer_info_req.type = RDE_MSG_PEER_INFO_RESP;
  peer_info_req.info.peer_info.ha_role = role->role();
  rde_mds_send(&peer_info_req, mds_dest);
}

/**
 * Initialize the RDE server.
 *
 * @return int, 0=OK
 */
static int initialize_rde() {
  RDE_RDA_CB *rde_rda_cb = &rde_cb->rde_rda_cb;
  int rc = NCSCC_RC_FAILURE;

  if ((rc = rde_rda_open(RDE_RDA_SOCK_NAME, rde_rda_cb)) != NCSCC_RC_SUCCESS) {
    goto init_failed;
  }

  /* Determine how this process was started, by NID or AMF */
  if (getenv("SA_AMF_COMPONENT_NAME") == nullptr)
    rde_cb->rde_amf_cb.nid_started = true;

  if ((rc = ncs_core_agents_startup()) != NCSCC_RC_SUCCESS) {
    LOG_ER("ncs_core_agents_startup FAILED");
    goto init_failed;
  }

  own_node_id = ncs_get_node_id();
  role = new Role(own_node_id);
  rde_rda_cb->role = role;
  rde_cb->rde_amf_cb.role = role;

  if (rde_cb->rde_amf_cb.nid_started &&
      (rc = ncs_sel_obj_create(&usr1_sel_obj)) != NCSCC_RC_SUCCESS) {
    LOG_ER("ncs_sel_obj_create FAILED");
    goto init_failed;
  }

  if ((rc = ncs_ipc_create(&rde_cb->mbx)) != NCSCC_RC_SUCCESS) {
    LOG_ER("ncs_ipc_create FAILED");
    goto init_failed;
  }

  if ((rc = ncs_ipc_attach(&rde_cb->mbx)) != NCSCC_RC_SUCCESS) {
    LOG_ER("ncs_ipc_attach FAILED");
    goto init_failed;
  }

  if (rde_cb->rde_amf_cb.nid_started &&
      signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
    LOG_ER("signal USR1 FAILED: %s", strerror(errno));
    goto init_failed;
  }

  if (rde_discovery_mds_register() != NCSCC_RC_SUCCESS) {
    LOG_ER("rde_discovery_mds_register() failed");
    rc = NCSCC_RC_FAILURE;
  }

  rc = NCSCC_RC_SUCCESS;

init_failed:
  return rc;
}

int main(int argc, char *argv[]) {
  nfds_t nfds = FD_CLIENT_START;
  pollfd fds[FD_CLIENT_START + RDA_MAX_CLIENTS];
  int ret;
  NCS_SEL_OBJ mbx_sel_obj;
  RDE_RDA_CB *rde_rda_cb = &rde_cb->rde_rda_cb;
  int term_fd;
  opensaf_reboot_prepare();

  daemonize(argc, argv);

  base::Conf::InitNodeName();

  if (initialize_rde() != NCSCC_RC_SUCCESS) goto init_failed;

  mbx_sel_obj = ncs_ipc_get_sel_obj(&rde_cb->mbx);

  /* If AMF started register immediately */
  if (!rde_cb->rde_amf_cb.nid_started &&
      rde_amf_init(&rde_cb->rde_amf_cb) != NCSCC_RC_SUCCESS) {
    goto init_failed;
  }

  daemon_sigterm_install(&term_fd);

  fds[FD_TERM].fd = term_fd;
  fds[FD_TERM].events = POLLIN;

  /* USR1/AMF fd */
  fds[FD_AMF].fd = rde_cb->rde_amf_cb.nid_started ? usr1_sel_obj.rmv_obj
                                                  : rde_cb->rde_amf_cb.amf_fd;
  fds[FD_AMF].events = POLLIN;

  /* Mailbox */
  fds[FD_MBX].fd = mbx_sel_obj.rmv_obj;
  fds[FD_MBX].events = POLLIN;

  /* RDA server socket */
  fds[FD_RDA_SERVER].fd = rde_cb->rde_rda_cb.fd;
  fds[FD_RDA_SERVER].events = POLLIN;

  if (rde_cb->rde_amf_cb.nid_started) {
    TRACE("NID started");
    if (nid_notify("RDE", NCSCC_RC_SUCCESS, nullptr) != NCSCC_RC_SUCCESS) {
      LOG_ER("nid_notify failed");
      goto init_failed;
    }
  } else {
    TRACE("Not NID started");
  }

  while (1) {
    nfds_t fds_to_poll =
        role->role() != PCS_RDA_UNDEFINED ? nfds : FD_CLIENT_START;
    ret = osaf_poll(fds, fds_to_poll, 0);
    if (ret == 0) {
      timespec ts;
      timespec *timeout = role->Poll(&ts);
      fds_to_poll = role->role() != PCS_RDA_UNDEFINED ? nfds : FD_CLIENT_START;
      ret = osaf_ppoll(fds, fds_to_poll, timeout, nullptr);
    }

    if (ret == -1) {
      if (errno == EINTR) continue;

      LOG_ER("poll failed - %s", strerror(errno));
      break;
    }

    if (fds[FD_TERM].revents & POLLIN) {
      rde_discovery_mds_unregister();
      daemon_exit();
    }

    if (fds[FD_AMF].revents & POLLIN) {
      if (rde_cb->rde_amf_cb.amf_hdl != 0) {
        SaAisErrorT error;
        TRACE("AMF event rec");
        if ((error = saAmfDispatch(rde_cb->rde_amf_cb.amf_hdl,
                                   SA_DISPATCH_ALL)) != SA_AIS_OK) {
          LOG_ER("saAmfDispatch failed: %u", error);
          goto done;
        }
      } else {
        TRACE("SIGUSR1 event rec");
        ncs_sel_obj_destroy(&usr1_sel_obj);

        if (rde_amf_init(&rde_cb->rde_amf_cb) != NCSCC_RC_SUCCESS) goto done;

        fds[FD_AMF].fd = rde_cb->rde_amf_cb.amf_fd;
      }
    }

    if (fds[FD_MBX].revents & POLLIN) handle_mbx_event();

    if (fds[FD_RDA_SERVER].revents & POLLIN) {
      int newsockfd;

      newsockfd = accept(rde_rda_cb->fd, nullptr, nullptr);
      if (newsockfd < 0) {
        LOG_ER("accept FAILED %s", strerror(errno));
        goto done;
      }

      /* Add the new client fd to client-list */
      rde_rda_cb->clients[rde_rda_cb->client_count].is_async = false;
      rde_rda_cb->clients[rde_rda_cb->client_count].fd = newsockfd;
      rde_rda_cb->client_count++;

      /* Update poll fd selection */
      fds[nfds].fd = newsockfd;
      fds[nfds].events = POLLIN;
      nfds++;

      TRACE("accepted new client, fd=%d, idx=%d, nfds=%lu", newsockfd,
            rde_rda_cb->client_count, nfds);
    }

    for (nfds_t i = FD_CLIENT_START;
         role->role() != PCS_RDA_UNDEFINED && i < fds_to_poll; i++) {
      if (fds[i].revents & POLLIN) {
        int client_disconnected = 0;
        TRACE("received msg on fd %u", fds[i].fd);
        rde_rda_client_process_msg(rde_rda_cb, fd_to_client_ixd(fds[i].fd),
                                   &client_disconnected);
        if (client_disconnected) {
          /* reinitialize the fd array & nfds */
          nfds = FD_CLIENT_START;
          for (int j = 0; j < rde_rda_cb->client_count; j++, nfds++) {
            fds[j + FD_CLIENT_START].fd = rde_rda_cb->clients[j].fd;
            fds[j + FD_CLIENT_START].events = POLLIN;
          }
          TRACE("client disconnected, fd array reinitialized, nfds=%lu", nfds);
          break;
        }
      }
    }
  }

init_failed:
  if (rde_cb->rde_amf_cb.nid_started &&
      nid_notify("RDE", NCSCC_RC_FAILURE, nullptr) != NCSCC_RC_SUCCESS) {
    LOG_ER("nid_notify failed");
  }

done:
  syslog(LOG_ERR, "Exiting...");
  exit(1);
}
