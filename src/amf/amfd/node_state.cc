#include "base/logtrace.h"
#include "base/ncssysf_def.h"
#include "osaf/consensus/consensus.h"
#include "amf/amfd/node.h"
#include "amf/amfd/node_state.h"
#include "amf/amfd/node_state_machine.h"
#include "amf/amfd/proc.h"

NodeState::NodeState(NodeStateMachine *fsm) :
  fsm_(fsm) {
}

// state 'Start'
Start::Start(NodeStateMachine *fsm) :
  NodeState(fsm) {
  avd_stop_tmr(fsm_->cb_, fsm_->timer_.get());
}

void Start::TimerExpired() {
  LOG_ER("unexpected timer event");
}

void Start::MdsUp() {
  TRACE_ENTER();
}

void Start::MdsDown() {
  TRACE_ENTER();

  if (fsm_->Active() == true) {
    Consensus consensus_service;
    if (consensus_service.IsRemoteFencingEnabled() == true) {
      // get CLM node name
      AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
      std::string hostname = osaf_extended_name_borrow(
        &node->node_info.nodeName);
      size_t first = hostname.find_first_of("=") + 1;
      size_t end = hostname.find_first_of(",");
      hostname = hostname.substr(first, end - first);

      // fence the lost node
      opensaf_reboot(fsm_->node_id_, hostname.c_str(), "Fencing remote node");

      // failover node
      avd_node_failover(node);

      fsm_->SetState(std::make_shared<End>(fsm_));
      return;
    }
  }

  // transition to 'Lost' state
  fsm_->SetState(std::make_shared<Lost>(fsm_));
}

void Start::NodeUp() {
  TRACE_ENTER();
}

// state 'Lost'
Lost::Lost(NodeStateMachine *fsm) :
  NodeState(fsm) {
  avd_stop_tmr(fsm_->cb_, fsm_->timer_.get());
  LOG_NO("Start timer for '%x'", fsm_->node_id_);
  avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                fsm_->cb_->node_failover_delay * SA_TIME_ONE_SECOND);
}

void Lost::TimerExpired() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);
  if (fsm_->Active() == true) {
    AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
    osafassert(node != nullptr);

    LOG_NO("Completing delayed node failover for '%s'",
            node->node_name.c_str());
    avd_node_failover(node);

    // transition to 'Failed' state
    fsm_->SetState(std::make_shared<Failed>(fsm_));
  } else {
    TRACE("Timer expired in 'Lost' state for '%x'  on standby. Restart timer",
            fsm_->node_id_);

    // wait for checkpoint to transition state
    // meanwhile, restart timer in case a SC failover to this node occurs
    avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                  fsm_->cb_->node_failover_delay * SA_TIME_ONE_SECOND);
  }
}

void Lost::MdsUp() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  // transition to 'LostFound' state
  fsm_->SetState(std::make_shared<LostFound>(fsm_));
}

void Lost::MdsDown() {
  if (fsm_->Active() == true) {
    LOG_ER("unexpected MDS down event");
  }
}

void Lost::NodeUp() {
  LOG_ER("unexpected node up event");
}

// state 'LostFound'
LostFound::LostFound(NodeStateMachine *fsm) :
  NodeState(fsm) {
  avd_stop_tmr(fsm_->cb_, fsm_->timer_.get());
  avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                fsm_->cb_->node_failover_node_wait * SA_TIME_ONE_SECOND);
}

void LostFound::TimerExpired() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
  osafassert(node != nullptr);

  // reboot node if it hasn't sent node_up within timer duration,
  // meaning it hasn't rebooted since we lost contact
  LOG_WA("Lost node '%s' has reappeared after network separation",
          node->node_name.c_str());

  if (fsm_->Active() == true) {
    // amfnd thinks it's been headless and resets its rcv_msg_id to 0,
    // also do the same here to avoid 'Message ID mismatch' errors
    // at amfnd
    node->snd_msg_id = 0;

    LOG_WA("Sending node reboot order");
    avd_d2n_reboot_snd(node);

    // transition to 'Failed' state
    fsm_->SetState(std::make_shared<LostRebooting>(fsm_));
  } else {
    TRACE("Timer expired in 'LostFound' state for '%x'  on standby. Restart timer",
            fsm_->node_id_);

    // wait for checkpoint to transition state
    // meanwhile, restart timer in case a SC failover to this node occurs
    avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                  fsm_->cb_->node_failover_node_wait * SA_TIME_ONE_SECOND);
  }
}

void LostFound::MdsUp() {
  if (fsm_->Active() == true) {
    LOG_ER("unexpected MDS up event");
  }
}

void LostFound::MdsDown() {
  LOG_WA("unexpected MDS down event");
}

