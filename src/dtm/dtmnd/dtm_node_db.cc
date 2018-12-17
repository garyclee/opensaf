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

#include <sys/epoll.h>
#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "base/ncs_main_papi.h"
#include "base/usrbuf.h"
#include "dtm/dtmnd/dtm.h"

/**
 * Function to add new node
 *
 * @param new_node
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
DTM_NODE_DB *dtm_node_new(const DTM_NODE_DB *new_node) {
  TRACE_ENTER();
  DTM_NODE_DB *node = static_cast<DTM_NODE_DB *>(malloc(sizeof(DTM_NODE_DB)));
  if (node != nullptr) {
    memcpy(node, new_node, sizeof(DTM_NODE_DB));
  } else {
    LOG_ER("malloc failed");
  }
  TRACE_LEAVE();
  return node;
}

/**
 * This function initializes the DTMS_CB including the Patricia trees
 *
 * @param dtms_cb
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t dtm_cb_init(DTM_INTERNODE_CB *dtms_cb) {
  NCS_PATRICIA_PARAMS nodeid_param;
  NCS_PATRICIA_PARAMS ipaddr_param;

  TRACE_ENTER();

  dtms_cb->node_id = ncs_get_node_id();
  if (dtms_cb->node_id == 0) {
    LOG_ER("node_id cannot be zero");
    return NCSCC_RC_FAILURE;
  }

  memset(&nodeid_param, 0, sizeof(NCS_PATRICIA_PARAMS));
  memset(&ipaddr_param, 0, sizeof(NCS_PATRICIA_PARAMS));

  nodeid_param.key_size = sizeof(uint32_t);
  ipaddr_param.key_size = INET6_ADDRSTRLEN;

  dtms_cb->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (dtms_cb->epoll_fd < 0) {
    LOG_ER("DTM: epoll_create() failed: %d", errno);
    return NCSCC_RC_FAILURE;
  }

  /* Initialize patricia tree for nodeid list */
  if (NCSCC_RC_SUCCESS !=
      ncs_patricia_tree_init(&dtms_cb->nodeid_tree, &nodeid_param)) {
    LOG_ER("DTM: ncs_patricia_tree_init FAILED");
    close(dtms_cb->epoll_fd);
    return NCSCC_RC_FAILURE;
  }

  /* Initialize comm_socket patricia tree */
  if (NCSCC_RC_SUCCESS !=
      ncs_patricia_tree_init(&dtms_cb->ip_addr_tree, &ipaddr_param)) {
    LOG_ER("DTM:ncs_patricia_tree_init FAILED");
    close(dtms_cb->epoll_fd);
    return NCSCC_RC_FAILURE;
  }

  if (m_NCS_IPC_CREATE(&dtms_cb->mbx) != NCSCC_RC_SUCCESS) {
    /* Mail box creation failed */
    LOG_ER("DTM:IPC create FAILED");
    close(dtms_cb->epoll_fd);
    return NCSCC_RC_FAILURE;
  } else {
    NCS_SEL_OBJ obj;

    if (NCSCC_RC_SUCCESS != m_NCS_IPC_ATTACH(&dtms_cb->mbx)) {
      m_NCS_IPC_RELEASE(&dtms_cb->mbx, nullptr);
      LOG_ER("DTM: Internode Mailbox  Attach failed");
      close(dtms_cb->epoll_fd);
      return NCSCC_RC_FAILURE;
    }

    obj = m_NCS_IPC_GET_SEL_OBJ(&dtms_cb->mbx);

    /* retreive the corresponding fd for mailbox and fill it in cb
     */
    dtms_cb->mbx_fd = m_GET_FD_FROM_SEL_OBJ(
        obj); /* extract and fill value needs to be extracted */
  }

  TRACE_LEAVE();
  return NCSCC_RC_SUCCESS;
}

/**
 * Retrieve node from node db
 *
 * @param key
 * @param i
 *
 * @return node
 *
 */
DTM_NODE_DB *dtm_node_get(uint8_t *key, KeyTypes type) {
  TRACE_ENTER();
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  DTM_NODE_DB *node = nullptr;

  osafassert(key != nullptr);

  switch (type) {
    case KeyTypes::kDtmNodeIdKeyType:
      TRACE("DTM: Getting node from the database by node_id : %u as key",
            *reinterpret_cast<NODE_ID *>(key));
      node = reinterpret_cast<DTM_NODE_DB *>(ncs_patricia_tree_get(
          &dtms_cb->nodeid_tree, key));
      if (node != nullptr) {
        // Adjust the pointer
        node = reinterpret_cast<DTM_NODE_DB *>(reinterpret_cast<char *>(node) -
            offsetof(DTM_NODE_DB, pat_nodeid));
      }
      break;

    case KeyTypes::kDtmNodeIpKeyType:
      TRACE("DTM: Getting node from the database by node_ip : %s as key",
            reinterpret_cast<char *>(key));
      node = reinterpret_cast<DTM_NODE_DB *>(ncs_patricia_tree_get(
          &dtms_cb->ip_addr_tree, key));
      if (node != nullptr) {
        // Adjust the pointer
        node = reinterpret_cast<DTM_NODE_DB *>(reinterpret_cast<char *>(node) -
            offsetof(DTM_NODE_DB, pat_ip_address));
      }
      break;

    default:
      osafassert(false);
      break;
  }

  TRACE_LEAVE();
  return node;
}

