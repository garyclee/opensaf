#ifndef AMF_AMFD_NODE_STATE_MACHINE_H_
#define AMF_AMFD_NODE_STATE_MACHINE_H_

#include <memory>
#include <saClm.h>
#include "base/macros.h"
#include "amf/amfd/node_state.h"
#include "amf/amfd/timer.h"

class NodeStateMachine {
 public:
  void TimerExpired();
  void MdsUp();
  void MdsDown();
  void NodeUp();
  void SetState(std::shared_ptr<NodeState> state);

  // is this the active controller
  bool Active();

  // use for ckpt encode/decode only
  void SetState(uint32_t state);
  uint32_t GetState();

  SaTimeT FailoverDelay() const;

  std::shared_ptr<AVD_TMR> timer_;
  std::shared_ptr<NodeState> state_;

  SaClmNodeIdT node_id_;
  struct cl_cb_tag *cb_;

  NodeStateMachine(struct cl_cb_tag *cb, const SaClmNodeIdT node_id);
  ~NodeStateMachine();

 private:
 NodeStateMachine();
 DELETE_COPY_AND_MOVE_OPERATORS(NodeStateMachine);
};

#endif // AMF_AMFD_NODE_STATE_MACHINE_H_
