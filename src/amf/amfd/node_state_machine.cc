#include "base/logtrace.h"
#include "osaf/consensus/consensus.h"
#include "amf/amfd/amfd.h"
#include "amf/amfd/node_state_machine.h"

NodeStateMachine::NodeStateMachine(struct cl_cb_tag *cb,
	                           const SaClmNodeIdT node_id) :
  node_id_(node_id),
  cb_(cb) {
  timer_ = std::make_shared<AVD_TMR>();
  timer_->node_id = node_id;
  timer_->type = AVD_TMR_NODE_FAILOVER;
  timer_->is_active = false;
  timer_->tmr_id = TMR_T_NULL;
  state_ = std::make_shared<Start>(this);
}

NodeStateMachine::~NodeStateMachine() {
  avd_stop_tmr(cb_, timer_.get());
}

void NodeStateMachine::TimerExpired() {
  // in case this was triggered manually, stop the timer
  avd_stop_tmr(cb_, timer_.get());
  state_->TimerExpired();
}

void NodeStateMachine::MdsUp() {
  state_->MdsUp();
}

void NodeStateMachine::MdsDown() {
  state_->MdsDown();
}

void NodeStateMachine::NodeUp() {
  state_->NodeUp();
}

void NodeStateMachine::SetState(std::shared_ptr<NodeState> state) {
  TRACE_ENTER2("'%x'", node_id_);
  state_ = state;
  AVD_AVND *node = avd_node_find_nodeid(node_id_);
  osafassert(node != nullptr);
  m_AVSV_SEND_CKPT_UPDT_ASYNC_UPDT(cb_, node, AVSV_CKPT_NODE_FAILOVER_STATE);
  // used for cold sync
  node->failover_state = state_->GetInt();

  if (state->GetInt() == NodeState::kEnd) {
    cb_->failover_list.erase(node_id_);
  }
}

// this is called as a result of check pointing from the active
void NodeStateMachine::SetState(uint32_t state) {
  TRACE_ENTER2("'%x', state '%u', current state '%u'",
                node_id_, state, state_->GetInt());

  if (state == state_->GetInt()) {
    LOG_NO("Node state unchanged");
    return;
  } else {
    LOG_NO("New state '%u'", state);
  }

  switch (state) {
    case NodeState::kStart:
      state_ = std::make_shared<Start>(this);
      break;
    case NodeState::kLost:
      state_ = std::make_shared<Lost>(this);
      break;
    case NodeState::kLostFound:
      state_ = std::make_shared<LostFound>(this);
      break;
    case NodeState::kLostRebooting:
      state_ = std::make_shared<LostRebooting>(this);
      break;
    case NodeState::kFailed:
      state_ = std::make_shared<Failed>(this);
      break;
    case NodeState::kFailedFound:
      state_ = std::make_shared<FailedFound>(this);
      break;
    case NodeState::kEnd:
      state_ = std::make_shared<End>(this);
      cb_->failover_list.erase(node_id_);
      break;
    default:
      LOG_ER("undefined state '%u'", state);
      break;
  }
}

uint32_t NodeStateMachine::GetState() {
  return state_->GetInt();
}

SaTimeT NodeStateMachine::FailoverDelay() const {
  TRACE_ENTER();

  SaTimeT delay;
  if (node_id_ == cb_->node_id_avd_other) {
    // If peer SC, it's guaranteed to fence after this amount of time
    // (2 * FMS_TAKEOVER_REQUEST_VALID_TIME).
    // This may be smaller than node_failover_delay.
    Consensus consensus_service;
    delay = 2 * consensus_service.TakeoverValidTime();
  } else {
    delay = cb_->node_failover_delay;
  }

  TRACE("delay is %llu", delay);
  return delay * SA_TIME_ONE_SECOND;
}

bool NodeStateMachine::Active() {
  return cb_->avail_state_avd == SA_AMF_HA_ACTIVE;
}
