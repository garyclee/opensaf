/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
 * Copyright Ericsson AB 2017 - All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.z
 *
 * Author(s): GoAhead Software
 *
 */

#include "dtm/dtmnd/dtm_node.h"
#include <limits.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "base/ncsencdec_pub.h"
#include "base/osaf_utility.h"
#include "dtm/dtmnd/multicast.h"
#include "dtm/dtmnd/dtm.h"
#include "dtm/dtmnd/dtm_inter.h"
#include "dtm/dtmnd/dtm_inter_disc.h"
#include "dtm/dtmnd/dtm_inter_trans.h"

#define DTM_INTERNODE_RECV_BUFFER_SIZE 1024

/* packet_size +  mds_indentifier + mds_version +  msg_type  +node_id +
 * node_name_len + node_name */
#define NODE_INFO_HDR_SIZE 16

#define NODE_INFO_PKT_SIZE (NODE_INFO_HDR_SIZE + _POSIX_HOST_NAME_MAX)

static void ReceiveBcastOrMcast();
static void AcceptTcpConnections(uint8_t *node_info_hrd,
                                 int node_info_buffer_len);
static void ReceiveFromMailbox();
static void AddNodeToEpoll(DTM_INTERNODE_CB *dtms_cb, DTM_NODE_DB *node);
static void RemoveNodeFromEpoll(DTM_INTERNODE_CB *dtms_cb, DTM_NODE_DB *node);

static DTM_NODE_DB dgram_sock_rcvr;
static DTM_NODE_DB stream_sock;
static DTM_NODE_DB mbx_fd;

/**
 * Function to construct the node info hdr
 *
 * @param dtms_cb buf_ptr pack_size
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
static uint32_t dtm_construct_node_info_hdr(DTM_INTERNODE_CB *dtms_cb,
                                            uint8_t *buf_ptr, int *pack_size) {
  /* Add the FRAG HDR to the Buffer */
  uint8_t *data;
  data = buf_ptr;

  *pack_size = NODE_INFO_HDR_SIZE + strlen(dtms_cb->node_name);

  ncs_encode_16bit(&data, static_cast<uint16_t>(*pack_size - 2)); /*pkt_type  */
  ncs_encode_32bit(&data, DTM_INTERNODE_SND_MSG_IDENTIFIER);
  ncs_encode_8bit(&data, DTM_INTERNODE_SND_MSG_VER);
  ncs_encode_8bit(&data, uint8_t{DTM_CONN_DETAILS_MSG_TYPE});
  ncs_encode_32bit(&data, dtms_cb->node_id);
  ncs_encode_32bit(&data, strlen(dtms_cb->node_name));
  ncs_encode_octets(&data, reinterpret_cast<uint8_t *>(dtms_cb->node_name),
                    static_cast<uint8_t>(strlen(dtms_cb->node_name)));

  return NCSCC_RC_SUCCESS;
}

/**
 * Function to process the node info hdr
 *
 * @param dtms_cb stream_sock buffer node_info_hrd buffer_len
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t dtm_process_node_info(DTM_INTERNODE_CB *dtms_cb, DTM_NODE_DB *node,
                               uint8_t *buffer, uint16_t buffer_size,
                               uint8_t *node_info_hrd, int buffer_len) {
  uint32_t node_id;
  uint32_t nodename_len;
  uint32_t rc = NCSCC_RC_SUCCESS;
  uint8_t *data = buffer;
  TRACE_ENTER();

  if (buffer_size < 2 * sizeof(uint32_t)) {
    LOG_ER("Received node_info of size %" PRIu16, buffer_size);
    rc = NCSCC_RC_FAILURE;
    goto done;
  }
  node_id = ncs_decode_32bit(&data);
  nodename_len = ncs_decode_32bit(&data);
  if (nodename_len >= sizeof(node->node_name) ||
      nodename_len > buffer_size - 2 * sizeof(uint32_t)) {
    LOG_ER("Received node_info of size %" PRIu16 " with nodename_len %" PRIu32,
           buffer_size, nodename_len);
    rc = NCSCC_RC_FAILURE;
    goto done;
  }

  if (!node->comm_status) {
    /*****************************************************/
    /* nodeinfo data back to the client  NODE is still */
    /*****************************************************/

    if (node->node_id == 0) {
      node->node_id = node_id;
      memcpy(node->node_name, data, nodename_len);
      node->node_name[nodename_len] = '\0';
      node->comm_status = true;
      if (dtm_node_add(node, KeyTypes::kDtmNodeIdKeyType) != NCSCC_RC_SUCCESS) {
        LOG_ER(
            "DTM:  A node already exists in the cluster with similar "
            "configuration (possible duplicate IP address and/or node id), please "
            "correct the other joining Node configuration");
        rc = NCSCC_RC_FAILURE;
        goto done;
      }
    } else if (node->node_id == node_id) {
      memcpy(node->node_name, data, nodename_len);
      node->node_name[nodename_len] = '\0';
      rc = dtm_comm_socket_send(node->comm_socket, node_info_hrd, buffer_len);
      if (rc != NCSCC_RC_SUCCESS) {
        LOG_ER("DTM: dtm_comm_socket_send() failed rc : %d", rc);
        rc = NCSCC_RC_FAILURE;
        goto done;
      }
      node->comm_status = true;
    } else {
      LOG_ER(
          "DTM:  A node already exists in the cluster with similar "
          "configuration (possible duplicate IP address and/or node id), please "
          "correct the other joining Node configuration");
      rc = NCSCC_RC_FAILURE;
      goto done;
    }

    TRACE(
        "DTM: dtm_process_node_info node_name: '%s' node_ip:%s, "
        "node_id:%u i_addr_family:%d ",
        node->node_name, node->node_ip, node->node_id, node->i_addr_family);
    rc = dtm_process_node_up_down(node->node_id, node->node_name, node->node_ip,
                                  node->i_addr_family, node->comm_status);

    if (rc != NCSCC_RC_SUCCESS) {
      LOG_ER("DTM: dtm_process_node_up_down() failed rc : %d ", rc);
      rc = NCSCC_RC_FAILURE;
    }
  } else {
    LOG_ER("DTM: Node down already  received  for this node ");
    osafassert(0);
  }