/**
 * Retrieve next node from node db by nodeid
 *
 * @param nodeid
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
DTM_NODE_DB *dtm_node_getnext_by_id(uint32_t node_id) {
  DTM_NODE_DB *node = nullptr;
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  TRACE_ENTER();

  if (node_id == 0) {
    node = reinterpret_cast<DTM_NODE_DB *>(
        ncs_patricia_tree_getnext(&dtms_cb->nodeid_tree, nullptr));
  } else {
    node = reinterpret_cast<DTM_NODE_DB *>(ncs_patricia_tree_getnext(
        &dtms_cb->nodeid_tree, reinterpret_cast<uint8_t *>(&node_id)));
  }

  if (node != nullptr) {
    /* Adjust the pointer */
    node = reinterpret_cast<DTM_NODE_DB *>(reinterpret_cast<char *>(node) -
                                           offsetof(DTM_NODE_DB, pat_nodeid));
    TRACE("DTM:Node found %d", node->node_id);
  }

  TRACE_LEAVE();
  return node;
}

/**
 * Adds the node to patricia tree
 *
 * @param node
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t dtm_node_add(DTM_NODE_DB *node, KeyTypes type) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  TRACE_ENTER2("DTM:value of type %d", static_cast<int>(type));

  osafassert(node != nullptr);

  switch (type) {
    case KeyTypes::kDtmNodeIdKeyType:
      TRACE("DTM:Adding node_id to the database with node_id :%u as key",
            node->node_id);
      node->pat_nodeid.key_info = reinterpret_cast<uint8_t *>(&(node->node_id));
      rc = ncs_patricia_tree_add(&dtms_cb->nodeid_tree, &node->pat_nodeid);
      if (rc != NCSCC_RC_SUCCESS) {
        TRACE("DTM:ncs_patricia_tree_add for node_id  FAILED for :%d :%u",
              node->node_id, rc);
        node->pat_nodeid.key_info = nullptr;
        goto done;
      }
      break;

    case KeyTypes::kDtmNodeIpKeyType:
      TRACE("DTM:Adding node_ip to the database with node_ip :%s as key",
            node->node_ip);
      node->pat_ip_address.key_info =
          reinterpret_cast<uint8_t *>(&(node->node_ip));
      rc = ncs_patricia_tree_add(&dtms_cb->ip_addr_tree, &node->pat_ip_address);
      if (rc != NCSCC_RC_SUCCESS) {
        TRACE("DTM:ncs_patricia_tree_add for node_ip  FAILED for :%s :%u",
              node->node_ip, rc);
        node->pat_ip_address.key_info = nullptr;
        goto done;
      }
      break;

    default:
      TRACE("DTM:Invalid Patricia add");
      osafassert(false);
      break;
  }

done:
  TRACE_LEAVE2("rc : %d", rc);
  return rc;
}

/**
 * Delete the node from the nodedb
 *
 * @param node
 *
 * @return NCSCC_RC_SUCCESS
 * @return NCSCC_RC_FAILURE
 *
 */
uint32_t dtm_node_delete(DTM_NODE_DB *node, KeyTypes type) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  DTM_INTERNODE_CB *dtms_cb = dtms_gl_cb;
  TRACE_ENTER2("DTM:value of type %d", static_cast<int>(type));

  osafassert(node != nullptr);

  switch (type) {
    case KeyTypes::kDtmNodeIdKeyType:
      if (node->node_id != 0 && node->pat_nodeid.key_info) {
        TRACE("DTM:Deleting node_id from the database with node_id :%u as key",
              node->node_id);
        if ((rc = ncs_patricia_tree_del(&dtms_cb->nodeid_tree,
                                        &node->pat_nodeid)) !=
            NCSCC_RC_SUCCESS) {
          TRACE("DTM:ncs_patricia_tree_del FAILED for node_id :%u rc :%d",
                node->node_id, rc);
          goto done;
        }
      }
      break;

    case KeyTypes::kDtmNodeIpKeyType:
      if (node->node_ip != nullptr && node->pat_ip_address.key_info) {
        TRACE("DTM:Deleting node_ip from the  database with node_ip :%s as key",
              node->node_ip);
        if ((rc = ncs_patricia_tree_del(&dtms_cb->ip_addr_tree,
                                        &node->pat_ip_address)) !=
            NCSCC_RC_SUCCESS) {
          TRACE("DTM:ncs_patricia_tree_del FAILED for node_ip :%s rc :%u",
                node->node_ip, rc);
          goto done;
        }
      }
      break;

    default:
      TRACE(
          "DTM:Deleting node from the database  with unknown option :%d as key",
          static_cast<int>(type));
      osafassert(0);
  }

done:
  TRACE_LEAVE();
  return rc;
}
