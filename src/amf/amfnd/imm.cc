/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2016 The OpenSAF Foundation
 * Copyright (C) 2017, Oracle and/or its affiliates. All rights reserved.
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
 * Author(s): Oracle
 *
 */

#include <thread>
#include <atomic>
#include <string>
#include "amf/amfnd/avnd.h"
#include <poll.h>
#include "osaf/immutil/immutil.h"
#include "amf/amfnd/imm.h"

ir_cb_t ir_cb;
struct pollfd ImmReader::fds[];
extern const AVND_EVT_HDLR g_avnd_func_list[AVND_EVT_MAX];
std::atomic<bool> imm_reader_thread_ready{false};

/**
 * This thread will read classes and object information thereby allowing main
 * thread to proceed with other important work.
 * As of now, SU instantiation needs imm data to be read, so
 * only this event is being handled here.
 * Going forward, we need to use this function for similar purposes.
 * @param _cb
 *
 * @return void*
 */
void ImmReader::imm_reader_thread() {
  AVND_EVT *evt;
  TRACE_ENTER();
  NCS_SEL_OBJ mbx_fd;

  /* create the mail box */
  uint32_t rc = m_NCS_IPC_CREATE(&ir_cb.mbx);
  if (NCSCC_RC_SUCCESS != rc) {
    LOG_CR("AvND Mailbox creation failed");
    exit(EXIT_FAILURE);
  }
  TRACE("Ir mailbox creation success");

  /* attach the mail box */
  rc = m_NCS_IPC_ATTACH(&ir_cb.mbx);
  if (NCSCC_RC_SUCCESS != rc) {
    LOG_CR("Ir mailbox attach failed");
    exit(EXIT_FAILURE);
  }
  imm_reader_thread_ready = true;

  mbx_fd = ncs_ipc_get_sel_obj(&ir_cb.mbx);
  fds[FD_MBX].fd = mbx_fd.rmv_obj;
  fds[FD_MBX].events = POLLIN;

  while (1) {
    if (poll(fds, irfds, -1) == (-1)) {
      if (errno == EINTR) {
        continue;
      }

      LOG_ER("poll Fail - %s", strerror(errno));
      exit(EXIT_FAILURE);
    }

    if (fds[FD_MBX].revents & POLLIN) {
      while (nullptr != (evt = (AVND_EVT *)ncs_ipc_non_blk_recv(&ir_cb.mbx)))
        ir_process_event(evt);
    }
  }
  TRACE_LEAVE();
}

/**
 * Start a imm reader thread.
 * @param Nothing.
 * @return Nothing.
 */
void ImmReader::imm_reader_thread_create() {
  TRACE_ENTER();

  std::thread t{imm_reader_thread};
  t.detach();

  TRACE_LEAVE();
}

/**
 * This function process events, which requires reading data from imm.
 * As of now, SU instantiation needs to read data, so diverting SU
 * instantiation in this thread. Going forward, we need to use this
 * function for similar purposes.
 * @param Event pointer.
 * @return Nothing.
 */