done:
  TRACE_LEAVE();
  return rc;
}

/**
 * Function to process node up and down
 *
 * @param node_id node_name comm_status
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t dtm_process_node_up_down(NODE_ID node_id, char *node_name,
                                  char *node_ip, sa_family_t i_addr_family,
                                  bool comm_status) {
  if (comm_status == true) {
    TRACE(
        "DTM: dtm_process_node_up_down node_ip:%s, node_id:%u i_addr_family:%d ",
        node_ip, node_id, i_addr_family);
    dtm_node_up(node_id, node_name, node_ip, i_addr_family, 0);
  } else {
    dtm_node_down(node_id, node_name, 0);
  }
  return NCSCC_RC_SUCCESS;
}

/**
 * Function to process internode poll and rcv message
 *
 * @param node local_len_buf node_info_hrd node_info_buffer_len close_conn fd
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
void dtm_internode_process_poll_rcv_msg_common(
    DTM_NODE_DB *node, uint8_t *buffer, uint16_t local_len_buf,
    uint8_t *node_info_hrd, uint16_t node_info_buffer_len, bool *close_conn) {
  uint8_t *data = buffer;
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;

  if (local_len_buf < 6) {
    LOG_ER("DTM: Malformed message received, size = %" PRId16, local_len_buf);
    return;
  }

  /* Decode the message */
  uint32_t identifier = ncs_decode_32bit(&data);
  uint8_t version = ncs_decode_8bit(&data);

  if ((DTM_INTERNODE_RCV_MSG_IDENTIFIER != identifier) ||
      (DTM_INTERNODE_RCV_MSG_VER != version)) {
    LOG_ER("DTM: Malformed packet recd, Ident : %d, ver : %d", identifier,
           version);
    return;
  }

  DTM_MSG_TYPES pkt_type = static_cast<DTM_MSG_TYPES>(ncs_decode_8bit(&data));

  if (pkt_type == DTM_UP_MSG_TYPE) {
    uint8_t *alloc_buffer = static_cast<uint8_t *>(malloc(local_len_buf - 6));
    if (alloc_buffer == nullptr) {
      LOG_ER("Memory allocation failed in dtm_internode_processing");
      return;
    }
    memcpy(alloc_buffer, buffer + 6, local_len_buf - 6);
    dtm_internode_process_rcv_up_msg(alloc_buffer, local_len_buf - 6,
                                     node->node_id);
  } else if (pkt_type == DTM_CONN_DETAILS_MSG_TYPE) {
    if (dtm_process_node_info(dtms_cb, node, buffer + 6, local_len_buf - 6,
                              node_info_hrd,
                              node_info_buffer_len) != NCSCC_RC_SUCCESS) {
      LOG_ER(" DTM : communication socket Connection closed");
      *close_conn = true;
    }
  } else if (pkt_type == DTM_DOWN_MSG_TYPE) {
    uint8_t *alloc_buffer = static_cast<uint8_t *>(malloc(local_len_buf - 6));
    if (alloc_buffer == nullptr) {
      LOG_ER("Memory allocation failed in dtm_internode_processing");
      return;
    }
    memcpy(alloc_buffer, buffer + 6, local_len_buf - 6);
    dtm_internode_process_rcv_down_msg(alloc_buffer, local_len_buf - 6,
                                       node->node_id);
  } else if (pkt_type == DTM_MESSAGE_MSG_TYPE) {
    if (local_len_buf < 14) {
      LOG_ER("DTM: Malformed DTM_MESSAGE received, size = %" PRId16,
             local_len_buf);
      return;
    }
    uint8_t *alloc_buffer = static_cast<uint8_t *>(malloc(local_len_buf + 2));
    if (alloc_buffer == nullptr) {
      LOG_ER("Memory allocation failed in dtm_internode_processing");
      return;
    }
    memcpy(alloc_buffer + 8, buffer + 6, local_len_buf - 6);
    NODE_ID dst_nodeid = ncs_decode_32bit(&data);
    if (dtms_cb->node_id != dst_nodeid)
      LOG_ER("Invalid dest_nodeid: %u received in dtm_internode_processing",
             dst_nodeid);
    uint32_t dst_processid = ncs_decode_32bit(&data);
    dtm_internode_process_rcv_data_msg(alloc_buffer, dst_processid,
                                       local_len_buf + 2);
  } else {
    LOG_ER("Unknown packet type %d received from node 0x%" PRIx32, pkt_type,
           node->node_id);
  }
}

