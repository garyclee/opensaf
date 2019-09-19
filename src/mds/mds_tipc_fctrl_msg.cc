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

#include "mds/mds_tipc_fctrl_msg.h"
#include "base/ncssysf_def.h"

namespace mds {
HeaderMessage::HeaderMessage(uint16_t msg_len, uint32_t mseq,
    uint16_t mfrag, uint16_t fseq): msg_len_(msg_len),
        mseq_(mseq), mfrag_(mfrag), fseq_(fseq) {
  pro_ver_ = MDS_PROT_FCTRL;
  pro_id_ = 0;
  msg_type_ = 0;
}

void HeaderMessage::Encode(uint8_t *msg) {
  uint8_t *ptr;

  // encode message length
  ptr = &msg[0];
  ncs_encode_16bit(&ptr, msg_len_);
  // encode sequence number
  ptr = &msg[2];
  ncs_encode_32bit(&ptr, mseq_);
  // encode sequence number
  ptr = &msg[6];
  ncs_encode_16bit(&ptr, mfrag_);
  // skip length_check: oct8&9
  // encode protocol version
  ptr = &msg[10];
  ncs_encode_8bit(&ptr, MDS_PROT_FCTRL);
}

void HeaderMessage::Decode(uint8_t *msg) {
  uint8_t *ptr;

  // decode message length
  ptr = &msg[0];
  msg_len_ = ncs_decode_16bit(&ptr);
  // decode sequence number
  ptr = &msg[2];
  mseq_ = ncs_decode_32bit(&ptr);
  // decode fragment number
  ptr = &msg[6];
  mfrag_ = ncs_decode_16bit(&ptr);
  // decode protocol version
  ptr = &msg[10];
  pro_ver_ = ncs_decode_8bit(&ptr);
  if ((pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_FCTRL) {
    // decode flow control sequence number
    ptr = &msg[8];
    fseq_ = ncs_decode_16bit(&ptr);
    // decode protocol identifier
    ptr = &msg[11];
    pro_id_ = ncs_decode_32bit(&ptr);
    if (pro_id_ == MDS_PROT_FCTRL_ID) {
      // decode message type
      ptr = &msg[15];
      msg_type_ = ncs_decode_8bit(&ptr);
    }
  } else {
    if (mfrag_ != 0) {
      ptr = &msg[8];
      fseq_ = ncs_decode_16bit(&ptr);
      if (fseq_ != 0) pro_ver_ = MDS_PROT_FCTRL;
    }
  }
}

void DataMessage::Decode(uint8_t *msg) {
  uint8_t *ptr;

  // decode service id
  ptr = &msg[MDTM_PKT_TYPE_OFFSET +
             MDTM_FRAG_HDR_LEN +
             MDS_HEADER_RCVR_SVC_ID_POSITION];
  svc_id_ = ncs_decode_16bit(&ptr);
  // decode snd_type
  ptr = &msg[17];
  snd_type_ = (ncs_decode_8bit(&ptr)) & 0x3f;
}

DataMessage::~DataMessage() {
  if (msg_data_ != nullptr) {
    delete msg_data_;
    msg_data_ = nullptr;
  }
}

ChunkAck::ChunkAck(uint16_t svc_id, uint16_t fseq, uint16_t chunk_size):
    svc_id_(svc_id), acked_fseq_(fseq), chunk_size_(chunk_size) {
  msg_type_ = kChunkAckMsgType;
}

void ChunkAck::Encode(uint8_t *msg) {
  uint8_t *ptr;
  // encode protocol identifier
  ptr = &msg[11];
  ncs_encode_32bit(&ptr, MDS_PROT_FCTRL_ID);
  // encode message type
  ptr = &msg[15];
  ncs_encode_8bit(&ptr, kChunkAckMsgType);
  // encode service id
  ptr = &msg[16];
  ncs_encode_16bit(&ptr, svc_id_);
  // encode flow control sequence number
  ptr = &msg[18];
  ncs_encode_16bit(&ptr, acked_fseq_);
  // encode chunk size
  ptr = &msg[20];
  ncs_encode_16bit(&ptr, chunk_size_);
}

void ChunkAck::Decode(uint8_t *msg) {
  uint8_t *ptr;

  // decode service id
  ptr = &msg[16];
  svc_id_ = ncs_decode_16bit(&ptr);
  // decode flow control sequence number
  ptr = &msg[18];
  acked_fseq_ = ncs_decode_16bit(&ptr);
  // decode chunk size
  ptr = &msg[20];
  chunk_size_ = ncs_decode_16bit(&ptr);
}

}  // end namespace mds
