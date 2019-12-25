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

#include "log/logd/lgs_mbcsv_cache.h"
#include "log/logd/lgs_cache.h"

uint32_t EncodeDecodePushAsync(EDU_HDL* edu_hdl, EDU_TKN* edu_tkn,
                               NCSCONTEXT ptr, uint32_t* ptr_data_len,
                               EDU_BUF_ENV* buf_env, EDP_OP_TYPE op,
                               EDU_ERR* o_err) {
  TRACE_ENTER();
  CkptPushAsync* ckpt_push_async = nullptr;
  CkptPushAsync** ckpt_push_async_dec_ptr;
  EDU_INST_SET ckpt_push_async_rec_ed_rules[] = {
    {EDU_START, EncodeDecodePushAsync, 0, 0, 0,
     sizeof(CkptPushAsync), 0, nullptr},

    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->invocation, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->ack_flags, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->client_id, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->stream_id, 0, nullptr},
    {EDU_EXEC, ncs_edp_string, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->svc_name, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->log_stamp, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns16, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->severity, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->dest, 0, nullptr},
    {EDU_EXEC, ncs_edp_string, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->from_node, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->queue_at, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->seq_id, 0, nullptr},
    {EDU_EXEC, ncs_edp_string, 0, 0, 0,
     (long)&((CkptPushAsync*)0)->log_record, 0, nullptr},

    {EDU_END, 0, 0, 0, 0, 0, 0, nullptr},
  };

  if (op == EDP_OP_TYPE_ENC) {
    ckpt_push_async = static_cast<CkptPushAsync*>(ptr);
  } else if (op == EDP_OP_TYPE_DEC) {
    ckpt_push_async_dec_ptr = static_cast<CkptPushAsync**>(ptr);
    if (*ckpt_push_async_dec_ptr == nullptr) {
      *o_err = EDU_ERR_MEM_FAIL;
      return NCSCC_RC_FAILURE;
    }
    memset(*ckpt_push_async_dec_ptr, 0, sizeof(CkptPushAsync));
    ckpt_push_async = *ckpt_push_async_dec_ptr;
  } else {
    ckpt_push_async = static_cast<CkptPushAsync*>(ptr);
  }

  uint32_t rc = m_NCS_EDU_RUN_RULES(edu_hdl, edu_tkn,
                             ckpt_push_async_rec_ed_rules,
                             ckpt_push_async, ptr_data_len, buf_env,
                             op, o_err);
  return rc;
}

uint32_t EncodeDecodePopAsync(EDU_HDL* edu_hdl, EDU_TKN* edu_tkn,
                              NCSCONTEXT ptr, uint32_t* ptr_data_len,
                              EDU_BUF_ENV* buf_env, EDP_OP_TYPE op,
                              EDU_ERR* o_err) {
  TRACE_ENTER();
  CkptPopAsync* ckpt_pop_async = nullptr, **ckpt_pop_async_dec_ptr;
  EDU_INST_SET ckpt_pop_data_rec_ed_rules[] = {
    {EDU_START, EncodeDecodePopAsync, 0, 0, 0,
     sizeof(CkptPopAsync), 0, nullptr},
    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPopAsync*)0)->seq_id, 0, nullptr},
    {EDU_END, 0, 0, 0, 0, 0, 0, nullptr},
  };

  if (op == EDP_OP_TYPE_ENC) {
    ckpt_pop_async = static_cast<CkptPopAsync*>(ptr);
  } else if (op == EDP_OP_TYPE_DEC) {
    ckpt_pop_async_dec_ptr = static_cast<CkptPopAsync**>(ptr);
    if (*ckpt_pop_async_dec_ptr == nullptr) {
      *o_err = EDU_ERR_MEM_FAIL;
      return NCSCC_RC_FAILURE;
    }
    memset(*ckpt_pop_async_dec_ptr, 0, sizeof(CkptPopAsync));
    ckpt_pop_async = *ckpt_pop_async_dec_ptr;
  } else {
    ckpt_pop_async = static_cast<CkptPopAsync*>(ptr);
  }

  return m_NCS_EDU_RUN_RULES(edu_hdl, edu_tkn, ckpt_pop_data_rec_ed_rules,
                             ckpt_pop_async, ptr_data_len, buf_env,
                             op, o_err);
}