/**
 * Function to process internode poll and rcv message
 *
 * @param fd close_conn node_info_hrd node_info_buffer_len
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
void dtm_internode_process_poll_rcv_msg(DTM_NODE_DB *node, bool *close_conn,
                                        uint8_t *node_info_hrd,
                                        uint16_t node_info_buffer_len) {
  TRACE_ENTER();
  for (;;) {
    ssize_t recd_bytes;
    do {
      recd_bytes =
          recv(node->comm_socket, node->recvbuf + node->recvbuf_size,
               sizeof(node->recvbuf) - node->recvbuf_size, MSG_DONTWAIT);
    } while (recd_bytes < 0 && errno == EINTR);
    if (recd_bytes == 0) {
      TRACE("Node 0x%" PRIx32 " closed the connection",
            static_cast<uint32_t>(node->node_id));
      *close_conn = true;
      break;
    }
    if (recd_bytes < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        if (errno == ECONNRESET) {
          TRACE("Connection reset by node 0x%" PRIx32,
                static_cast<uint32_t>(node->node_id));
        } else {
          LOG_ER("recv() from node 0x%" PRIx32 " failed, errno=%d",
                 static_cast<uint32_t>(node->node_id), errno);
        }
        *close_conn = true;
      }
      break;
    }
    node->recvbuf_size += recd_bytes;
    while (node->recvbuf_size >= sizeof(uint16_t)) {
      uint16_t message_size = (static_cast<uint16_t>(node->recvbuf[0]) << 8) |
                              static_cast<uint16_t>(node->recvbuf[1]);
      uint32_t total_size =
          sizeof(uint16_t) + (static_cast<uint32_t>(message_size));
      if (node->recvbuf_size < total_size) break;
      dtm_internode_process_poll_rcv_msg_common(
          node, node->recvbuf + sizeof(uint16_t), message_size, node_info_hrd,
          node_info_buffer_len, close_conn);
      node->recvbuf_size -= total_size;
      memmove(node->recvbuf, node->recvbuf + total_size, node->recvbuf_size);
    }
  }
  TRACE_LEAVE();
}

/**
 * Function to handle node discovery process
 *
 * @param args
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
void node_discovery_process(void *arg) {
  TRACE_ENTER();

  bool close_conn = false;
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;

  /* Data Received */
  int node_info_buffer_len = 0;
  uint8_t node_info_hrd[NODE_INFO_PKT_SIZE];

  dgram_sock_rcvr.comm_socket = dtms_cb->multicast_->fd();
  stream_sock.comm_socket = dtms_cb->stream_sock;
  mbx_fd.comm_socket = dtms_cb->mbx_fd;
  AddNodeToEpoll(dtms_cb, &dgram_sock_rcvr);
  AddNodeToEpoll(dtms_cb, &stream_sock);
  AddNodeToEpoll(dtms_cb, &mbx_fd);

  /*************************************************************/
  /* Set up the initial listening socket */
  /*************************************************************/

  if (dtm_construct_node_info_hdr(dtms_cb, node_info_hrd,
                                  &node_info_buffer_len) != NCSCC_RC_SUCCESS) {
    LOG_ER("DTM: dtm_construct_node_info_hdr failed");
    goto done;
  }

  /*************************************************************/
  /* Loop waiting for incoming connects or for incoming data */
  /* on any of the connected sockets. */
  /*************************************************************/

  for (;;) {
    /***********************************************************/
    /* Call poll() and wait . */
    /***********************************************************/
    struct epoll_event events[128];
    int poll_ret;
    do {
      poll_ret = epoll_wait(dtms_cb->epoll_fd, &events[0],
                            sizeof(events) / sizeof(events[0]), -1);
    } while (poll_ret < 0 && errno == EINTR);
    /***********************************************************/

    /* Check to see if the poll call failed. */
    /***********************************************************/
    if (poll_ret < 0) {
      LOG_ER("epoll_wait() failed: %d", errno);
      break;
    }

    /***********************************************************/
    /* One or more descriptors are readable. Need to */
    /* determine which ones they are. */
    /***********************************************************/
    for (int i = 0; i < poll_ret; ++i) {
      DTM_NODE_DB *node = static_cast<DTM_NODE_DB *>(events[i].data.ptr);

      /*********************************************************/
      /* Loop through to find the descriptors that returned */
      /* EPOLLIN and determine whether it's the listening */
      /* or the active connection. */
      /*********************************************************/
      if ((events[i].events & EPOLLIN) != 0) {
        if (node == &dgram_sock_rcvr) {
          /* Data Received */
          ReceiveBcastOrMcast();
        } else if (node == &stream_sock) {
          /*******************************************************/
          /* Listening descriptor is readable. */
          /*******************************************************/
          TRACE(" DTM :Listening socket is readable");
          AcceptTcpConnections(node_info_hrd, node_info_buffer_len);
        } else if (node == &mbx_fd) {
          /* MBX fd messages that need to be sent
           * out from this node */
          /* Process the mailbox events */
          ReceiveFromMailbox();
        } else {
          /*********************************************************/
          /* This is not the listening socket,
           * therefore an */
          /* existing connection must be readable
           */
          /*********************************************************/
          dtm_internode_process_poll_rcv_msg(node, &close_conn, node_info_hrd,
                                             node_info_buffer_len);
        }
      } else if ((events[i].events & EPOLLOUT) != 0) {
        dtm_internode_process_pollout(node);
      } else if ((events[i].events & EPOLLHUP) != 0) {
        close_conn = true;
      }

      /*******************************************************/
      /* If the close_conn flag was turned on, we need */
      /* to clean up this active connection. This */
      /* clean up process includes removing the */
      /* descriptor. */
      /*******************************************************/
      if (close_conn) {
        if (node == &dgram_sock_rcvr || node == &stream_sock ||
            node == &mbx_fd) {
          osaf_abort(node->comm_socket);
        }
        close_conn = false;
        RemoveNodeFromEpoll(dtms_cb, node);
        dtm_comm_socket_close(node);
      }
    }
  }

