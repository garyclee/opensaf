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

#include "log/logd/lgs_cache.h"

#include "log/logd/lgs_dest.h"
#include "log/logd/lgs_mbcsv_cache.h"
#include "log/logd/lgs_evt.h"
#include "log/logd/lgs_evt.h"
#include "log/logd/lgs_mbcsv.h"
#include "log/logd/lgs_config.h"
#include "base/time.h"

// The unique id of each queue element. Using this sequence id
// to check if the standby is kept the queue in sync with the active.
static size_t gl_seq_num = 0;

// Periodic timer. Using in the main poll when the cache has data.
static const unsigned kTimeoutMs = 100;

// Represent the maximum of the queue. Why 1000? A too large would impact
// the time of cold sync, or in other word impact the standby node startup.
// Besides, the last elements of the queue would be expired when LOG goes
// through such a large queue.
static const unsigned kMaxQueueSize = 1000;

// The valid range of the LOG resilient time.
static const unsigned kMaxTimeoutInSecond = 30;
static const unsigned kMinTimeoutInSecond = 15;

static bool is_streaming_supported(const log_stream_t* stream) {
  return stream->name != SA_LOG_STREAM_ALARM &&
      stream->name != SA_LOG_STREAM_NOTIFICATION;
}

Cache::WriteAsyncInfo::WriteAsyncInfo(WriteAsyncParam* param, MDS_DEST fr_dest,
                                      const char* node_name) {
  TRACE_ENTER();
  invocation = param->invocation;
  ack_flags  = param->ack_flags;
  client_id  = param->client_id;
  stream_id  = param->lstr_id;
  severity   = 0;
  dest       = fr_dest;
  log_stamp  = 0;
  svc_name   = nullptr;
  from_node  = nullptr;
  log_stream_t* str = stream();
  if (is_streaming_supported(str) == true) {
    severity  = param->logRecord->logHeader.genericHdr.logSeverity;
    svc_name  = strdup(osaf_extended_name_borrow(
        param->logRecord->logHeader.genericHdr.logSvcUsrName));
    log_stamp = param->logRecord->logTimeStamp;
    from_node  = strdup(node_name);
  }
}

Cache::WriteAsyncInfo::WriteAsyncInfo(const CkptPushAsync* data) {
  TRACE_ENTER();
  invocation = data->invocation;
  ack_flags  = data->ack_flags;
  client_id  = data->client_id;
  stream_id  = data->stream_id;
  log_stamp  = data->log_stamp;
  severity   = data->severity;
  dest       = data->dest;
  svc_name   = nullptr;
  from_node  = nullptr;
  if (data->svc_name)  svc_name  = strdup(data->svc_name);
  if (data->from_node) from_node = strdup(data->from_node);
}

std::string Cache::WriteAsyncInfo::info() const {
  TRACE_ENTER();
  char output[256];
  snprintf(output, sizeof(output), "invocation = %llu, client(%s) = %" PRIx64,
           invocation, from_node == nullptr ? "(null)" : from_node, dest);
  LOG_NO("info = %s", output);
  return std::string{output};
}

void Cache::WriteAsyncInfo::Dump() const {
  LOG_NO("invocation: %llu", invocation);
  LOG_NO("client_id: %u", client_id);
  LOG_NO("stream_id: %u", stream_id);
  LOG_NO("svc_name: %s", svc_name == nullptr ? "(null)" : svc_name);
  LOG_NO("from_node: %s", from_node == nullptr ? "(null)" : from_node);
}

void Cache::WriteAsyncInfo::CloneData(CkptPushAsync* output) const {
  TRACE_ENTER();
  output->invocation = invocation;
  output->ack_flags  = ack_flags;
  output->client_id  = client_id;
  output->stream_id  = stream_id;
  output->svc_name   = svc_name;
  output->log_stamp  = log_stamp;
  output->severity   = severity;
  output->dest       = dest;
  output->from_node  = from_node;
}