uint32_t EncodeDecodePopAndWriteAsync(EDU_HDL* edu_hdl, EDU_TKN* edu_tkn,
                                      NCSCONTEXT ptr, uint32_t* ptr_data_len,
                                      EDU_BUF_ENV* buf_env, EDP_OP_TYPE op,
                                      EDU_ERR* o_err) {
  TRACE_ENTER();
  CkptPopAndWriteAsync* ckpt_pop_and_write_async = nullptr;
  CkptPopAndWriteAsync** ckpt_pop_and_write_async_dec_ptr;
  EDU_INST_SET ckpt_pop_and_write_async_rec_ed_rules[] = {
    {EDU_START, EncodeDecodePopAndWriteAsync, 0, 0, 0,
     sizeof(CkptPopAndWriteAsync), 0, nullptr},
    {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
     (long)&((CkptPopAndWriteAsync*)0)->stream_id, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
     (long)&((CkptPopAndWriteAsync*)0)->record_id, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
     (long)&((CkptPopAndWriteAsync*)0)->file_size, 0, nullptr},
    {EDU_EXEC, ncs_edp_string, 0, 0, 0,
     (long)&((CkptPopAndWriteAsync*)0)->log_file, 0, nullptr},
    {EDU_EXEC, ncs_edp_string, 0, 0, 0,
     (long)&((CkptPopAndWriteAsync*)0)->log_record, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPopAndWriteAsync*)0)->timestamp, 0, nullptr},
    {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
     (long)&((CkptPopAndWriteAsync*)0)->seq_id, 0, nullptr},
    {EDU_END, 0, 0, 0, 0, 0, 0, nullptr},
  };

  if (op == EDP_OP_TYPE_ENC) {
    ckpt_pop_and_write_async = static_cast<CkptPopAndWriteAsync*>(ptr);
  } else if (op == EDP_OP_TYPE_DEC) {
    ckpt_pop_and_write_async_dec_ptr = static_cast<CkptPopAndWriteAsync**>(ptr);
    if (*ckpt_pop_and_write_async_dec_ptr == nullptr) {
      *o_err = EDU_ERR_MEM_FAIL;
      return NCSCC_RC_FAILURE;
    }
    memset(*ckpt_pop_and_write_async_dec_ptr, 0, sizeof(CkptPopAndWriteAsync));
    ckpt_pop_and_write_async = *ckpt_pop_and_write_async_dec_ptr;
  } else {
    ckpt_pop_and_write_async = static_cast<CkptPopAndWriteAsync*>(ptr);
  }

  return m_NCS_EDU_RUN_RULES(edu_hdl, edu_tkn,
                             ckpt_pop_and_write_async_rec_ed_rules,
                             ckpt_pop_and_write_async, ptr_data_len, buf_env,
                             op, o_err);
}

uint32_t DecodePushAsync(lgs_cb_t* cb, void* ckpt_msg,
                         NCS_MBCSV_CB_ARG* cbk_arg) {
  assert(lgs_is_peer_v8());
  TRACE_ENTER();
  auto ckpt_msg_v8 = static_cast<lgsv_ckpt_msg_v8_t *>(ckpt_msg);
  auto data = &ckpt_msg_v8->ckpt_rec.push_async;
  return ckpt_decode_log_struct(cb, cbk_arg, ckpt_msg, data,
                                EncodeDecodePushAsync);
}

uint32_t DecodePopAsync(lgs_cb_t* cb, void* ckpt_msg,
                        NCS_MBCSV_CB_ARG* cbk_arg) {
  assert(lgs_is_peer_v8());
  auto ckpt_msg_v8 = static_cast<lgsv_ckpt_msg_v8_t *>(ckpt_msg);
  auto data = &ckpt_msg_v8->ckpt_rec.pop_async;
  return ckpt_decode_log_struct(cb, cbk_arg, ckpt_msg, data,
                                EncodeDecodePopAsync);
}

uint32_t DecodePopAndWriteAsync(lgs_cb_t* cb, void* ckpt_msg,
                                NCS_MBCSV_CB_ARG* cbk_arg) {
  assert(lgs_is_peer_v8());
  auto ckpt_msg_v8 = static_cast<lgsv_ckpt_msg_v8_t *>(ckpt_msg);
  auto data = &ckpt_msg_v8->ckpt_rec.pop_and_write_async;
  return ckpt_decode_log_struct(cb, cbk_arg, ckpt_msg, data,
                                EncodeDecodePopAndWriteAsync);
}