void ImmReader::ir_process_event(AVND_EVT *evt) {
  uint32_t res = NCSCC_RC_SUCCESS;
  AVND_SU *su = 0;
  TRACE_ENTER2("Evt type:%u", evt->type);
  if (AVND_EVT_IR == evt->type) {
    /* get the su */
    su = avnd_sudb_rec_get(avnd_cb->sudb,
      Amf::to_string(&evt->info.ir_evt.su_name));
    if (!su) {
      TRACE("SU'%s', not found in DB",
        osaf_extended_name_borrow(&evt->info.ir_evt.su_name));
      goto done;
    }

    if (false == su->is_ncs) {
      res = avnd_comp_config_get_su(su);
    } else {
      res = NCSCC_RC_FAILURE;
    }

    {
      AVND_EVT *evt_ir = 0;
      SaNameT copy_name;
      TRACE("Sending to main thread.");
      osaf_extended_name_alloc(su->name.c_str(), &copy_name);
      evt_ir =
        avnd_evt_create(avnd_cb, AVND_EVT_IR, 0, nullptr, &copy_name, 0, 0);
      osaf_extended_name_free(&copy_name);
      if (res == NCSCC_RC_SUCCESS)
        evt_ir->info.ir_evt.status = true;
      else
        evt_ir->info.ir_evt.status = false;

      res = m_NCS_IPC_SEND(&avnd_cb->mbx, evt_ir, evt_ir->priority);

      if (NCSCC_RC_SUCCESS != res)
        LOG_CR("AvND send event to main thread mailbox failed, type = %u",
          evt_ir->type);

      TRACE("Imm Read:'%d'", evt_ir->info.ir_evt.status);

      /* if failure, free the event */
      if (NCSCC_RC_SUCCESS != res) avnd_evt_destroy(evt_ir);
    }
  } else if (AVND_EVT_AVA_ERR_REP == evt->type) {
    AVSV_AMF_API_INFO *api_info = &evt->info.ava.msg->info.api_info;
    AVSV_AMF_ERR_REP_PARAM *err_rep = &api_info->param.err_rep;
    SaAisErrorT amf_rc = SA_AIS_OK;
    AVND_COMP *comp = 0;
    SaImmHandleT immOmHandle = 0;
    SaVersionT immVersion = {'A', 2, 15};
    SaImmAccessorHandleT accessorHandle = 0;
    const SaImmAttrValuesT_2 **attributes;
    std::string su_name;
    SaNameT node_name, clm_node_name;
    SaClmNodeIdT node_id;
    SaAisErrorT error;
    // Check if this component is already in the internode db.
    // If it is, then it means that Error Report operation
    // is already undergoing on this comp, so return TRY_AGAIN.
    if (nullptr !=
        avnd_cb->compdb_internode.find(Amf::to_string(&err_rep->err_comp))) {
      LOG_NO("Already Error Report Operation undergoing on '%s'",
        Amf::to_string(&err_rep->err_comp).c_str());
      amf_rc = SA_AIS_ERR_TRY_AGAIN;
      goto imm_not_initialized;
    }
    // Check if component is configured in the Cluster.
    error = saImmOmInitialize_cond(&immOmHandle, nullptr, &immVersion);
    if (error != SA_AIS_OK) {
      LOG_ER("saImmOmInitialize failed: %u", error);
      amf_rc = SA_AIS_ERR_TRY_AGAIN;
      goto imm_not_initialized;
    }

    error = amf_saImmOmAccessorInitialize(immOmHandle, accessorHandle);
    if (error != SA_AIS_OK) {
      LOG_ER("amf_saImmOmAccessorInitialize failed: %u", error);
      amf_rc = SA_AIS_ERR_TRY_AGAIN;
      goto immOmHandle_finalize;
    }

    // Find the component in the Imm DB.
    if ((error = amf_saImmOmAccessorGet_o2(
      immOmHandle, accessorHandle, Amf::to_string(&err_rep->err_comp), nullptr,
      reinterpret_cast<SaImmAttrValuesT_2 ***>
      (const_cast<SaImmAttrValuesT_2 ***>(&attributes)))) != SA_AIS_OK) {
      if (error == SA_AIS_ERR_NOT_EXIST) {
        LOG_NO("Component '%s' is not configured",
          Amf::to_string(&err_rep->err_comp).c_str());
        amf_rc = SA_AIS_ERR_NOT_EXIST;
      } else {
        LOG_ER("amf_saImmOmAccessorGet_o2(for Comp '%s') failed: %u",
          Amf::to_string(&err_rep->err_comp).c_str(), error);
        amf_rc = SA_AIS_ERR_TRY_AGAIN;
      }

      goto immOmAccessor_finalize;
    }

    // Component is configured. Now, extract SU name from comp name.
    avnd_cpy_SU_DN_from_DN(su_name, Amf::to_string(&err_rep->err_comp));
    TRACE("SU name '%s'", su_name.c_str());

    // Get the SU attributes from Imm DB.
    if ((error = amf_saImmOmAccessorGet_o2(
      immOmHandle, accessorHandle, su_name, nullptr,
      reinterpret_cast<SaImmAttrValuesT_2 ***>
      (const_cast<SaImmAttrValuesT_2 ***>(&attributes)))) != SA_AIS_OK) {
      if (error == SA_AIS_ERR_NOT_EXIST) {
        LOG_NO("SU '%s' is not configured", su_name.c_str());
        amf_rc = SA_AIS_ERR_NOT_EXIST;
      } else {
        LOG_ER("amf_saImmOmAccessorGet_o2(for SU '%s') failed: %u",
          su_name.c_str(), error);
        amf_rc = SA_AIS_ERR_TRY_AGAIN;
      }

      goto immOmAccessor_finalize;
    }

    // Get Amf node name, where this SU is hosted.
    (void)immutil_getAttr(const_cast<SaImmAttrNameT>("saAmfSUHostedByNode"),
                          attributes, 0, &node_name);
    TRACE("Hosting Amf node '%s'", Amf::to_string(&node_name).c_str());

    // Get Amf node attributes from Imm DB.
    if ((error = amf_saImmOmAccessorGet_o2(
      immOmHandle, accessorHandle, Amf::to_string(&node_name), nullptr,
      reinterpret_cast<SaImmAttrValuesT_2 ***>
      (const_cast<SaImmAttrValuesT_2 ***>(&attributes)))) != SA_AIS_OK) {
      if (error == SA_AIS_ERR_NOT_EXIST) {
        LOG_NO("Amf Node '%s' is not configured",
               Amf::to_string(&node_name).c_str());
        amf_rc = SA_AIS_ERR_NOT_EXIST;
      } else {
        LOG_ER("amf_saImmOmAccessorGet_o2(for node '%s') failed: %u",
          Amf::to_string(&node_name).c_str(), error);
        amf_rc = SA_AIS_ERR_TRY_AGAIN;
      }

      goto immOmAccessor_finalize;
    }
    TRACE("Hosting Amf node '%s' found.", Amf::to_string(&node_name).c_str());

    // Find the Clm Node name from Amf node attributes.
    (void)immutil_getAttr(const_cast<SaImmAttrNameT>("saAmfNodeClmNode"),
                           attributes, 0, &clm_node_name);
    TRACE("Clm node name '%s'", Amf::to_string(&clm_node_name).c_str());

    // Get Clm node attributes from Imm DB.
    if ((error = amf_saImmOmAccessorGet_o2(
        immOmHandle, accessorHandle, Amf::to_string(&clm_node_name),
        nullptr, reinterpret_cast<SaImmAttrValuesT_2 ***>
        (const_cast<SaImmAttrValuesT_2 ***>(&attributes)))) != SA_AIS_OK) {
      if (error == SA_AIS_ERR_NOT_EXIST) {
        LOG_NO("Clm Node '%s' is not configured",
          Amf::to_string(&clm_node_name).c_str());
        amf_rc = SA_AIS_ERR_NOT_EXIST;
      } else {
        LOG_ER("amf_saImmOmAccessorGet_o2(for clm node '%s') failed: %u",
          Amf::to_string(&clm_node_name).c_str(), error);
        amf_rc = SA_AIS_ERR_TRY_AGAIN;
      }

      goto immOmAccessor_finalize;
    }
    TRACE("Clm node '%s' found.", Amf::to_string(&clm_node_name).c_str());

    // Find the configured value of saClmNodeID from Clm node attributes.
    (void)immutil_getAttr(const_cast<SaImmAttrNameT>("saClmNodeID"),
                           attributes, 0, &node_id);
    TRACE("Node id '%u'", node_id);

    // We have all the information, send the event to the destined node.
    res = avnd_avnd_msg_send(avnd_cb, reinterpret_cast<uint8_t *>(api_info),
                             api_info->type, &evt->mds_ctxt, node_id);
    if (NCSCC_RC_SUCCESS != res) {
      // We couldn't send this to the destined AvND, tell user to try again.
      amf_rc = SA_AIS_ERR_UNAVAILABLE;
      LOG_ER("avnd_int_ext_comp_hdlr:Msg Send Failed: '%u'", res);
      goto immOmAccessor_finalize;
    } else {
      TRACE("avnd_avnd_msg_send:Msg Send Success");
      // Add the internode component in the DB
      comp = new AVND_COMP();
      comp->name = Amf::to_string(&err_rep->err_comp);
      comp->node_id = node_id;
      comp->reg_dest = api_info->dest;
      comp->mds_ctxt = evt->mds_ctxt;
      comp->comp_type = AVND_COMP_TYPE_INTER_NODE_NP;
      // Add to the internode available compdb.
      res = avnd_cb->compdb_internode.insert(comp->name, comp);
      if (NCSCC_RC_SUCCESS != res) {
        delete comp;
        comp = nullptr;
        amf_rc = SA_AIS_ERR_TRY_AGAIN;
        LOG_ER("compdb_internode.insert failed: '%u'", res);
        goto immOmAccessor_finalize;
      }
      immutil_saImmOmAccessorFinalize(accessorHandle);
      immutil_saImmOmFinalize(immOmHandle);
      goto done;
    }
immOmAccessor_finalize:
    immutil_saImmOmAccessorFinalize(accessorHandle);
immOmHandle_finalize:
    immutil_saImmOmFinalize(immOmHandle);
imm_not_initialized:
    /* send the response back to AvA */
    res = avnd_amf_resp_send(avnd_cb, AVSV_AMF_ERR_REP, amf_rc, 0,
                             &api_info->dest, &evt->mds_ctxt, comp, 0);
    if (NCSCC_RC_SUCCESS != res) {
      LOG_ER("avnd_amf_resp_send() failed: %u", res);
    }
  }
done:
  if (evt) avnd_evt_destroy(evt);

  TRACE_LEAVE2("res '%u'", res);
}