Cache::Data::Data(std::shared_ptr<WriteAsyncInfo> info,
                  char* log_record, int Size)
    : param_{info}, log_record_{log_record}, size_{Size} {
  queue_at_ = base::TimespecToNanos(base::ReadMonotonicClock());
  seq_id_   = gl_seq_num++;
}

Cache::Data::Data(const CkptPushAsync* data) {
  TRACE_ENTER();
  param_      = std::make_shared<WriteAsyncInfo>(data);
  queue_at_   = data->queue_at;
  seq_id_     = data->seq_id;
  log_record_ = strdup(data->log_record);
  size_       = strlen(log_record_);
}

void Cache::Data::Dump() const {
  LOG_NO("- Cache::Data - ");
  LOG_NO("log_record: %s", log_record_);
  LOG_NO("seq_id_: %" PRIu64, seq_id_);
  LOG_NO("Queue at: %" PRIu64, queue_at_);
  param_->Dump();
}

static bool can_streaming(const log_stream_t* stream) {
  return (stream->name != SA_LOG_STREAM_ALARM &&
          stream->name != SA_LOG_STREAM_NOTIFICATION);
}

void Cache::Data::Streaming() const {
  TRACE_ENTER();
  log_stream_t* stream = param_->stream();
  if (stream == nullptr || can_streaming(stream) == false) return;

  // Packing Record data that is carring necessary information
  // to form RFC5424 syslog msg, and sends to destination name(s).
  RecordData data{};
  timespec time;
  data.name        = stream->name.c_str();
  data.logrec      = log_record_;
  data.networkname = lgs_get_networkname().c_str();
  data.msgid       = stream->rfc5424MsgId.c_str();
  data.isRtStream  = stream->isRtStream;
  data.recordId    = stream->logRecordId;
  data.hostname    = param_->from_node;
  data.appname     = param_->svc_name;
  data.sev         = param_->severity;
  data.facilityId  = stream->facilityId;
  time.tv_sec      = (param_->log_stamp / (SaTimeT)SA_TIME_ONE_SECOND);
  time.tv_nsec     = (param_->log_stamp % (SaTimeT)SA_TIME_ONE_SECOND);
  data.time        = time;
  WriteToDestination(data, stream->dest_names);
}

bool Cache::Data::is_overdue() const {
  uint32_t max_time = Cache::instance()->Timeout();
  timespec current  = base::ReadMonotonicClock();
  timespec queue_at = base::NanosToTimespec(queue_at_);
  timespec max_resilience{static_cast<time_t>(max_time), 0};
  return (current - queue_at > max_resilience);
}

bool Cache::Data::is_valid(std::string* reason) const {
  if (is_stream_open() == false) {
    *reason = "the log stream has been closed";
    return false;
  }
  if (is_overdue() == true) {
    *reason = "the record is overdue (stream: " + param_->stream()->name + ")";
    return false;
  }
  return true;
}

void Cache::Data::CloneData(CkptPushAsync* output) const {
  TRACE_ENTER();
  param_->CloneData(output);
  output->queue_at   = queue_at_;
  output->seq_id     = seq_id_;
  output->log_record = log_record_;
}

int Cache::Data::SyncPushWithStandby() const {
  TRACE_ENTER();
  assert(is_active() == true && "This instance does not run with active role");
  if (lgs_is_peer_v8() == false) return NCSCC_RC_SUCCESS;
  lgsv_ckpt_msg_v8_t ckpt_v8;
  void* ckpt_data;
  memset(&ckpt_v8, 0, sizeof(ckpt_v8));
  ckpt_v8.header.ckpt_rec_type = LGS_CKPT_PUSH_ASYNC;
  ckpt_v8.header.num_ckpt_records = 1;
  ckpt_v8.header.data_len = 1;
  auto data = &ckpt_v8.ckpt_rec.push_async;
  CloneData(data);
  ckpt_data = &ckpt_v8;
  return lgs_ckpt_send_async(lgs_cb, ckpt_data, NCS_MBCSV_ACT_ADD);
}

