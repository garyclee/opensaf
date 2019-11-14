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
  pro_ver_ = MDS_PROT_UNDEFINED;
  pro_id_ = 0;
  msg_type_ = 0;
}

void HeaderMessage::Encode(uint8_t *msg) {
  uint8_t *ptr;

  // encode message length
  ptr = &msg[HeaderMessage::FieldIndex::kMessageLength];
  ncs_encode_16bit(&ptr, msg_len_);
  // encode sequence number
  ptr = &msg[HeaderMessage::FieldIndex::kSequenceNumber];
  ncs_encode_32bit(&ptr, mseq_);
  // encode fragment number
  ptr = &msg[HeaderMessage::FieldIndex::kFragmentNumber];
  ncs_encode_16bit(&ptr, mfrag_);
  // skip length_check: oct8&9
  // encode protocol version
  ptr = &msg[HeaderMessage::FieldIndex::kProtocolVersion];
  ncs_encode_8bit(&ptr, MDS_PROT_FCTRL);
}

void HeaderMessage::Decode(uint8_t *msg) {
  uint8_t *ptr;
  pro_id_ = 0;
  msg_type_ = 0;
  pro_ver_ = MDS_PROT_UNDEFINED;
  // decode message length
  ptr = &msg[HeaderMessage::FieldIndex::kMessageLength];
  msg_len_ = ncs_decode_16bit(&ptr);
  // decode sequence number
  ptr = &msg[HeaderMessage::FieldIndex::kSequenceNumber];
  mseq_ = ncs_decode_32bit(&ptr);
  // decode fragment number
  ptr = &msg[HeaderMessage::FieldIndex::kFragmentNumber];
  mfrag_ = ncs_decode_16bit(&ptr);
  // The unfragment or 1st fragment always has encoded the protocol version
  // at oct10, refer to mdtm_add_mds_hdr(). Therefore we can rely on oct10
  // to identify the flow control message
  if (mfrag_ == 0 || ((mfrag_ & 0x7fff) == 1)) {
    // decode protocol version
    ptr = &msg[HeaderMessage::FieldIndex::kProtocolVersion];
    pro_ver_ = ncs_decode_8bit(&ptr);
    if ((pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_FCTRL) {
      // decode flow control sequence number
      ptr = &msg[HeaderMessage::FieldIndex::kFlowControlSequenceNumber];
      fseq_ = ncs_decode_16bit(&ptr);
      // decode protocol identifier if the mfrag_ and mseq_ are 0
      // otherwise it is always DataMessage within non-zero mseq_ and mfrag_
      if (mfrag_ == 0 && mseq_ == 0) {
        ptr = &msg[ChunkAck::FieldIndex::kProtocolIdentifier];
        pro_id_ = ncs_decode_32bit(&ptr);
        if (pro_id_ == MDS_PROT_FCTRL_ID) {
          // decode message type
          ptr = &msg[ChunkAck::FieldIndex::kFlowControlMessageType];
          msg_type_ = ncs_decode_8bit(&ptr);
        }
      }
    }
  } else {
    // this is the other fragment after 1st fragment (i.e. 2nd, 3rd, etc...)
    // the oct10 is written by fragment data, it is not encoded for
    // protocol version anymore.
    // In legacy protocol version, lengthcheck(oct8) = messagelength(oct0) - 10
    // Thus, if fseq(oct8) != messagelength(oct0) - 10, this is MDS_PROT_FCTRL
    // Otherwise, it could be either MDS_PROT_FCTRL or MDS_PROT_LEGACY
    // so set the pro_ver_ as MDS_PROT_UNDEFINED and refer to the portidmap
    // to decide the protocol version
    ptr = &msg[HeaderMessage::FieldIndex::kFlowControlSequenceNumber];
    fseq_ = ncs_decode_16bit(&ptr);
    if (msg_len_ - fseq_ - 2 != MDTM_FRAG_HDR_LEN) {
      pro_ver_ = MDS_PROT_FCTRL;
    }
  }
}

bool HeaderMessage::IsControlMessage() {
  if (((pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_FCTRL) &&
      pro_id_ == MDS_PROT_FCTRL_ID) {
    return true;
  }
  return false;
}

bool HeaderMessage::IsLegacyMessage() {
  return (pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_LEGACY;
}

bool HeaderMessage::IsFlowMessage() {
  if (((pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_FCTRL) &&
      pro_id_ != MDS_PROT_FCTRL_ID) {
    return true;
  }
  return false;
}

bool HeaderMessage::IsUndefinedMessage() {
  return (pro_ver_ & MDS_PROT_VER_MASK) == MDS_PROT_UNDEFINED;
}

void DataMessage::Decode(uint8_t *msg) {
  uint8_t *ptr;

  // decode service id
  ptr = &msg[MDTM_PKT_TYPE_OFFSET +
             MDTM_FRAG_HDR_LEN +
             MDS_HEADER_RCVR_SVC_ID_POSITION];
  svc_id_ = ncs_decode_16bit(&ptr);
  // decode snd_type
  ptr = &msg[DataMessage::FieldIndex::kSendType];
  snd_type_ = (ncs_decode_8bit(&ptr)) & 0x3f;
}

DataMessage::~DataMessage() {
  if (msg_data_ != nullptr) {
    delete[] msg_data_;
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
  ptr = &msg[ChunkAck::FieldIndex::kProtocolIdentifier];
  ncs_encode_32bit(&ptr, MDS_PROT_FCTRL_ID);
  // encode message type
  ptr = &msg[ChunkAck::FieldIndex::kFlowControlMessageType];
  ncs_encode_8bit(&ptr, kChunkAckMsgType);
  // encode service id
  ptr = &msg[ChunkAck::FieldIndex::kServiceId];
  ncs_encode_16bit(&ptr, svc_id_);
  // encode flow control sequence number
  ptr = &msg[ChunkAck::FieldIndex::kFlowControlSequenceNumber];
  ncs_encode_16bit(&ptr, acked_fseq_);
  // encode chunk size
  ptr = &msg[ChunkAck::FieldIndex::kChunkAckSize];
  ncs_encode_16bit(&ptr, chunk_size_);
}

void ChunkAck::Decode(uint8_t *msg) {
  uint8_t *ptr;

  // decode service id
  ptr = &msg[ChunkAck::FieldIndex::kServiceId];
  svc_id_ = ncs_decode_16bit(&ptr);
  // decode flow control sequence number
  ptr = &msg[ChunkAck::FieldIndex::kFlowControlSequenceNumber];
  acked_fseq_ = ncs_decode_16bit(&ptr);
  // decode chunk size
  ptr = &msg[ChunkAck::FieldIndex::kChunkAckSize];
  chunk_size_ = ncs_decode_16bit(&ptr);
}


Nack::Nack(uint16_t svc_id, uint16_t fseq):
    svc_id_(svc_id), nacked_fseq_(fseq) {
  msg_type_ = kNackMsgType;
}

void Nack::Encode(uint8_t *msg) {
  uint8_t *ptr;
  // encode protocol identifier
  ptr = &msg[Nack::FieldIndex::kProtocolIdentifier];
  ncs_encode_32bit(&ptr, MDS_PROT_FCTRL_ID);
  // encode message type
  ptr = &msg[Nack::FieldIndex::kFlowControlMessageType];
  ncs_encode_8bit(&ptr, kNackMsgType);
  // encode service id
  ptr = &msg[Nack::FieldIndex::kServiceId];
  ncs_encode_16bit(&ptr, svc_id_);
  // encode flow control sequence number
  ptr = &msg[Nack::FieldIndex::kFlowControlSequenceNumber];
  ncs_encode_16bit(&ptr, nacked_fseq_);
}

void Nack::Decode(uint8_t *msg) {
  uint8_t *ptr;

  // decode service id
  ptr = &msg[Nack::FieldIndex::kServiceId];
  svc_id_ = ncs_decode_16bit(&ptr);
  // decode flow control sequence number
  ptr = &msg[Nack::FieldIndex::kFlowControlSequenceNumber];
  nacked_fseq_ = ncs_decode_16bit(&ptr);
}


void Intro::Encode(uint8_t *msg) {
  uint8_t *ptr;
  // encode protocol identifier
  ptr = &msg[Intro::FieldIndex::kProtocolIdentifier];
  ncs_encode_32bit(&ptr, MDS_PROT_FCTRL_ID);
  // encode message type
  ptr = &msg[Intro::FieldIndex::kFlowControlMessageType];
  ncs_encode_8bit(&ptr, kIntroMsgType);
}

}  // end namespace mds
