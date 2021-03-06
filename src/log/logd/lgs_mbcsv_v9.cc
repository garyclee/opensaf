/*      -*- OpenSAF  -*-
 *
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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

#include "log/logd/lgs_mbcsv_v9.h"
#include "base/logtrace.h"

/****************************************************************************
 * Name          : edp_ed_cfg_stream_rec_v9
 *
 * Description   : This function is an EDU program for encoding/decoding
 *                 lgsv checkpoint cfg_update_stream log rec.
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

uint32_t edp_ed_cfg_stream_rec_v9(EDU_HDL *edu_hdl, EDU_TKN *edu_tkn,
                                  NCSCONTEXT ptr, uint32_t *ptr_data_len,
                                  EDU_BUF_ENV *buf_env, EDP_OP_TYPE op,
                                  EDU_ERR *o_err) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  lgs_ckpt_stream_cfg_v4_t *ckpt_stream_cfg_msg_ptr = nullptr,
                           **ckpt_stream_cfg_msg_dec_ptr;
  TRACE_ENTER();
  EDU_INST_SET ckpt_stream_cfg_rec_ed_rules[] = {
      {EDU_START, edp_ed_cfg_stream_rec_v9, 0, 0, 0,
       sizeof(lgs_ckpt_stream_cfg_v4_t), 0, nullptr},
      {EDU_EXEC, ncs_edp_string, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->name, 0,
       nullptr},
      {EDU_EXEC, ncs_edp_string, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->fileName,
       0, nullptr},
      {EDU_EXEC, ncs_edp_string, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->pathName,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->maxLogFileSize,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))
                       ->fixedLogRecordSize,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->haProperty,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->logFullAction,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))
                       ->logFullHaltThreshold,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->maxFilesRotated,
       0, nullptr},
      {EDU_EXEC, ncs_edp_string, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->logFileFormat,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->severityFilter,
       0, nullptr},
      {EDU_EXEC, ncs_edp_string, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->logFileCurrent,
       0, nullptr},
      {EDU_EXEC, ncs_edp_string, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->dest_names,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns64, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))
                       ->c_file_close_time_stamp,
       0, nullptr},
      {EDU_EXEC, ncs_edp_uns32, 0, 0, 0,
       (int64_t) &
           (reinterpret_cast<lgs_ckpt_stream_cfg_v4_t *>(0))->facilityId,
       0, nullptr},
      {EDU_END, 0, 0, 0, 0, 0, 0, nullptr},
  };

  if (op == EDP_OP_TYPE_ENC) {
    ckpt_stream_cfg_msg_ptr = static_cast<lgs_ckpt_stream_cfg_v4_t *>(ptr);
  } else if (op == EDP_OP_TYPE_DEC) {
    ckpt_stream_cfg_msg_dec_ptr = static_cast<lgs_ckpt_stream_cfg_v4_t **>(ptr);
    if (*ckpt_stream_cfg_msg_dec_ptr == nullptr) {
      *o_err = EDU_ERR_MEM_FAIL;
      return NCSCC_RC_FAILURE;
    }
    memset(*ckpt_stream_cfg_msg_dec_ptr, '\0',
           sizeof(lgs_ckpt_stream_cfg_v4_t));
    ckpt_stream_cfg_msg_ptr = *ckpt_stream_cfg_msg_dec_ptr;
  } else {
    ckpt_stream_cfg_msg_ptr = static_cast<lgs_ckpt_stream_cfg_v4_t *>(ptr);
  }

  rc = m_NCS_EDU_RUN_RULES(edu_hdl, edu_tkn, ckpt_stream_cfg_rec_ed_rules,
                           ckpt_stream_cfg_msg_ptr, ptr_data_len, buf_env, op,
                           o_err);
  TRACE_LEAVE();
  return rc;
}

/****************************************************************************
 * Name          : edp_ed_ckpt_msg_v9
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

uint32_t edp_ed_ckpt_msg_v9(EDU_HDL *edu_hdl, EDU_TKN *edu_tkn, NCSCONTEXT ptr,
                            uint32_t *ptr_data_len, EDU_BUF_ENV *buf_env,
                            EDP_OP_TYPE op, EDU_ERR *o_err) {
  uint32_t rc = NCSCC_RC_SUCCESS;
  lgsv_ckpt_msg_v9_t *ckpt_msg_ptr = nullptr, **ckpt_msg_dec_ptr;
  TRACE_ENTER();
  EDU_INST_SET ckpt_msg_ed_rules[] = {
      {EDU_START, edp_ed_ckpt_msg_v9, 0, 0, 0, sizeof(lgsv_ckpt_msg_v9_t), 0,
       nullptr},
      {EDU_EXEC, edp_ed_header_rec, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->header, 0,
       nullptr},

      {EDU_TEST, ncs_edp_uns32, 0, 0, 0,
       (int64_t) & (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->header, 0,
       (EDU_EXEC_RTINE)ckpt_msg_test_type},

      /* Reg Record */
      {EDU_EXEC, edp_ed_reg_rec_v6, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) & (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))
                       ->ckpt_rec.initialize_client,
       0, nullptr},

      /* Finalize record */
      {EDU_EXEC, edp_ed_finalize_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) & (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))
                       ->ckpt_rec.finalize_client,
       0, nullptr},

      /* write log Record */
      {EDU_EXEC, edp_ed_write_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.write_log,
       0, nullptr},

      /* Open stream */
      {EDU_EXEC, edp_ed_open_stream_rec, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.stream_open,
       0, nullptr},

      /* Close stream */
      {EDU_EXEC, edp_ed_close_stream_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.stream_close,
       0, nullptr},

      /* Agent dest */
      {EDU_EXEC, edp_ed_agent_down_rec_v2, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.stream_cfg,
       0, nullptr},

      /* Cfg stream */
      {EDU_EXEC, edp_ed_cfg_stream_rec_v9, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.stream_cfg,
       0, nullptr},

      /* Lgs cfg */
      {EDU_EXEC, edp_ed_lgs_cfg_rec_v5, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.lgs_cfg,
       0, nullptr},

      /* Push a write async */
      {EDU_EXEC, EncodeDecodePushAsync, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.push_async,
       0, nullptr},

      /* Pop a write async */
      {EDU_EXEC, EncodeDecodePopAsync, 0, 0, static_cast<int>(EDU_EXIT),
       (int64_t) &
           (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))->ckpt_rec.pop_async,
       0, nullptr},

      /* Pop a write a sync and after done processing the write  request */
      {EDU_EXEC, EncodeDecodePopAndWriteAsync, 0, 0,
       static_cast<int>(EDU_EXIT),
       (int64_t) & (reinterpret_cast<lgsv_ckpt_msg_v9_t *>(0))
                       ->ckpt_rec.pop_and_write_async,
       0, nullptr},

      {EDU_END, 0, 0, 0, 0, 0, 0, nullptr},
  };

  if (op == EDP_OP_TYPE_ENC) {
    ckpt_msg_ptr = static_cast<lgsv_ckpt_msg_v9_t *>(ptr);
  } else if (op == EDP_OP_TYPE_DEC) {
    ckpt_msg_dec_ptr = static_cast<lgsv_ckpt_msg_v9_t **>(ptr);
    if (*ckpt_msg_dec_ptr == nullptr) {
      *o_err = EDU_ERR_MEM_FAIL;
      return NCSCC_RC_FAILURE;
    }
    memset(*ckpt_msg_dec_ptr, '\0', sizeof(lgsv_ckpt_msg_v9_t));
    ckpt_msg_ptr = *ckpt_msg_dec_ptr;
  } else {
    ckpt_msg_ptr = static_cast<lgsv_ckpt_msg_v9_t *>(ptr);
  }

  rc = m_NCS_EDU_RUN_RULES(edu_hdl, edu_tkn, ckpt_msg_ed_rules, ckpt_msg_ptr,
                           ptr_data_len, buf_env, op, o_err);
  TRACE_LEAVE();
  return rc;
}