int Cache::Data::SyncPopWithStandby() const {
  TRACE_ENTER();
  assert(is_active() == true && "This instance does not run with active role");
  if (lgs_is_peer_v8() == false) return NCSCC_RC_SUCCESS;
  lgsv_ckpt_msg_v8_t ckpt_v8;
  memset(&ckpt_v8, 0, sizeof(ckpt_v8));
  ckpt_v8.header.ckpt_rec_type = LGS_CKPT_POP_ASYNC;
  ckpt_v8.header.num_ckpt_records = 1;
  ckpt_v8.header.data_len = 1;
  CkptPopAsync* data = &ckpt_v8.ckpt_rec.pop_async;
  data->seq_id = seq_id_;
  return lgs_ckpt_send_async(lgs_cb, &ckpt_v8, NCS_MBCSV_ACT_ADD);
}

int Cache::Data::SyncWriteWithStandby() const {
  TRACE_ENTER();
  assert(is_active() == true && "This instance does not run with active role");
  if (lgs_is_peer_v8() == false) return NCSCC_RC_SUCCESS;
  log_stream_t* stream = param_->stream();
  if (stream == nullptr) {
    LOG_NO("The stream id (%u) is closed. Drop the write sync.", param_->stream_id);
    return NCSCC_RC_SUCCESS;
  }
  lgs_ckpt_log_async(stream, log_record_);
  return NCSCC_RC_SUCCESS;
}

int Cache::Data::SyncPopAndWriteWithStandby() const {
  TRACE_ENTER();
  assert(is_active() == true && "This instance does not run with active role");
  log_stream_t* stream = param_->stream();
  if (stream == nullptr) {
    LOG_NO("The stream id (%u) is closed. Drop the pop&write sync.", param_->stream_id);
    return NCSCC_RC_SUCCESS;
  }

  lgsv_ckpt_msg_v8_t ckpt_v8;
  memset(&ckpt_v8, 0, sizeof(ckpt_v8));
  ckpt_v8.header.ckpt_rec_type = LGS_CKPT_POP_WRITE_ASYNC;
  ckpt_v8.header.num_ckpt_records = 1;
  ckpt_v8.header.data_len = 1;

  auto data = &ckpt_v8.ckpt_rec.pop_and_write_async;
  data->log_record = log_record_;
  data->stream_id  = stream->streamId;
  data->record_id  = stream->logRecordId;
  data->file_size  = stream->curFileSize;
  data->log_file   = const_cast<char*>(stream->logFileCurrent.c_str());
  data->timestamp  = stream->act_last_close_timestamp;
  data->seq_id     = seq_id_;
  return lgs_ckpt_send_async(lgs_cb, &ckpt_v8, NCS_MBCSV_ACT_ADD);
}

int Cache::Data::Write() const {
  TRACE_ENTER();
  log_stream_t* stream = param_->stream();
  assert(stream && "log stream is nullptr");
  return log_stream_write_h(stream, log_record_, size_);
}

bool Cache::Data::IsActToClientRequired() const {
  return (is_client_alive() == true &&
          param_->ack_flags == SA_LOG_RECORD_WRITE_ACK);
}

void Cache::Data::AckToClient(SaAisErrorT code) const {
  TRACE_ENTER();
  assert(is_active() == true && "This instance does not run with active role");
  if (IsActToClientRequired() == false) return;
  lgs_send_write_log_ack(param_->client_id, param_->invocation, code, param_->dest);
}

