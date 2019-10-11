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

#ifndef MDS_MDS_TIPC_FCTRL_MSG_H_
#define MDS_MDS_TIPC_FCTRL_MSG_H_

#include <linux/tipc.h>
#include "base/ncssysf_ipc.h"
#include "base/ncssysf_tmr.h"
#include "mds/mds_dt.h"

namespace mds {

class Event {
 public:
  enum class Type {
    kEvtPortIdAll = 0,
    kEvtPortIdUp,     // event of new portid published (not used)
    kEvtPortIdDown,   // event of portid widthdrawn (not used)

    kEvtDataFlowAll,
    kEvtRcvData,       // event that received data msg from peer
    kEvtSendChunkAck,  // event to send the ack for a chunk of N accumulative
                       // data msgs (N>=1)
    kEvtRcvChunkAck,   // event that received the ack for a chunk of N
                       // accumulative data msgs (N>=1)
    kEvtSendSelectiveAck,  // event to send the ack for a number of selective
                           // data msgs (not supported)
    kEvtRcvSelectiveAck,   // event that received the ack for a numer of
                           // selective data msgs (not supported)
    kEvtDropData,          // event reported from tipc that a message is not
                           // delivered
    kEvtRcvNack,           // event that received nack message
    kEvtRcvIntro,          // event that received intro message
    kEvtTmrAll,
    kEvtTmrTxProb,    // event that tx probation timer expired for once
    kEvtTmrChunkAck,  // event to send the chunk ack
  };
  NCS_IPC_MSG next_{0};
  Type type_;

  // Used for flow event only
  struct tipc_portid id_;
  uint16_t svc_id_{0};
  uint32_t mseq_{0};
  uint16_t mfrag_{0};
  uint16_t fseq_{0};
  uint16_t chunk_size_{1};
  explicit Event(Type type):type_(type) {}
  Event(Type type, struct tipc_portid id, uint16_t svc_id,
      uint32_t mseq, uint16_t mfrag, uint16_t f_seg_num):
    id_(id), svc_id_(svc_id),
    mseq_(mseq), mfrag_(mfrag), fseq_(f_seg_num) {
    type_ = type;
  }
  Event(Type type, struct tipc_portid id, uint16_t svc_id, uint32_t mseq,
      uint16_t mfrag, uint16_t f_seg_num, uint16_t chunk_size):
    id_(id), svc_id_(svc_id), mseq_(mseq), mfrag_(mfrag),
    fseq_(f_seg_num), chunk_size_(chunk_size) {
    type_ = type;
  }
  bool IsTimerEvent() const { return (type_ > Type::kEvtTmrAll); }
  bool IsFlowEvent() const {
    return (Type::kEvtDataFlowAll < type_ && type_ < Type::kEvtTmrAll);
  }
};

class BaseMessage {
 public:
  virtual ~BaseMessage() {}
  virtual void Decode(uint8_t *msg) {}
  virtual void Encode(uint8_t *msg) {}
};

class HeaderMessage: public BaseMessage {
 public:
  enum FieldIndex {
    kMessageLength = 0,
    kSequenceNumber = 2,
    kFragmentNumber = 6,
    kLengthCheck = 8,
    kFlowControlSequenceNumber = kLengthCheck,  // reuse kLengthCheck
    kProtocolVersion = 10
  };
  uint8_t* msg_ptr_{nullptr};
  uint16_t msg_len_{0};
  uint32_t mseq_{0};
  uint16_t mfrag_{0};
  uint16_t fseq_{0};
  uint8_t pro_ver_{0};
  uint32_t pro_id_{0};
  uint16_t msg_type_{0};
  HeaderMessage() {}
  HeaderMessage(uint16_t msg_len, uint32_t mseq, uint16_t mfrag,
      uint16_t fseq);
  virtual ~HeaderMessage() {}
  void Decode(uint8_t *msg) override;
  void Encode(uint8_t *msg) override;
};

class DataMessage: public BaseMessage {
 public:
  enum FieldIndex {
    kSendType = 17,
  };
  HeaderMessage header_;
  uint16_t svc_id_{0};

  uint8_t* msg_data_{nullptr};
  uint8_t snd_type_{0};

  bool is_sent_{true};
  DataMessage() {}
  virtual ~DataMessage();
  void Decode(uint8_t *msg) override;
};

class ChunkAck: public BaseMessage {
 public:
  enum FieldIndex {
    kProtocolIdentifier = 11,
    kFlowControlMessageType = 15,
    kServiceId = 16,
    kFlowControlSequenceNumber = 18,
    kChunkAckSize = 20
  };
  static const uint8_t kChunkAckMsgType = 1;
  static const uint16_t kChunkAckMsgLength = 22;

  uint8_t msg_type_{0};
  uint16_t svc_id_{0};
  uint16_t acked_fseq_{0};
  uint16_t chunk_size_{1};
  ChunkAck() {}
  ChunkAck(uint16_t svc_id, uint16_t fseq, uint16_t chunk_size);
  virtual ~ChunkAck() {}
  void Encode(uint8_t *msg) override;
  void Decode(uint8_t *msg) override;
};

class Nack: public BaseMessage {
 public:
  enum FieldIndex {
    kProtocolIdentifier = 11,
    kFlowControlMessageType = 15,
    kServiceId = 16,
    kFlowControlSequenceNumber = 18,
  };
  static const uint8_t kNackMsgType = 2;
  static const uint16_t kNackMsgLength = 20;

  uint8_t msg_type_{0};
  uint16_t svc_id_{0};
  uint16_t nacked_fseq_{0};
  Nack() {}
  Nack(uint16_t svc_id, uint16_t fseq);
  virtual ~Nack() {}
  void Encode(uint8_t *msg) override;
  void Decode(uint8_t *msg) override;
};

class Intro: public BaseMessage {
 public:
  enum FieldIndex {
    kProtocolIdentifier = 11,
    kFlowControlMessageType = 15,
  };
  static const uint8_t kIntroMsgType = 3;
  static const uint16_t kIntroMsgLength = 16;

  uint8_t msg_type_{0};

  Intro() { msg_type_ = kIntroMsgType; }
  virtual ~Intro() {}
  void Encode(uint8_t *msg) override;
};


}  // end namespace mds

#endif  // MDS_MDS_TIPC_FCTRL_MSG_H_
