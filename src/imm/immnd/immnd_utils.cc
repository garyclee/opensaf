/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2018 - All Rights Reserved.
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
 */

#include "imm/immnd/immnd_utils.h"

#include <cstdio>
#include <string>
#include <vector>

#include "imm/common/immsv_evt.h"
#include "base/saf_error.h"
#include "base/osaf_extended_name.h"

#define MAX_NUMBER_FEVS_MSG 5    // Number of printed last FEVS when IMMND down
#define MAX_LENGTH_FEVS_MSG 256  // Max length of printed FEVS info

static std::vector<std::string> latest_fevs(MAX_NUMBER_FEVS_MSG, "(none)");

void SyslogRecentFevs() {
  LOG_NO("Recent fevs:");
  for (const auto &i : latest_fevs) {
    LOG_NO("%s", i.c_str());
  }
}

void CollectRecentFevsLogData(const IMMND_EVT *evt, SaUint64T msg_no) {
  char evt_data[MAX_LENGTH_FEVS_MSG];
  char evt_info[MAX_LENGTH_FEVS_MSG] = "(none)";
  const char* evt_name = immsv_get_immnd_evt_name(evt->type);

  switch (evt->type) {
    case IMMND_EVT_A2ND_OBJ_CREATE:
    case IMMND_EVT_A2ND_OBJ_CREATE_2:
    case IMMND_EVT_A2ND_OI_OBJ_CREATE:
    case IMMND_EVT_A2ND_OI_OBJ_CREATE_2:
      snprintf(evt_info, sizeof(evt_info), "%s",
          evt->info.objCreate.className.buf);
      break;

    case IMMND_EVT_A2ND_OBJ_MODIFY:
    case IMMND_EVT_A2ND_OI_OBJ_MODIFY:
      snprintf(evt_info, sizeof(evt_info), "%s",
          evt->info.objModify.objectName.buf);
      break;

    case IMMND_EVT_A2ND_OBJ_DELETE:
    case IMMND_EVT_A2ND_OI_OBJ_DELETE:
      snprintf(evt_info, sizeof(evt_info), "%s",
          evt->info.objDelete.objectName.buf);
      break;

    case IMMND_EVT_A2ND_OBJ_SYNC:
    case IMMND_EVT_A2ND_OBJ_SYNC_2:
      snprintf(evt_info, sizeof(evt_info), "%s",
          evt->info.obj_sync.objectName.buf);
      break;

    case IMMND_EVT_A2ND_IMM_ADMOP:
    case IMMND_EVT_A2ND_IMM_ADMOP_ASYNC:
      snprintf(evt_info, sizeof(evt_info), "%s(op_id:%llu)",
          evt->info.admOpReq.objectName.buf,
          evt->info.admOpReq.operationId);
      break;

    case IMMND_EVT_A2ND_CLASS_CREATE:
    case IMMND_EVT_A2ND_CLASS_DELETE:
      snprintf(evt_info, sizeof(evt_info), "%s",
          evt->info.classDescr.className.buf);
      break;

    case IMMND_EVT_D2ND_DISCARD_IMPL:
    case IMMND_EVT_A2ND_OI_IMPL_CLR:
    case IMMND_EVT_D2ND_IMPLSET_RSP:
    case IMMND_EVT_D2ND_IMPLSET_RSP_2:
      snprintf(evt_info, sizeof(evt_info), "%s(%u)",
          evt->info.implSet.impl_name.buf,
          evt->info.implSet.impl_id);
      break;

    case IMMND_EVT_D2ND_ADMINIT:
      snprintf(evt_info, sizeof(evt_info), "%s(%d)",
          osaf_extended_name_borrow(
              &(evt->info.adminitGlobal.i.adminOwnerName)),
          evt->info.adminitGlobal.globalOwnerId);
      break;

    case IMMND_EVT_D2ND_CCBINIT:
      snprintf(evt_info, sizeof(evt_info), "ccb_id:%u",
          evt->info.ccbinitGlobal.globalCcbId);
      break;

    case IMMND_EVT_A2ND_AUG_ADMO:
      snprintf(evt_info, sizeof(evt_info), "Add admo_id:%u to ccb_id:%u",
          evt->info.objDelete.adminOwnerId, evt->info.objDelete.ccbId);
      break;

    case IMMND_EVT_A2ND_OI_CL_IMPL_SET:
    case IMMND_EVT_A2ND_OI_OBJ_IMPL_SET:
      snprintf(evt_info, sizeof(evt_info), "Set impl_id:%u to %s",
          evt->info.implSet.impl_id,
          evt->info.implSet.impl_name.buf);
      break;

    case IMMND_EVT_A2ND_OI_CL_IMPL_REL:
    case IMMND_EVT_A2ND_OI_OBJ_IMPL_REL:
      snprintf(evt_info, sizeof(evt_info), "Release impl_id:%u out of %s",
          evt->info.implSet.impl_id,
          evt->info.implSet.impl_name.buf);
      break;

    case IMMND_EVT_A2ND_ADMO_FINALIZE:
    case IMMND_EVT_D2ND_ADMO_HARD_FINALIZE:
    case IMMND_EVT_A2ND_ADMO_SET:
    case IMMND_EVT_A2ND_ADMO_RELEASE:
    case IMMND_EVT_A2ND_ADMO_CLEAR:
      snprintf(evt_info, sizeof(evt_info), "admo_id:%u",
          evt->info.admFinReq.adm_owner_id);
      break;

    case IMMND_EVT_A2ND_OI_CCB_AUG_INIT:
      snprintf(evt_info, sizeof(evt_info), "ccb_id:%u",
          evt->info.ccbUpcallRsp.ccbId);
      break;

    case IMMND_EVT_A2ND_CCB_OBJ_CREATE_RSP:
    case IMMND_EVT_A2ND_CCB_OBJ_CREATE_RSP_2:
    case IMMND_EVT_A2ND_CCB_OBJ_MODIFY_RSP:
    case IMMND_EVT_A2ND_CCB_OBJ_MODIFY_RSP_2:
    case IMMND_EVT_A2ND_CCB_OBJ_DELETE_RSP:
    case IMMND_EVT_A2ND_CCB_OBJ_DELETE_RSP_2:
    case IMMND_EVT_A2ND_CCB_COMPLETED_RSP:
    case IMMND_EVT_A2ND_CCB_COMPLETED_RSP_2:
      snprintf(evt_info, sizeof(evt_info), "ccb_id:%u, rc:%s",
          evt->info.ccbUpcallRsp.ccbId,
          saf_error(evt->info.ccbUpcallRsp.result));
      break;

    case IMMND_EVT_A2ND_CCB_APPLY:
    case IMMND_EVT_A2ND_CCB_VALIDATE:
    case IMMND_EVT_A2ND_CCB_FINALIZE:
    case IMMND_EVT_D2ND_ABORT_CCB:
      snprintf(evt_info, sizeof(evt_info), "ccb_id:%u",
          evt->info.ccbId);
      break;

    case IMMND_EVT_A2ND_PBE_ADMOP_RSP:
      snprintf(evt_info, sizeof(evt_info), "rc:%s",
          saf_error(evt->info.admOpRsp.result));
      break;

    case IMMND_EVT_A2ND_PBE_PRT_OBJ_CREATE_RSP:
    case IMMND_EVT_A2ND_PBE_PRTO_DELETES_COMPLETED_RSP:
    case IMMND_EVT_A2ND_PBE_PRT_ATTR_UPDATE_RSP:
      snprintf(evt_info, sizeof(evt_info), "rc:%s",
          saf_error(evt->info.ccbUpcallRsp.result));
      break;

    case IMMND_EVT_D2ND_SYNC_FEVS_BASE:
      snprintf(evt_info, sizeof(evt_info), "sync fevs base: %llu",
          evt->info.syncFevsBase);
      break;

    case IMMND_EVT_A2ND_OBJ_SAFE_READ:
      snprintf(evt_info, sizeof(evt_info), "ccb_id:%u locks %s",
          evt->info.searchInit.ccbId,
          evt->info.searchInit.rootName.buf);
      break;

    case IMMND_EVT_D2ND_IMPLDELETE:
      snprintf(evt_info, sizeof(evt_info), "Delete %u implementer(s)",
          evt->info.impl_delete.size);
      break;

    case IMMND_EVT_D2ND_DISCARD_NODE:
      snprintf(evt_info, sizeof(evt_info), "node_id:%x",
          evt->info.ctrl.nodeId);
      break;

    default:
      break;
  }

  snprintf(evt_data, MAX_LENGTH_FEVS_MSG,
      "<%llu>[%s -> %s]", msg_no, evt_name, evt_info);

  static int index = 0;
  latest_fevs[index] = std::string(evt_data);
  if (++index > MAX_NUMBER_FEVS_MSG - 1) index = 0;
}