/* End of serving running. */
/*************************************************************/
/* Clean up all of the sockets that are open */
/*************************************************************/
done:
  TRACE_LEAVE();
  return;
}

static void ReceiveBcastOrMcast() {
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  uint8_t inbuf[DTM_INTERNODE_RECV_BUFFER_SIZE];
  ssize_t recd_bytes;
  do {
    recd_bytes = dtms_cb->multicast_->Receive(inbuf, sizeof(inbuf));
    if (recd_bytes >= static_cast<ssize_t>(sizeof(uint16_t))) {
      uint8_t *data1 = inbuf;
      uint16_t recd_buf_len = ncs_decode_16bit(&data1);
      if (recd_buf_len == static_cast<size_t>(recd_bytes)) {
        DTM_NODE_DB *new_node = dtm_process_connect(
            dtms_cb, data1, (recd_bytes - sizeof(uint16_t)));
        if (new_node != nullptr) {
          // Add the new incoming connection to
          // the pollfd structure
          LOG_IN("DTM: add New incoming connection to fd : %d\n",
                 new_node->comm_socket);
          AddNodeToEpoll(dtms_cb, new_node);
        }
      } else {
        // Log message that we are dropping the data
        LOG_ER("DTM: BRoadcastLEN-MISMATCH %" PRIu16 "/%zd: dropping the data",
               recd_buf_len, recd_bytes);
      }
    } else {
      if (recd_bytes >= 0)
        LOG_ER("DTM: recd bytes=%zd on DGRAM sock", recd_bytes);
    }
  } while (recd_bytes >= 0);
}

