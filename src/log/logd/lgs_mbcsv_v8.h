/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2019 - All Rights Reserved.
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

#ifndef LOG_LOGD_LGS_MBCSV_V8_H_
#define LOG_LOGD_LGS_MBCSV_V8_H_

#include "log/logd/lgs_mbcsv_v2.h"
#include "log/logd/lgs_mbcsv_v3.h"
#include "log/logd/lgs_mbcsv_v5.h"
#include "log/logd/lgs_mbcsv_v6.h"

#include "base/ncs_edu_pub.h"
#include "base/ncsencdec_pub.h"

struct CkptPushAsync {
  SaInvocationT invocation;
  uint32_t ack_flags;
  uint32_t client_id;
  uint32_t stream_id;
  char* svc_name;
  SaTimeT log_stamp;
  SaLogSeverityT severity;
  MDS_DEST dest;
  char* from_node;

  uint64_t queue_at;
  uint64_t seq_id;
  char* log_record;
};

struct CkptPopAsync {
  uint64_t seq_id;
};

struct CkptPopAndWriteAsync {
  uint32_t stream_id;
  uint32_t record_id;
  uint32_t file_size;
  char* log_file;
  char* log_record;
  uint64_t timestamp;
  uint64_t seq_id;
};

struct lgsv_ckpt_msg_v8_t {
  lgsv_ckpt_header_t header;
  union {
    lgs_ckpt_initialize_msg_v6_t initialize_client;
    lgs_ckpt_finalize_msg_v2_t finalize_client;
    lgs_ckpt_write_log_v2_t write_log;
    lgs_ckpt_agent_down_v2_t agent_down;
    lgs_ckpt_stream_open_t stream_open;
    lgs_ckpt_stream_close_v2_t stream_close;
    lgs_ckpt_stream_cfg_v3_t stream_cfg;
    lgs_ckpt_lgs_cfg_v5_t lgs_cfg;
    CkptPushAsync push_async;
    CkptPopAsync pop_async;
    CkptPopAndWriteAsync pop_and_write_async;
  } ckpt_rec;
};

uint32_t edp_ed_ckpt_msg_v8(EDU_HDL *edu_hdl, EDU_TKN *edu_tkn, NCSCONTEXT ptr,
                            uint32_t *ptr_data_len, EDU_BUF_ENV *buf_env,
                            EDP_OP_TYPE op, EDU_ERR *o_err);

uint32_t EncodeDecodePushAsync(EDU_HDL* edu_hdl, EDU_TKN* edu_tkn,
                               NCSCONTEXT ptr, uint32_t* ptr_data_len,
                               EDU_BUF_ENV* buf_env, EDP_OP_TYPE op,
                               EDU_ERR* o_err);
uint32_t EncodeDecodePopAsync(EDU_HDL* edu_hdl, EDU_TKN* edu_tkn,
                              NCSCONTEXT ptr, uint32_t* ptr_data_len,
                              EDU_BUF_ENV* buf_env, EDP_OP_TYPE op,
                              EDU_ERR* o_err);
uint32_t EncodeDecodePopAndWriteAsync(EDU_HDL* edu_hdl, EDU_TKN* edu_tkn,
                                      NCSCONTEXT ptr, uint32_t* ptr_data_len,
                                      EDU_BUF_ENV* buf_env, EDP_OP_TYPE op,
                                      EDU_ERR* o_err);
uint32_t DecodePushAsync(lgs_cb_t* cb, void* ckpt_msg,
                         NCS_MBCSV_CB_ARG* cbk_arg);
uint32_t DecodePopAsync(lgs_cb_t* cb, void* ckpt_msg,
                        NCS_MBCSV_CB_ARG* cbk_arg);
uint32_t DecodePopAndWriteAsync(lgs_cb_t* cb, void* ckpt_msg,
                                NCS_MBCSV_CB_ARG* cbk_arg);

uint32_t ckpt_proc_push_async(lgs_cb_t* cb, void* data);
uint32_t ckpt_proc_pop_async(lgs_cb_t* cb, void* data);
uint32_t ckpt_proc_pop_write_async(lgs_cb_t* cb, void* data);

void Dump(const CkptPushAsync* data);


#ifdef SIMULATE_NFS_UNRESPONSE
extern uint32_t test_counter;
#endif

#endif  // LOG_LOGD_LGS_MBCSV_V8_H_
