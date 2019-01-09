#ifndef AMF_AMFD_NODE_STATE_H_
#define AMF_AMFD_NODE_STATE_H_

#include <stdint.h>
#include "base/macros.h"

class NodeStateMachine;

class NodeState {
 public:
  enum NodeStates : uint32_t {kUndefined = 0, kStart, kLost,
                              kLostFound, kLostRebooting,
                              kFailed, kFailedFound, kEnd};

  virtual void TimerExpired() =0;
  virtual void MdsUp() =0;
  virtual void MdsDown() =0;
  virtual void NodeUp() =0;
  virtual uint32_t GetInt() =0;
  NodeState(NodeStateMachine *fsm);
  virtual ~NodeState() {};

 protected:
  NodeStateMachine *fsm_;

 private:
  NodeState();
  DELETE_COPY_AND_MOVE_OPERATORS(NodeState);
};

class Start : public NodeState {
 public:
  Start(NodeStateMachine *fsm);
  virtual void TimerExpired();
  virtual void MdsUp();
  virtual void MdsDown();
  virtual void NodeUp();
  virtual uint32_t GetInt() {return kStart;}
};

class Lost : public NodeState {
 public:
  Lost(NodeStateMachine *fsm);
  virtual void TimerExpired();
  virtual void MdsUp();
  virtual void MdsDown();
  virtual void NodeUp();
  virtual uint32_t GetInt() {return kLost;}
};

class LostFound : public NodeState {
 public:
  LostFound(NodeStateMachine *fsm);
  virtual void TimerExpired();
  virtual void MdsUp();
  virtual void MdsDown();
  virtual void NodeUp();
  virtual uint32_t GetInt() {return kLostFound;}
};

class LostRebooting : public NodeState {
 public:
  LostRebooting(NodeStateMachine *fsm);
  virtual void TimerExpired();
  virtual void MdsUp();
  virtual void MdsDown();
  virtual void NodeUp();
  virtual uint32_t GetInt() {return kLostRebooting;}
};

class Failed : public NodeState {
 public:
  Failed(NodeStateMachine *fsm);
  virtual void TimerExpired();
  virtual void MdsUp();
  virtual void MdsDown();
  virtual void NodeUp();
  virtual uint32_t GetInt() {return kFailed;}
};

class FailedFound : public NodeState {
 public:
  FailedFound(NodeStateMachine *fsm);
  virtual void TimerExpired();
  virtual void MdsUp();
  virtual void MdsDown();
  virtual void NodeUp();
  virtual uint32_t GetInt() {return kFailedFound;}
};

class End : public NodeState {
 public:
  End(NodeStateMachine *fsm);
  virtual void TimerExpired();
  virtual void MdsUp();
  virtual void MdsDown();
  virtual void NodeUp();
  virtual uint32_t GetInt() {return kEnd;}
};

#endif // AMF_AMFD_NODE_STATE_H_