uint32_t ckpt_proc_push_async(lgs_cb_t* cb, void* data) {
  TRACE_ENTER();
  assert(lgs_is_peer_v8() && "The peer should run with V8 or beyond!");
  auto data_v8 = static_cast<lgsv_ckpt_msg_v8_t*>(data);
  auto param = &data_v8->ckpt_rec.push_async;
  //Dump(param);
  auto cache = std::make_shared<Cache::Data>(param);
  Cache::instance()->Push(cache);
  // Remember to free memory for string types that are allocated by
  // the underlying edu layer.
  lgs_free_edu_mem(param->log_record);
  lgs_free_edu_mem(param->from_node);
  lgs_free_edu_mem(param->svc_name);
  return NCSCC_RC_SUCCESS;
}

uint32_t ckpt_proc_pop_async(lgs_cb_t* cb, void* data) {
  TRACE_ENTER();
  assert(lgs_is_peer_v8() && "The peer should run with V8 or beyond!");
  auto data_v8 = static_cast<lgsv_ckpt_msg_v8_t*>(data);
  auto param = &data_v8->ckpt_rec.pop_async;
  uint64_t seq_id = param->seq_id;
  auto top = Cache::instance()->Front();
  if (top->seq_id_ != seq_id) {
    LOG_ER("Out of sync! Expected seq: (%" PRIu64 "), Got: (%" PRIu64 ")",
           seq_id, top->seq_id_);
    return NCSCC_RC_FAILURE;
  }

  TRACE("Pop the element with seq id: (%" PRIu64 ")", seq_id);
  Cache::instance()->Pop();
  return NCSCC_RC_SUCCESS;
}

uint32_t ckpt_proc_pop_write_async(lgs_cb_t* cb, void* data) {
  TRACE_ENTER();
  assert(lgs_is_peer_v8() && "The peer should run with V8 or beyond!");
  auto data_v8 = static_cast<lgsv_ckpt_msg_v8_t*>(data);
  auto param = &data_v8->ckpt_rec.pop_and_write_async;
  uint64_t seq_id = param->seq_id;
  auto top = Cache::instance()->Front();
  if (top->seq_id_ != seq_id) {
    LOG_ER("Out of sync! Expected seq: (%" PRIu64 "), Got: (%" PRIu64 ")",
           seq_id, top->seq_id_);
    return NCSCC_RC_FAILURE;
  }

  TRACE("Pop the element with seq id: (%" PRIu64 ")", seq_id);
  Cache::instance()->Pop();

  char* log_file = param->log_file;
  auto timestamp = param->timestamp;
  auto stream = log_stream_get_by_id(param->stream_id);
  if (stream ==  nullptr) {
    LOG_NO("Not found stream id (%d)", param->stream_id);
    lgs_free_edu_mem(param->log_record);
    lgs_free_edu_mem(log_file);
    return NCSCC_RC_SUCCESS;
  }

  stream->logRecordId = param->record_id;
  stream->curFileSize = param->file_size;
  stream->logFileCurrent = param->log_file;

  return WriteOnStandby(stream, timestamp, log_file, param->log_record);
}

/****************************************************************************
 * Name          : edp_ed_ckpt_msg_v8
 *
 * Description   : This function is an EDU program for encoding/decoding
 *                 lgsv checkpoint messages. This program runs the
 *                 edp_ed_hdr_rec program first to decide the
 *                 checkpoint message type based on which it will call the
 *                 appropriate EDU programs for the different checkpoint
 *                 messages.
 *
 * Arguments     : EDU_HDL - pointer to edu handle,
 *                 EDU_TKN - internal edu token to help encode/decode,
 *                 POINTER to the structure to encode/decode from/to,
 *                 data length specifying number of structures,
 *                 EDU_BUF_ENV - pointer to buffer for encoding/decoding.
 *                 op - operation type being encode/decode.
 *                 EDU_ERR - out param to indicate errors in processing.
 *
 * Return Values : NCSCC_RC_SUCCESS/NCSCC_RC_FAILURE
 *
 * Notes         : None.
 *****************************************************************************/