void LostFound::NodeUp() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  if (fsm_->Active() == true) {
    AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
    osafassert(node != nullptr);

    // don't call avd_node_delete_nodeid as the node is already up
    avd_node_failover(node);
    node->node_info.member = SA_TRUE;

    fsm_->SetState(std::make_shared<End>(fsm_));
  } else {
    // wait for checkpoint to transition state
    // we are standby and shouldn't get node up
    LOG_ER("unexpected node up event");
  }
}

// state 'LostRebooting'
LostRebooting::LostRebooting(NodeStateMachine *fsm) :
  NodeState(fsm) {
  avd_stop_tmr(fsm_->cb_, fsm_->timer_.get());
  avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                fsm_->cb_->node_failover_node_wait * SA_TIME_ONE_SECOND);
}

void LostRebooting::TimerExpired() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  if (fsm_->Active() == true) {
    AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
    osafassert(node != nullptr);

    LOG_NO("Completing delayed node failover for '%s'",
            node->node_name.c_str());
    avd_node_failover(node);

    fsm_->SetState(std::make_shared<End>(fsm_));
  } else {
    TRACE("Timer expired in 'LostRebooting' state for '%x'  on standby. Restart timer",
            fsm_->node_id_);

    // wait for checkpoint to transition state
    // meanwhile, restart timer in case a SC failover to this node occurs
    avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                  fsm_->cb_->node_failover_node_wait * SA_TIME_ONE_SECOND);
  }
}

void LostRebooting::MdsUp() {
  if (fsm_->Active() == true) {
    LOG_ER("unexpected MDS up event");
  }
}

void LostRebooting::MdsDown() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
  osafassert(node != nullptr);

  if (fsm_->Active() == true) {
    LOG_WA("Node '%s' is down. Failover its previous assignments",
            node->node_name.c_str());
    avd_node_failover(node);

    AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
    osafassert(node != nullptr);

    fsm_->SetState(std::make_shared<End>(fsm_));
  } else {
    // wait for checkpoint to transition state
  }
}

void LostRebooting::NodeUp() {
  LOG_ER("unexpected node up event");
}

// state 'Failed'

Failed::Failed(NodeStateMachine *fsm) :
  NodeState(fsm) {
  avd_stop_tmr(fsm_->cb_, fsm_->timer_.get());
}

void Failed::TimerExpired() {
  LOG_ER("unexpected timer event");
}

void Failed::MdsUp() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  // transition to 'FailedFound' state
  fsm_->SetState(std::make_shared<FailedFound>(fsm_));
}

void Failed::MdsDown() {
  LOG_WA("unexpected MDS down event");
}

void Failed::NodeUp() {
  LOG_WA("unexpected node up event");

  // transition to 'FailedFound' state anyway, as it's evident
  // the node is back up even though we missed MDS up
  fsm_->SetState(std::make_shared<FailedFound>(fsm_));
}

// state 'FailedFound'

FailedFound::FailedFound(NodeStateMachine *fsm) :
  NodeState(fsm) {
  avd_stop_tmr(fsm_->cb_, fsm_->timer_.get());

  // start timer2, wait for node up
  avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                fsm_->cb_->node_failover_node_wait * SA_TIME_ONE_SECOND);
}

void FailedFound::TimerExpired() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  AVD_AVND *node = avd_node_find_nodeid(fsm_->node_id_);
  osafassert(node != nullptr);

  LOG_WA("Failed node '%s' has reappeared after network separation",
          node->node_name.c_str());

  if (fsm_->Active() == true) {
    LOG_WA("Sending node reboot order");
    avd_d2n_reboot_snd(node);

    fsm_->SetState(std::make_shared<End>(fsm_));
  } else {
    TRACE("Timer expired in 'FailedFound' state for '%x'  on standby. Restart timer",
            fsm_->node_id_);

    // wait for checkpoint to transition state
    // meanwhile, restart timer in case a SC failover to this node occurs
    avd_start_tmr(fsm_->cb_, fsm_->timer_.get(),
                  fsm_->cb_->node_failover_node_wait * SA_TIME_ONE_SECOND);
  }
}

void FailedFound::MdsUp() {
  if (fsm_->Active() == true) {
    LOG_ER("unexpected MDS up event");
  }
}

void FailedFound::MdsDown() {
  LOG_WA("unexpected MDS down event");
}

void FailedFound::NodeUp() {
  TRACE_ENTER2("node_id %x", fsm_->node_id_);

  fsm_->SetState(std::make_shared<End>(fsm_));
}

// state 'End'

End::End(NodeStateMachine *fsm) :
  NodeState(fsm) {
  avd_stop_tmr(fsm_->cb_, fsm_->timer_.get());
}

void End::TimerExpired() {
  osafassert(false);
}

void End::MdsUp() {
  osafassert(false);
}

void End::MdsDown() {
  osafassert(false);
}

void End::NodeUp() {
  osafassert(false);
}
