/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
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
 * Author(s): Emerson Network Power
 *
 */

#ifndef FM_FMD_FM_CB_H_
#define FM_FMD_FM_CB_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <atomic>
#include <string>
#include <saAmf.h>
#include <saClm.h>
#include "base/mutex.h"
#include "base/ncssysf_ipc.h"
#include "base/ncssysf_tmr.h"
#include "fm/fmd/fm_amf.h"
#include "mds/mds_papi.h"
#include "rde/agent/rda_papi.h"


extern uint32_t gl_fm_hdl;

typedef enum {
  FM_TMR_TYPE_MIN,
  FM_TMR_PROMOTE_ACTIVE,
  FM_TMR_ACTIVATION_SUPERVISION,
  FM_TMR_TYPE_MAX
} FM_TMR_TYPE;

typedef enum {
  FM_TMR_STATUS_MIN,
  FM_TMR_RUNNING,
  FM_TMR_STOPPED,
  FM_TMR_STATUS_MAX
} FM_TMR_STATUS;

typedef struct fm_tmr {
  tmr_t tmr_id;
  FM_TMR_TYPE type;
  FM_TMR_STATUS status;
} FM_TMR;

/* FM control block */
struct FM_CB {
  uint32_t cb_hdl{};
  SYSF_MBX mbx{};

  /* FM AMF CB */
  FM_AMF_CB fm_amf_cb{};
  std::atomic<NODE_ID> node_id{};
  SaNameT node_name{};

  std::string peer_node_name{};
  std::atomic<NODE_ID> peer_node_id{};
  std::atomic<MDS_DEST> peer_adest{}; /* will be 0 if peer is not up */

  /* Holds own role. */
  PCS_RDA_ROLE role{};

  /* AMF HA state for FM */
  SaAmfHAStateT amf_state{};

  /* MDS handles. */
  MDS_DEST adest{};
  MDS_HDL adest_hdl{};
  MDS_HDL adest_pwe1_hdl{};

  /* Timers */
  FM_TMR promote_active_tmr{};
  FM_TMR activation_supervision_tmr{};

  /* Time in terms of one hundredth of seconds (500 for 5 secs.) */
  uint32_t active_promote_tmr_val{};
  uint32_t activation_supervision_tmr_val{};
  bool fully_initialized{false};
  bool csi_assigned{false};
  /* Variable to indicate OpenSAF control of TIPC transport */
  bool control_tipc{true};
  /* Booleans to mark service down events of critical Osaf Services */
  bool immd_down{true};
  bool immnd_down{true};
  std::atomic<bool> amfnd_down{true};
  bool amfd_down{true};
  bool fm_down{false};

  std::atomic<bool> peer_sc_up{false};
  bool well_connected{false};
  std::atomic<uint64_t> cluster_size{};
  struct timespec last_well_connected{};
  struct timespec node_isolation_timeout{};
  SaClmHandleT clm_hdl{};
  bool use_remote_fencing{false};
  SaNameT peer_clm_node_name{};
  std::atomic<bool> peer_node_terminated{false};

  base::Mutex mutex_{};

  std::string config_file;
};

extern const char *role_string[];
extern FM_CB *fm_cb;

/*****************************************************************
 *         Prototypes for extern functions                       *
 *****************************************************************/
uint32_t fm_rda_init(FM_CB *);
uint32_t fm_rda_set_role(FM_CB *, PCS_RDA_ROLE);
#endif  // FM_FMD_FM_CB_H_