int Cache::EncodeColdSync(NCS_UBAID* uba) const {
  TRACE_ENTER();
  assert(is_active() == true && "This instance does not run with active role");
  if (lgs_is_peer_v8() == false) return NCSCC_RC_SUCCESS;

  uint8_t* pheader = ncs_enc_reserve_space(uba, sizeof(lgsv_ckpt_header_t));
  if (pheader == nullptr) {
    LOG_NO("Cache::ColdSync failed.");
    return EDU_ERR_MEM_FAIL;
  }
  ncs_enc_claim_space(uba, sizeof(lgsv_ckpt_header_t));

  EDU_ERR ederror;
  uint32_t num_rec = 0;
  for (const auto& e : pending_write_async_) {
    CkptPushAsync data;
    e->CloneData(&data);
    int rc = m_NCS_EDU_EXEC(&lgs_cb->edu_hdl, EncodeDecodePushAsync, uba,
                            EDP_OP_TYPE_ENC, &data, &ederror);
    if (rc != NCSCC_RC_SUCCESS) {
      m_NCS_EDU_PRINT_ERROR_STRING(ederror);
      return rc;
    }
    num_rec++;
  }
  lgsv_ckpt_header_t ckpt_hdr;
  memset(&ckpt_hdr, 0, sizeof(ckpt_hdr));
  ckpt_hdr.ckpt_rec_type = LGS_CKPT_PUSH_ASYNC;
  ckpt_hdr.num_ckpt_records = num_rec;
  ncs_encode_32bit(&pheader, ckpt_hdr.ckpt_rec_type);
  ncs_encode_32bit(&pheader, ckpt_hdr.num_ckpt_records);
  ncs_encode_32bit(&pheader, ckpt_hdr.data_len);
  return NCSCC_RC_SUCCESS;
}

int Cache::DecodeColdSync(NCS_UBAID* uba, lgsv_ckpt_header_t* header,
                          void* vdata, void** vckpt_rec,
                          size_t ckpt_rec_size) const {
  TRACE_ENTER();
  assert(is_active() == false && "This instance does not run with standby role");
  if (lgs_is_peer_v8() == false) return NCSCC_RC_SUCCESS;

  assert(uba && header && "Either uba or header is nullptr");
  if (dec_ckpt_header(uba, header) != NCSCC_RC_SUCCESS) {
    LOG_NO("lgs_dec_ckpt_header FAILED");
    return NCSCC_RC_FAILURE;
  }

  if (header->ckpt_rec_type != LGS_CKPT_PUSH_ASYNC) {
    LOG_NO("failed: LGS_CKPT_PUSH_ASYNC type is expected, got %u", header->ckpt_rec_type);
    return NCSCC_RC_FAILURE;
  }

  uint32_t num_rec = header->num_ckpt_records;
  int rc = NCSCC_RC_SUCCESS;
  EDU_ERR ederror;
  lgsv_ckpt_msg_v8_t msg_v8;
  auto data = &msg_v8.ckpt_rec.push_async;
  CkptPushAsync* cache_data;
  while (num_rec) {
    cache_data = data;
    rc = m_NCS_EDU_EXEC(&lgs_cb->edu_hdl, EncodeDecodePushAsync,
                        uba, EDP_OP_TYPE_DEC,
                        &cache_data, &ederror);
    if (rc != NCSCC_RC_SUCCESS) {
      m_NCS_EDU_PRINT_ERROR_STRING(ederror);
      return rc;
    }

    rc = process_ckpt_data(lgs_cb, vdata);
    if (rc != NCSCC_RC_SUCCESS) return rc;

    memset(*vckpt_rec, 0, ckpt_rec_size);
    --num_rec;
  }
  return NCSCC_RC_SUCCESS;
}

void Cache::PeriodicCheck() {
  // NOTE: Avoid adding debug trace into this periodic check 'context',
  // otherwise the log may be get flooded when the file system is hung.
  if (Empty() == true || is_active() == false) return;
  PopOverdueData();
  FlushFrontElement();
}

