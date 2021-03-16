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

#ifndef DTM_DTMND_DTM_NODE_H_
#define DTM_DTMND_DTM_NODE_H_
#define DTM_TCP_TIMEOUT_SECS 10

#include <sys/socket.h>
#include <cstddef>
#include <cstdint>
#include "dtm/dtmnd/dtm_cb.h"
#include "mds/mds_papi.h"

extern char *dtm_validate_listening_ip_addr(DTM_INTERNODE_CB *config);
extern uint32_t dtm_stream_nonblocking_listener(DTM_INTERNODE_CB *dtms_cb);
extern uint32_t dtm_sockdesc_close(int sock_desc);
extern DTM_NODE_DB *dtm_process_connect(DTM_INTERNODE_CB *dtms_cb,
                                        uint8_t *buffer, uint16_t len);
extern DTM_NODE_DB *dtm_process_accept(DTM_INTERNODE_CB *dtms_cb,
                                       int stream_sock);
extern uint32_t dtm_comm_socket_send(int sock_desc, const void *buffer,
                                     int buffer_len);
extern void dtm_comm_socket_close(DTM_NODE_DB *node);
extern uint32_t dtm_process_node_up_down(NODE_ID node_id, char *node_name,
                                         char *node_ip,
                                         sa_family_t i_addr_family,
                                         bool comm_status);
extern void dtm_internode_set_pollout(DTM_NODE_DB *node);
extern void dtm_internode_clear_pollout(DTM_NODE_DB *node);

#endif  // DTM_DTMND_DTM_NODE_H_
