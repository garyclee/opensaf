/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2017 - All Rights Reserved.
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

#ifndef SMF_SMFD_SMFADMINSTATE_H_
#define SMF_SMFD_SMFADMINSTATE_H_

#include <atomic>
#include <string>
#include <vector>
#include <list>

#include "ais/include/saAis.h"
#include "ais/include/saAmf.h"
#include "ais/include/saImmOm.h"
#include "base/macros.h"

#include "smf/smfd/SmfUpgradeStep.h"

//==============================================================================
// Operates on the units of an external unitNameAndState list that is provided
// when an object of this class is created. Nodes admin state is handled in
// parallel using a node group.
//
// The unit initial admin state in the provided list (initState) is saved when
// locking.
// When unlocking, unlock is done until initial admin state is reached if keep
// flag is true else end state will be unlocked regardless of initial state.
// Units handled can be Nodes or SUs
//
// All methods return bool false if the admin operation fail.
// Syslog is written to inform why the operation failed.
//
//==============================================================================
class SmfAdminStateHandler {
 public:
  explicit SmfAdminStateHandler(std::list<unitNameAndState>* i_allUnits,
                                SaUint32T smfKeepDuState,
                                SaTimeT adminOpTimeout);
  ~SmfAdminStateHandler();

  bool lock();
  bool lock_in();
  bool unlock_in();
  bool unlock();
  bool restart();

 private:
  bool initImmOmAndSetAdminOwnerName();
  bool becomeAdminOwnerOf(const std::string& object_name);
  bool releaseAdminOwnerOf(const std::string& object_name);
  void finalizeNodeGroupOm();
  bool isRestartError(SaAisErrorT ais_rc);

  // Result in m_nodeList and m_suList
  void createNodeAndSULockLists(SaAmfAdminStateT adminState);
  void createNodeAndSUUnlockLists(SaAmfAdminStateT adminState);
  void createUnitLists(SaAmfAdminStateT adminState, bool checkInitState);
  // Updates list pointed to by m_allUnits
  bool saveInitAndCurrentStateForAllUnits();
  bool saveCurrentStateForAllUnits();

  // Using m_nodeList
  bool createNodeGroup(SaAmfAdminStateT i_initState);

  bool deleteNodeGroup();

  bool changeAdminState(SaAmfAdminStateT fromState,
                      SaAmfAdminOperationIdT toState);
  bool adminOperationNodeGroup(SaAmfAdminStateT fromState,
                                 SaAmfAdminOperationIdT toState);
  // Using m_suList
  bool adminOperationParallel(SaAmfAdminOperationIdT adminState,
                                const std::list<unitNameAndState>& i_nodeList);
  bool nodeGroupAdminOperation(SaAmfAdminOperationIdT adminState);
  bool adminOperation(SaAmfAdminOperationIdT adminState,
                      const std::string& unitName);
  void adminOperationAsync(SaAmfAdminOperationIdT adminState,
                      const std::string& unitName);

  SaAmfAdminStateT getAdminState(const std::string& i_unit);
  bool setNodeGroupParentDn();

  std::list<unitNameAndState>* allUnits_;
  std::list<unitNameAndState> suList_;
  std::list<unitNameAndState> nodeList_;
  std::list<class SmfImmUtils*> adminAsyncList_;

  const SaVersionT immVersion_{'A', 2, 17};

  std::string nodeGroupParentDn_;

  SaImmHandleT omHandle_;
  SaImmAdminOwnerHandleT ownerHandle_;
  SaImmCcbHandleT ccbHandle_;
  SaImmAccessorHandleT accessorHandle_;
  // Indicate if creation was succesful
  bool creation_fail_;

  // This variable will contain the AIS return code after a calling a
  // method using OpenSAF API. If this variable does not contain
  // SA_AIS_OK the called method has failed.
  //
  SaAisErrorT errno_;

  // Count instances of this class
  static std::atomic<unsigned int> next_instance_number_;

  bool smfKeepDuState_;
  SaTimeT adminOpTimeout_;

  unsigned int instance_number_{0};
  std::string instanceNodeGroupName_;
  std::string admin_owner_name_;

  DELETE_COPY_AND_MOVE_OPERATORS(SmfAdminStateHandler);
};

#endif  // SMF_SMFD_SMFADMINSTATE_H_