void Cache::Write(std::shared_ptr<Data> data) {
  TRACE_ENTER();
  // The resilience feature is disable. Fwd request to I/O thread right away.
  if (Capacity() == 0) {
    int rc = data->Write();
    if (rc == -1 || rc == -2) {
      data->AckToClient(SA_AIS_ERR_TRY_AGAIN);
      return;
    }
    // Write OK. Do post processings.
    PostWrite(data);
    return;
  }

  // The resilience feature is enabled. Caching the request if needed.
  if (Empty() == true && is_iothread_ready() == true) {
    int rc = data->Write();
    // TODO(vu.m.nguyen): the error code is very unclear to know
    // what '-1' and '-2' really mean.
    if (rc == -1 || rc == -2) {
      Push(data);
      return;
    }
    // Write OK. Do post processings.
    PostWrite(data);
    return;
  }

  // Either having data in the queue or the io thread is not yet ready.
  Push(data);
  FlushFrontElement();
}

void Cache::PostWrite(std::shared_ptr<Data> data) {
  data->Streaming();
  data->SyncWriteWithStandby();
  data->AckToClient(SA_AIS_OK);
}

void Cache::PopOverdueData() {
  if (Empty() == true || is_active() == false) return;
  std::string reason{"Ok"};
  auto data = Front();
  if (data->is_valid(&reason) == false) {
    // Either the targeting stream has been closed or the owner is dead.
    // syslog the detailed info about dropped log record if latter case.
    if (data->is_client_alive() == false) {
      LOG_NO("Drop the invalid log record, reason: %s", reason.c_str());
      LOG_NO("The record info: %s", data->record());
    }
    Pop(false);
  }
}

void Cache::FlushFrontElement() {
  if (Empty() || !is_active() || !is_iothread_ready()) return;
  auto data = Front();
  int rc = data->Write();
  // Write still gets timeout, do nothing.
  if ((rc == -1) || (rc == -2)) return;
  // Flush the front element successfully.
  Pop(true);
}

void Cache::Push(std::shared_ptr<Data> data) {
  TRACE_ENTER();
  if (Full() == true) {
    data->AckToClient(SA_AIS_ERR_TRY_AGAIN);
    return;
  }
  if (is_active() == true) {
    data->SyncPushWithStandby();
  }
  pending_write_async_.push_back(data);
  TRACE("Number of pending reqs after push: %zu", Size());
}

void Cache::Pop(bool wstatus) {
  TRACE_ENTER();
  auto data = Front();
  if (is_active() == true) {
    // The data is going to be dropped as it is no longer valid.
    if (wstatus == false) {
      data->SyncPopWithStandby();
    } else {
      // The data has been written successfully to file. Do post-processings.
      data->Streaming();
      data->SyncPopAndWriteWithStandby();
    }
    SaAisErrorT ack_code = wstatus ? SA_AIS_OK : SA_AIS_ERR_TRY_AGAIN;
    data->AckToClient(ack_code);
  }
  pending_write_async_.pop_front();
  TRACE("Number of pending reqs after pop: %zu", Size());
}

int Cache::GeneratePollTimeout(timespec last) const {
  if (Size() == 0 || is_active() == false) return -1;
  struct timespec passed_time;
  struct timespec current = base::ReadMonotonicClock();
  osaf_timespec_subtract(&current, &last, &passed_time);
  auto passed_time_ms = osaf_timespec_to_millis(&passed_time);
  return (passed_time_ms < kTimeoutMs) ? (kTimeoutMs - passed_time_ms) : 0;
}

uint32_t Cache::Timeout() const {
  uint32_t timeout = *(static_cast<const uint32_t*>(
      lgs_cfg_get(LGS_IMM_LOG_RESILIENCE_TIMEOUT)));
  return timeout;
}

size_t Cache::Capacity() const {
  uint32_t max_size = *(static_cast<const uint32_t*>(
      lgs_cfg_get(LGS_IMM_LOG_MAX_PENDING_WRITE_REQ)));
  return max_size;
}

bool Cache::VerifyMaxQueueSize(uint32_t max) const {
  if (max <= kMaxQueueSize && max >= Size()) return true;
  return false;
}

bool Cache::VerifyResilienceTime(uint32_t time) const {
  if (time >= kMinTimeoutInSecond && time <= kMaxTimeoutInSecond) return true;
  return false;
}
