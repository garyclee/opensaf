/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2017 - All Rights Reserved.
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

#ifndef SRC_LOG_AGENT_LGA_MDS_H_
#define SRC_LOG_AGENT_LGA_MDS_H_

#include <stdint.h>
#include <saAis.h>

struct lga_cb_t;
struct lgsv_msg_t;

uint32_t lga_mds_init();
uint32_t lga_mds_deinit();
void lga_msg_destroy(lgsv_msg_t *msg);

uint32_t lga_mds_msg_sync_send(lgsv_msg_t *i_msg, lgsv_msg_t **o_msg,
                               SaTimeT timeout, uint32_t prio);
uint32_t lga_mds_msg_async_send(lgsv_msg_t *i_msg, uint32_t prio);

bool lga_mds_msg_does_support_init_clm_status();

#endif  // SRC_LOG_AGENT_LGA_MDS_H_