uint32_t edp_ed_ckpt_msg_v8(EDU_HDL *edu_hdl, EDU_TKN *edu_tkn, NCSCONTEXT ptr,
                            uint32_t *ptr_data_len, EDU_BUF_ENV *buf_env,
                            EDP_OP_TYPE op, EDU_ERR *o_err) {
  TRACE_ENTER();
  lgsv_ckpt_msg_v8_t *ckpt_msg_ptr = nullptr, **ckpt_msg_dec_ptr;
  EDU_INST_SET ckpt_msg_ed_rules[] = {
      {EDU_START, edp_ed_ckpt_msg_v8, 0, 0, 0, sizeof(lgsv_ckpt_msg_v8_t), 0,
       nullptr},
      {EDU_EXEC, edp_ed_header_rec, 0, 0, 0,
       (long)&((lgsv_ckpt_msg_v8_t *)0)->header, 0, nullptr},

      {EDU_TEST, ncs_edp_uns32, 0, 0, 0,
       (long)&((lgsv_ckpt_msg_v8_t *)0)->header, 0,
       (EDU_EXEC_RTINE)ckpt_msg_test_type},

      /* Reg Record */
      {EDU_EXEC, edp_ed_reg_rec_v6, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.initialize_client, 0,
       nullptr},

      /* Finalize record */
      {EDU_EXEC, edp_ed_finalize_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.finalize_client, 0, nullptr},

      /* write log Record */
      {EDU_EXEC, edp_ed_write_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.write_log, 0, nullptr},

      /* Open stream */
      {EDU_EXEC, edp_ed_open_stream_rec, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.stream_open, 0, nullptr},

      /* Close stream */
      {EDU_EXEC, edp_ed_close_stream_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.stream_close, 0, nullptr},

      /* Agent dest */
      {EDU_EXEC, edp_ed_agent_down_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.stream_cfg, 0, nullptr},

      /* Cfg stream */
      {EDU_EXEC, edp_ed_cfg_stream_rec_v6, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.stream_cfg, 0, nullptr},

      /* Lgs cfg */
      {EDU_EXEC, edp_ed_lgs_cfg_rec_v5, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.lgs_cfg, 0, nullptr},

      /* Push a write async */
      {EDU_EXEC, EncodeDecodePushAsync, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.push_async, 0, nullptr},

      /* Pop a write async */
      {EDU_EXEC, EncodeDecodePopAsync, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.pop_async, 0, nullptr},

      /* Pop a write a sync and after done processing the write  request */
      {EDU_EXEC, EncodeDecodePopAndWriteAsync, 0, 0, static_cast<int>(EDU_EXIT),
       (long)&((lgsv_ckpt_msg_v8_t *)0)->ckpt_rec.pop_and_write_async, 0,
       nullptr},

      {EDU_END, 0, 0, 0, 0, 0, 0, nullptr},
  };

  if (op == EDP_OP_TYPE_ENC) {
    ckpt_msg_ptr = static_cast<lgsv_ckpt_msg_v8_t *>(ptr);
  } else if (op == EDP_OP_TYPE_DEC) {
    ckpt_msg_dec_ptr = static_cast<lgsv_ckpt_msg_v8_t **>(ptr);
    if (*ckpt_msg_dec_ptr == nullptr) {
      *o_err = EDU_ERR_MEM_FAIL;
      return NCSCC_RC_FAILURE;
    }
    memset(*ckpt_msg_dec_ptr, '\0', sizeof(lgsv_ckpt_msg_v8_t));
    ckpt_msg_ptr = *ckpt_msg_dec_ptr;
  } else {
    ckpt_msg_ptr = static_cast<lgsv_ckpt_msg_v8_t *>(ptr);
  }

  return m_NCS_EDU_RUN_RULES(edu_hdl, edu_tkn, ckpt_msg_ed_rules, ckpt_msg_ptr,
                             ptr_data_len, buf_env, op, o_err);
}

void Dump(const CkptPushAsync* data) {
  LOG_NO("- CkptPushAsync info - ");
  LOG_NO("invocation: %llu", data->invocation);
  LOG_NO("client_id: %u", data->client_id);
  LOG_NO("stream_id: %u", data->stream_id);
  LOG_NO("svc_name: %s", data->svc_name == nullptr ? "(null)" : data->svc_name);
  LOG_NO("from_node: %s", data->from_node == nullptr ? "(null)" :
         data->from_node);
  LOG_NO("log_record: %s", data->log_record);
  LOG_NO("seq_id_: %" PRIu64, data->seq_id);
  LOG_NO("Queue at: %" PRIu64, data->queue_at);
}
