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

#ifndef DTM_DTMND_DTM_H_
#define DTM_DTMND_DTM_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdint>
#include "dtm/dtmnd/dtm_cb.h"
#include "mds/mds_papi.h"

extern DTM_INTERNODE_CB *dtms_gl_cb;
extern bool initial_discovery_phase;

#define BMCAST_MSG_LEN ( sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t)

#define m_NODE_DISCOVERY_TASKNAME "NODE_DISCOVERY"
#define m_NODE_DISCOVERY_STACKSIZE NCS_STACKSIZE_HUGE

/* The default value of SO_RCVBUF & SO_SNDBUF , it is set by the
 * rmem_default/wmem_default  */
#define DTM_MAX_TAG_LEN 256

typedef enum {
  DTM_MBX_UP_TYPE = 1,
  DTM_MBX_DOWN_TYPE = 2,
  DTM_MBX_NODE_UP_TYPE = 3,
  DTM_MBX_NODE_DOWN_TYPE = 4,
  DTM_MBX_MSG_TYPE = 5,
} MBX_POST_TYPES;

typedef struct dtm_rcv_msg_elem {
  void *next;
  MBX_POST_TYPES type;
  union {
    struct {
      uint16_t len;
      uint8_t *buffer;
      uint32_t dst_pid;
    } data;

    struct {
      uint16_t len;
      uint8_t *buffer;
      NODE_ID node_id;
    } svc_event;

    struct {
      char node_name[255];
      NODE_ID node_id;
      SYSF_MBX mbx;
      sa_family_t i_addr_family; /* Indicates V4 or V6 */
      char node_ip[INET6_ADDRSTRLEN];

    } node;
  } info;

} DTM_RCV_MSG_ELEM;

typedef enum {
  DTM_MBX_ADD_DISTR_TYPE = 1,
  DTM_MBX_DEL_DISTR_TYPE = 2,
  DTM_MBX_DATA_MSG_TYPE = 3,
} MBX_SND_POST_TYPES;

typedef struct dtm_snd_msg_elem {
  void *next;
  MBX_SND_POST_TYPES type;
  uint8_t pri;
  union {
    struct {
      uint32_t server_type;
      uint32_t server_inst;
      uint32_t pid;
    } svc_event;
    struct {
      NODE_ID dst_nodeid;
      uint16_t buff_len;
      uint8_t *buffer;
    } data;
  } info;
} DTM_SND_MSG_ELEM;

enum class KeyTypes{
  kDtmNodeIdKeyType = 0,
  kDtmNodeIpKeyType = 2,
};

extern void node_discovery_process(void *arg);
extern uint32_t dtm_cb_init(DTM_INTERNODE_CB *dtms_cb);
extern DTM_NODE_DB *dtm_node_get(uint8_t *key, KeyTypes type);
extern DTM_NODE_DB *dtm_node_getnext_by_id(uint32_t node_id);
extern uint32_t dtm_node_add(DTM_NODE_DB *node, KeyTypes type);
extern uint32_t dtm_node_delete(DTM_NODE_DB *nnode, KeyTypes type);
extern DTM_NODE_DB *dtm_node_new(const DTM_NODE_DB *new_node);
extern void dtm_print_config(DTM_INTERNODE_CB *config);
extern int dtm_read_config(DTM_INTERNODE_CB *config,
                           const char *dtm_config_file);
extern uint32_t dtm_service_discovery_init(DTM_INTERNODE_CB *dtms_cb);

#endif  // DTM_DTMND_DTM_H_
