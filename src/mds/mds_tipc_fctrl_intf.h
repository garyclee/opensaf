/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2019 The OpenSAF Foundation
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

#ifndef MDS_MDS_TIPC_FCTRL_INTF_H_
#define MDS_MDS_TIPC_FCTRL_INTF_H_

#include <linux/tipc.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t mds_tipc_fctrl_initialize(int dgramsock, struct tipc_portid id,
    uint64_t rcv_buf_size, int32_t ackto,
    int32_t acksize, bool mbrcast_enabled);
uint32_t mds_tipc_fctrl_shutdown(void);
uint32_t mds_tipc_fctrl_rcv_data(uint8_t *buffer, uint16_t len,
    struct tipc_portid id);
uint32_t mds_tipc_fctrl_portid_up(struct tipc_portid id, uint32_t type);
uint32_t mds_tipc_fctrl_portid_down(struct tipc_portid id, uint32_t type);
uint32_t mds_tipc_fctrl_portid_terminate(struct tipc_portid id);
uint32_t mds_tipc_fctrl_drop_data(uint8_t *buffer, uint16_t len,
    struct tipc_portid id);
uint32_t mds_tipc_fctrl_sndqueue_capable(struct tipc_portid id,
    uint16_t* next_seq);
uint32_t mds_tipc_fctrl_trysend(const uint8_t *buffer, uint16_t len,
    struct tipc_portid id);
#ifdef __cplusplus
}
#endif

#endif  // MDS_MDS_TIPC_FCTRL_INTF_H_