static void AcceptTcpConnections(uint8_t *node_info_hrd,
                                 int node_info_buffer_len) {
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  DTM_NODE_DB *new_node;
  while ((new_node = dtm_process_accept(dtms_cb, dtms_cb->stream_sock)) !=
         nullptr) {
    if (dtm_comm_socket_send(new_node->comm_socket, node_info_hrd,
                             node_info_buffer_len) == NCSCC_RC_SUCCESS) {
      TRACE("DTM: add New incoming connection to fd: %d",
            new_node->comm_socket);
      AddNodeToEpoll(dtms_cb, new_node);
    } else {
      dtm_comm_socket_close(new_node);
      LOG_ER("DTM: send() failed");
    }
  }
}

static void ReceiveFromMailbox() {
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  DTM_SND_MSG_ELEM *msg_elem;
  while ((msg_elem = reinterpret_cast<DTM_SND_MSG_ELEM *>(
              ncs_ipc_non_blk_recv(&dtms_cb->mbx))) != nullptr) {
    if (msg_elem->type == DTM_MBX_ADD_DISTR_TYPE) {
      dtm_internode_add_to_svc_dist_list(msg_elem->info.svc_event.server_type,
                                         msg_elem->info.svc_event.server_inst,
                                         msg_elem->info.svc_event.pid);
    } else if (msg_elem->type == DTM_MBX_DEL_DISTR_TYPE) {
      dtm_internode_del_from_svc_dist_list(msg_elem->info.svc_event.server_type,
                                           msg_elem->info.svc_event.server_inst,
                                           msg_elem->info.svc_event.pid);
    } else if (msg_elem->type == DTM_MBX_DATA_MSG_TYPE) {
      dtm_prepare_data_msg(msg_elem->info.data.buffer,
                           msg_elem->info.data.buff_len);
      dtm_internode_snd_msg_to_node(msg_elem->info.data.buffer,
                                    msg_elem->info.data.buff_len,
                                    msg_elem->info.data.dst_nodeid);
    } else {
      LOG_ER("DTM Intranode :Invalid evt type from mbx");
    }
    free(msg_elem);
  }
}

static void AddNodeToEpoll(DTM_INTERNODE_CB *dtms_cb, DTM_NODE_DB *node) {
  struct epoll_event event = {EPOLLIN, {.ptr = node}};
  if (epoll_ctl(dtms_cb->epoll_fd, EPOLL_CTL_ADD, node->comm_socket, &event) !=
      0) {
    LOG_ER("DTM: epoll_ctl(%d, EPOLL_CTL_ADD, %d) failed: %d",
           dtms_cb->epoll_fd, node->comm_socket, errno);
    exit(EXIT_FAILURE);
  }
}

static void RemoveNodeFromEpoll(DTM_INTERNODE_CB *dtms_cb, DTM_NODE_DB *node) {
  if (epoll_ctl(dtms_cb->epoll_fd, EPOLL_CTL_DEL, node->comm_socket, nullptr) !=
      0) {
    LOG_ER("DTM: epoll_ctl(%d, EPOLL_CTL_DEL, %d) failed: %d",
           dtms_cb->epoll_fd, node->comm_socket, errno);
    exit(EXIT_FAILURE);
  }
}

/**
 * Function to set poll fdlist
 *
 * @param fd events
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
void dtm_internode_set_pollout(DTM_NODE_DB *node) {
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  struct epoll_event event = {EPOLLIN | EPOLLOUT, {.ptr = node}};
  if (epoll_ctl(dtms_cb->epoll_fd, EPOLL_CTL_MOD, node->comm_socket, &event) ==
      0) {
    TRACE("event set success, in the poll fd list");
  } else {
    LOG_ER("DTM: epoll_ctl(%d, EPOLL_CTL_MOD, %d) failed: %d",
           dtms_cb->epoll_fd, node->comm_socket, errno);
  }
}

/**
 * Function to reset poll fdlist
 *
 * @param fd
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
void dtm_internode_clear_pollout(DTM_NODE_DB *node) {
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  struct epoll_event event = {EPOLLIN, {.ptr = node}};
  if (epoll_ctl(dtms_cb->epoll_fd, EPOLL_CTL_MOD, node->comm_socket, &event) ==
      0) {
    TRACE("event set success, in the poll fd list");
  } else {
    LOG_ER("DTM: epoll_ctl(%d, EPOLL_CTL_MOD, %d) failed: %d",
           dtms_cb->epoll_fd, node->comm_socket, errno);
  }
}
