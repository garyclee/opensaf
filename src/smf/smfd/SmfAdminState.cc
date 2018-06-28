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

#include "smf/smfd/SmfAdminState.h"

#include <stdio.h>
#include <atomic>
#include <string>

#include "ais/include/saAis.h"
#include "ais/include/saAmf.h"
#include "ais/include/saImmOm.h"
#include "base/osaf_extended_name.h"
#include "base/saf_error.h"
#include "base/time.h"

#include "osaf/immutil/immutil.h"
#include "smf/smfd/smfd.h"
#include "smf/smfd/SmfUtils.h"

#include "smf/smfd/imm_modify_config/immccb.h"
#include "smf/smfd/SmfUpgradeStep.h"

//==============================================================================
// Class SmfAdminOperation methods
//==============================================================================

// Make m_next_instance_number thread safe
std::atomic<unsigned int> SmfAdminStateHandler::next_instance_number_{1};

SmfAdminStateHandler::SmfAdminStateHandler(std::list<unitNameAndState>*
                                           i_allUnits,
                                           SaUint32T smfKeepDuState,
                                           SaTimeT adminOpTimeout)
    : allUnits_(i_allUnits),
      omHandle_(0),
      ownerHandle_(0),
      ccbHandle_(0),
      accessorHandle_(0),
      creation_fail_(false),
      errno_(SA_AIS_OK),
      smfKeepDuState_(0),
      adminOpTimeout_(adminOpTimeout),
      instance_number_(1) {
  if (smfKeepDuState == 0) {
    smfKeepDuState_ = false;
  } else {
    smfKeepDuState_ = true;
  }

  // Create an instance unique name for a node group IMM object
  // using an instance number.
  instance_number_ = next_instance_number_++;
  instanceNodeGroupName_ =
      "safAmfNodeGroup=smfLockAdmNg" + std::to_string(instance_number_);
  admin_owner_name_ = "smfNgAdmOwnerName";
  TRACE("%s m_instanceNodeGroupName '%s'", __FUNCTION__,
        instanceNodeGroupName_.c_str());
  TRACE("%s m_NodeGroupAdminOwnerName '%s'", __FUNCTION__,
        admin_owner_name_.c_str());

  // Become admin owner using above created name
  bool rc = initImmOmAndSetAdminOwnerName();
  if (rc == false) {
    LOG_NO("%s: initAdminOwner() Fail", __FUNCTION__);
    creation_fail_ = true;
  }
}

SmfAdminStateHandler::~SmfAdminStateHandler() { finalizeNodeGroupOm(); }

///
/// Operates on the allUnits_ list
/// Using nodeList_ and suList_
/// - Save all units state before locking
/// - Lock all units with admin state unlocked
/// Return false if Fail
///
bool SmfAdminStateHandler::lock() {
  TRACE_ENTER();
  bool rc = false;

  if (creation_fail_) {
    goto done;
  }

  rc = saveInitAndCurrentStateForAllUnits();

  if (rc == false) {
    LOG_NO("%s: saveInitAndCurrentStateForAllUnits() Fail %s", __FUNCTION__,
           saf_error(errno_));
    goto done;
  }

  createNodeAndSULockLists(SA_AMF_ADMIN_UNLOCKED);

  rc = changeAdminState(SA_AMF_ADMIN_UNLOCKED, SA_AMF_ADMIN_LOCK);
  if (rc == false) {
    LOG_NO("%s setAdminState() Fail", __FUNCTION__);
  }

done:
  TRACE_LEAVE();
  return rc;
}

///
/// Using nodeList_ and suList_
/// Lock instantiate all units with admin state locked
/// Return false if Fail
///
bool SmfAdminStateHandler::lock_in() {
  TRACE_ENTER();
  bool rc = false;

  if (creation_fail_) {
    goto done;
  }

  rc = saveCurrentStateForAllUnits();

  if (rc == false) {
    LOG_NO("%s: saveCurrentStateForAllUnits() Fail %s", __FUNCTION__,
           saf_error(errno_));
    goto done;
  }

  createNodeAndSULockLists(SA_AMF_ADMIN_LOCKED);

  rc = changeAdminState(SA_AMF_ADMIN_LOCKED, SA_AMF_ADMIN_LOCK_INSTANTIATION);
  if (rc == false) {
    LOG_NO("%s setAdminState() Fail", __FUNCTION__);
  }

done:
  TRACE_LEAVE();
  return rc;
}

///
/// Using nodeList_ and suList_
/// Unlock instantiate all units with admin state locked in
/// Return false if Fail
///
bool SmfAdminStateHandler::unlock_in() {
  TRACE_ENTER();
  bool rc = false;

  if (creation_fail_) {
    goto done;
  }

  rc = saveCurrentStateForAllUnits();

  if (rc == false) {
    LOG_NO("%s: saveCurrentStateForAllUnits() Fail %s", __FUNCTION__,
           saf_error(errno_));
    goto done;
  }

  createNodeAndSUUnlockLists(SA_AMF_ADMIN_LOCKED_INSTANTIATION);

  // Note: This actually change state to SA_AMF_ADMIN_LOCKED
  //       The admin operation is SA_AMF_ADMIN_UNLOCK_INSTANTIATION but
  //       there is no SA_AMF_ADMIN_UNLOCK_INSTANTIATION state
  rc = changeAdminState(SA_AMF_ADMIN_LOCKED_INSTANTIATION,
                     SA_AMF_ADMIN_UNLOCK_INSTANTIATION);
  if (rc == false) {
    LOG_NO("%s setAdminState() Fail", __FUNCTION__);
  }

done:
  TRACE_LEAVE();
  return rc;
}

///
/// Using nodeList_ and suList_
/// Unlock all units with admin state unlocked in
/// Return false if Fail
///
bool SmfAdminStateHandler::unlock() {
  TRACE_ENTER();
  bool rc = false;

  if (creation_fail_) {
    goto done;
  }

  rc = saveCurrentStateForAllUnits();

  if (rc == false) {
    LOG_NO("%s: saveCurrentStateForAllUnits() Fail %s", __FUNCTION__,
           saf_error(errno_));
    goto done;
  }

  createNodeAndSUUnlockLists(SA_AMF_ADMIN_LOCKED);

  rc = changeAdminState(SA_AMF_ADMIN_LOCKED, SA_AMF_ADMIN_UNLOCK);
  if (rc == false) {
    LOG_NO("%s changeAdminState() Fail", __FUNCTION__);
  }

done:
  TRACE_LEAVE();
  return rc;
}

/// Try to restart all units in the allUnits_ list.
/// Return false if Fail
///
bool SmfAdminStateHandler::restart() {
  TRACE_ENTER();
  bool rc = false;

  if (creation_fail_) {
    goto done;
  }

  errno_ = SA_AIS_OK;
  for (auto &unit : *allUnits_) {
    if (unit.name.find("safComp") != std::string::npos) {
      // Only if the unit is a component
      adminOperation(SA_AMF_ADMIN_RESTART, unit.name);
      rc = isRestartError(errno_);
      TRACE("\tais_rc '%s'", saf_error(errno_));
      if (rc == false) break;
    }
  }

done:
  TRACE_LEAVE();
  return rc;
}

/// -----------------------------------------------
/// SmfAdminStateHandler Private
/// -----------------------------------------------

// Initialize an OM (get an OM handle) and become admin owner with
// admin owner name set to the SmfAdminOperation admin owner name
// Note: Does not set admin ownership for any object
//       Use becomeAdminOwnerOf(object) and releaseAdminOwnerOf(object)
//
//
bool SmfAdminStateHandler::initImmOmAndSetAdminOwnerName() {
  SaAisErrorT ais_rc = SA_AIS_ERR_TRY_AGAIN;
  SaVersionT local_version = immVersion_;
  int timeout_try_cnt = 6;
  bool rc = true;

  // OM handle
  while (timeout_try_cnt > 0) {
    ais_rc = immutil_saImmOmInitialize(&omHandle_, NULL, &local_version);
    if (ais_rc != SA_AIS_ERR_TIMEOUT) break;
    timeout_try_cnt--;
    local_version = immVersion_;
  }
  if (ais_rc != SA_AIS_OK) {
    LOG_NO("%s: saImmOmInitialize Fail %s", __FUNCTION__, saf_error(ais_rc));
    rc = false;
  }

  // Admin owner handle and set Admin owner name
  if (rc == true) {
    timeout_try_cnt = 6;
    while (timeout_try_cnt > 0) {
      ais_rc = immutil_saImmOmAdminOwnerInitialize(
          omHandle_, const_cast<char *>(admin_owner_name_.c_str()), SA_TRUE,
          &ownerHandle_);
      if (ais_rc != SA_AIS_ERR_TIMEOUT) break;
      timeout_try_cnt--;
    }
    if (ais_rc != SA_AIS_OK) {
      LOG_NO("%s: saImmOmAdminOwnerInitialize Fail %s", __FUNCTION__,
             saf_error(ais_rc));
      rc = false;
    }
  }

    return rc;
}

// Become admin owner of one object
// Note: The admin owner handle must have been initialized.
//       See initImmOmAndSetAdminOwnerName()
// Return false if Fail
bool SmfAdminStateHandler::becomeAdminOwnerOf(const std::string& object_name) {
  TRACE_ENTER();
  SaNameT object_for_admin_ownership;
  saAisNameLend(object_name.c_str(), &object_for_admin_ownership);
  const SaNameT *objectNames[2] = {&object_for_admin_ownership, NULL};

  // We are taking admin owner on the parent DN of this object. This can
  // be conflicting with other threads which also want to create objects.
  // Specifically SmfImmOperation takes admin owner when creating IMM
  // objects. Retry if object has admin owner already.
  int timeout_try_cnt = 6;
  SaAisErrorT ais_rc = SA_AIS_OK;
  while (timeout_try_cnt > 0) {
    TRACE("%s: calling adminOwnerSet", __FUNCTION__);
    ais_rc = immutil_saImmOmAdminOwnerSet(ownerHandle_, objectNames,
                                          SA_IMM_SUBTREE);
    if (ais_rc != SA_AIS_ERR_EXIST) break;

    timeout_try_cnt--;
    TRACE(
        "%s: saImmOmAdminOwnerSet returned SA_AIS_ERR_EXIST, "
        "sleep 1 second and retry",
        __FUNCTION__);
    osaf_nanosleep(&kOneSecond);
  }

  bool rc = true;
  if (ais_rc != SA_AIS_OK) {
    LOG_NO("%s: saImmOmAdminOwnerSet owner name '%s' Fail '%s'", __FUNCTION__,
           admin_owner_name_.c_str(), saf_error(ais_rc));
    rc = false;
  }

  TRACE_LEAVE();
  return rc;
}

// Release admin ownership of one object
// Note: The admin owner handle must have been initialized.
//       See initImmOmAndSetAdminOwnerName()
// Return false if Fail
bool SmfAdminStateHandler::releaseAdminOwnerOf(const std::string& object_name) {
  TRACE_ENTER();
  SaNameT object_for_admin_ownership;
  bool rc = true;

  saAisNameLend(object_name.c_str(), &object_for_admin_ownership);
  const SaNameT *objectNames[2] = {&object_for_admin_ownership, NULL};

  SaAisErrorT ais_rc = immutil_saImmOmAdminOwnerRelease(ownerHandle_,
                                                        objectNames,
                                                        SA_IMM_SUBTREE);
  if (ais_rc != SA_AIS_OK) {
    LOG_NO("saImmOmAdminOwnerRelease Fail '%s'",
           saf_error(ais_rc));
    rc = false;
  }

  TRACE_LEAVE();
  return rc;
}

// Free all IMM handles that's based on the omHandle and
// give up admin ownership of Amf Cluster object
void SmfAdminStateHandler::finalizeNodeGroupOm() {
  TRACE_ENTER();
  if (omHandle_ != 0) {
    (void)immutil_saImmOmFinalize(omHandle_);
    omHandle_ = 0;
  }
  TRACE_LEAVE();
}

/// Check if the AIS return is considered a campaign error
/// Do not Fail if any of the following AIS codes
///
bool SmfAdminStateHandler::isRestartError(SaAisErrorT ais_rc) {
  TRACE_ENTER();
  bool rc = true;

  if (ais_rc != SA_AIS_OK && ais_rc != SA_AIS_ERR_BAD_OPERATION) {
    rc = false;
  }

  TRACE_LEAVE();
  return rc;
}

/// Return false if Fail. errno_ is set
/// Only SU and Node
///
bool SmfAdminStateHandler::saveInitAndCurrentStateForAllUnits() {
  TRACE_ENTER();
  bool rc = true;

  for (auto &unit : *allUnits_) {
    if (unit.name.find("safComp") != std::string::npos) {
      // A component has no admin state
      continue;
    }
    unit.initState = getAdminState(unit.name);
    unit.currentState = unit.initState;
    if (errno_ != SA_AIS_OK) {
      LOG_NO("%s: getAdminStateForUnit() Fail %s", __FUNCTION__,
             saf_error(errno_));
      rc = false;
      break;
    }
  }

  TRACE_LEAVE();
  return rc;
}

/// Return false if Fail. errno_ is set
/// Only SU and Node
///
bool SmfAdminStateHandler::saveCurrentStateForAllUnits() {
  TRACE_ENTER();
  bool rc = true;

  for (auto &unit : *allUnits_) {
    if (unit.name.find("safComp") != std::string::npos) {
      // A component has no admin state
      continue;
    }
    unit.currentState = getAdminState(unit.name);
    if (errno_ != SA_AIS_OK) {
      LOG_NO("%s: getAdminStateForUnit() Fail %s", __FUNCTION__,
             saf_error(errno_));
      rc = false;
      break;
    }
  }

  TRACE_LEAVE();
  return rc;
}

/// From the allUnits_ list create a list of SUs and a
/// list of Nodes that has given admin state
/// of the list
///
void SmfAdminStateHandler::createNodeAndSULockLists(SaAmfAdminStateT
                                                    adminState) {
  createUnitLists(adminState, false);
}

/// From the allUnits_ list create a list of SUs and a
/// list of Nodes that has given admin state
/// If smfKeepDuState set then nodes that already has the init state shall not
/// be in list
///
void SmfAdminStateHandler::createNodeAndSUUnlockLists(
    SaAmfAdminStateT adminState) {
  createUnitLists(adminState, smfKeepDuState_);
}

/// From the allUnits_ list create a list of SUs and a
/// list of Nodes that has given admin state
/// If checkInitState is true then nodes  and SUs that are in their init state
/// shall not be part of the list
///
void SmfAdminStateHandler::createUnitLists(SaAmfAdminStateT adminState,
                                        bool checkInitState) {
  TRACE_ENTER();
  // The lists are reused. Clean before creating new lists
  suList_.clear();
  nodeList_.clear();

  for (auto &unit : *allUnits_) {
    if (checkInitState == true) {
      if (unit.currentState == unit.initState) continue;
    }
    if (unit.name.find("safSu") != std::string::npos) {
      if (unit.currentState == adminState) suList_.push_back(unit);
    } else if (unit.name.find("safAmfNode") != std::string::npos) {
      if (unit.currentState == adminState) nodeList_.push_back(unit);
    } else if (unit.name.find("safComp") != std::string::npos) {
      // Shall not be added to list
      TRACE("safComp found in list");
    } else {
      // Should never happen
      LOG_ER("%s, [%d] Unknown unit %s", __FUNCTION__, __LINE__,
             unit.name.c_str());
      abort();
    }
  }
  TRACE_LEAVE();
}

// Set given admin state using a node group or serialized:
// - For units in m_suList serialized is used
// - For units in m_nodeList:
//   - One node in list use serialized
//   - More than one node use node group
bool SmfAdminStateHandler::changeAdminState(SaAmfAdminStateT fromState,
                                      SaAmfAdminOperationIdT toState) {
  bool rc = true;
  TRACE_ENTER();

  // Set admin state for nodes
  if (nodeList_.size() > 1) {
    TRACE("%s: Use node group", __FUNCTION__);
    rc = adminOperationNodeGroup(fromState, toState);
    if (rc == false) {
      LOG_NO("%s: changeNodeGroupAdminState() Fail %s", __FUNCTION__,
             saf_error(errno_));
    }
  } else if (nodeList_.size() == 1) {
    TRACE("%s: Use serialized for one node", __FUNCTION__);
    rc = adminOperationSerialized(toState, nodeList_);
    if (rc == false) {
      LOG_NO("%s: setAdminStateNode() Fail %s", __FUNCTION__,
             saf_error(errno_));
    }
  }

  // Set admin state for SUs
  if ((rc == true) && (!suList_.empty())) {
    TRACE("%s: Use serialized for SUs", __FUNCTION__);
    // Do only if setting admin state for nodes did not fail
    rc = adminOperationSerialized(toState, suList_);
    if (rc == false) {
      LOG_NO("%s: setAdminStateSUs() Fail %s", __FUNCTION__,
             saf_error(errno_));
    }
  }

  TRACE_LEAVE();
  return rc;
}

// Set given admin state in the nodeList_ using a node group
// Return false if Fail. errno_ is set
//
bool SmfAdminStateHandler::adminOperationNodeGroup(
    SaAmfAdminStateT fromState, SaAmfAdminOperationIdT toState) {
  bool rc = true;
  SaAisErrorT ais_errno = SA_AIS_OK;

  TRACE_ENTER();

  if (!nodeList_.empty()) {
    rc = createNodeGroup(fromState);
    if (rc == false) {
      ais_errno = errno_;
      LOG_NO("%s: createNodeGroup() Fail", __FUNCTION__);
    }
    if (rc == true) {
      rc = nodeGroupAdminOperation(toState);
      if (rc == false) {
        ais_errno = errno_;
        LOG_NO("%s: setNodeGroupAdminState() Fail %s", __FUNCTION__,
               saf_error(ais_errno));
      }
      if (deleteNodeGroup() == false) {
        LOG_NO("%s: deleteNodeGroup(), Fail. "
            "Changing state did not fail. %s Return code is not set to Fail",
               __FUNCTION__, __FUNCTION__);
      }
    }
  } else {
    TRACE("\t nodeList_ is empty!");
  }

  errno_ = ais_errno;

  TRACE_LEAVE();
  return rc;
}

// Set given admin state to all units in the given unitList
// Return false if Fail. errno_ is set
//
bool SmfAdminStateHandler::adminOperationSerialized(
    SaAmfAdminOperationIdT adminState,
    const std::list<unitNameAndState> &i_unitList) {
  bool rc = true;
  errno_ = SA_AIS_OK;

  TRACE_ENTER();

  if (!i_unitList.empty()) {
    for (auto &unit : i_unitList) {
      rc = adminOperation(adminState, unit.name);
      if (rc == false) {
        // Failed to set admin state
        break;
      }
    }
  }

  TRACE_LEAVE();
  return rc;
}

/// Read current state from the IMM object in the given object name (dn)
/// If the unit is a node or an SU, admin state is fetched.
/// errno_ is set
/// NOTE: The return value is undefined if errno_ != SA_AIS_OK
///
SaAmfAdminStateT SmfAdminStateHandler::getAdminState(
    const std::string &i_objectName) {
  TRACE_ENTER();

  // Holds the returned value
  SaImmAttrValuesT_2 **attributes;
  // Gives undefined return value
  SaAmfAdminStateT *p_adminState = nullptr;

  errno_ = SA_AIS_OK;

  // Get accessor handle
  int timeout_try_cnt = 6;
  SaAisErrorT ais_rc = SA_AIS_OK;
  while (timeout_try_cnt > 0) {
    ais_rc = immutil_saImmOmAccessorInitialize(omHandle_, &accessorHandle_);
    if (ais_rc != SA_AIS_ERR_TIMEOUT) break;
    timeout_try_cnt--;
  }
  if (ais_rc != SA_AIS_OK) {
    LOG_NO("%s: saImmOmAccessorInitialize Fail %s", __FUNCTION__,
           saf_error(ais_rc));
  }

  // IMM object from i_unit.name
  SaNameT objectName;
  saAisNameLend(i_objectName.c_str(), &objectName);

  // Admin state shall be read from a SU object or a Node object
  if (i_objectName.find("safSu") != std::string::npos) {
    SaImmAttrNameT suAdminStateAttr[] = {
        const_cast<char *>("saAmfSUAdminState"), NULL};
    errno_ = immutil_saImmOmAccessorGet_2(accessorHandle_, &objectName,
                                           suAdminStateAttr, &attributes);
  } else if (i_objectName.find("safAmfNode") != std::string::npos) {
    SaImmAttrNameT nodeAdminStateAttr[] = {
        const_cast<char *>("saAmfNodeAdminState"), NULL};
    errno_ = immutil_saImmOmAccessorGet_2(accessorHandle_, &objectName,
                                           nodeAdminStateAttr, &attributes);
  } else {
    // Should never happen
    TRACE("\t i_objectName '%s'", i_objectName.c_str());
    TRACE("\t Not containing 'safSu' or 'safAmfNode'?");
    LOG_ER("%s, [%d] Unknown object %s", __FUNCTION__, __LINE__,
           i_objectName.c_str());
    abort();
  }

  if (errno_ == SA_AIS_OK) {
    SaImmAttrValuesT_2 *attribute = attributes[0];
    if (attribute->attrValuesNumber != 0) {
      p_adminState = static_cast<SaAmfAdminStateT *>(attribute->attrValues[0]);
    } else {
      // Should never happen
      LOG_ER("%s: [%d] No Admin State value found", __FUNCTION__, __LINE__);
      abort();
    }
  } else {
    LOG_NO("%s saImmOmAccessorGet_2 Fail %s", __FUNCTION__, saf_error(errno_));
  }

  SaAmfAdminStateT adminState = SA_AMF_ADMIN_UNLOCKED;
  if (p_adminState != nullptr) adminState = *p_adminState;

  TRACE_LEAVE();
  return adminState;
}

/// Set safAmfCluster to be parent DN for the Node group used for setting admin
/// state. Fetched from the SaAmfCluster class
/// Return false if Fail. errno_ is set
///
bool SmfAdminStateHandler::setNodeGroupParentDn() {
  bool rc = true;
  errno_ = SA_AIS_OK;

  TRACE_ENTER();

  SaImmSearchHandleT searchHandle;
  SaImmAttrValuesT_2 **attributes;  // Not used. Wanted info in objectName
  SaNameT objectName;               // Out data from IMM search.

  const char *className = "SaAmfCluster";
  SaImmSearchParametersT_2 searchParam;
  searchParam.searchOneAttr.attrName = const_cast<char *>("SaImmAttrClassName");
  searchParam.searchOneAttr.attrValueType = SA_IMM_ATTR_SASTRINGT;
  searchParam.searchOneAttr.attrValue = &className;  // void *

  errno_ = immutil_saImmOmSearchInitialize_2(
      omHandle_, NULL, SA_IMM_SUBTREE,
      SA_IMM_SEARCH_ONE_ATTR | SA_IMM_SEARCH_GET_NO_ATTR, &searchParam, NULL,
      &searchHandle);
  if (errno_ != SA_AIS_OK) {
    LOG_NO("%s: saImmOmSearchInitialize_2 Fail %s", __FUNCTION__,
           saf_error(errno_));
    rc = false;
    goto done;
  }

  errno_ = immutil_saImmOmSearchNext_2(searchHandle, &objectName, &attributes);
  if (errno_ != SA_AIS_OK) {
    LOG_NO("%s: saImmOmSearchNext_2 Fail %s", __FUNCTION__, saf_error(errno_));
    rc = false;
  }

  errno_ = immutil_saImmOmSearchFinalize(searchHandle);
  if (errno_ != SA_AIS_OK) {
    LOG_NO("%s: saImmOmFinalize Fail %s", __FUNCTION__, saf_error(errno_));
    rc = false;
  }

  if (rc != false) {
    const char *tmp_str = saAisNameBorrow(&objectName);
    nodeGroupParentDn_ = tmp_str;
    TRACE("\t Object name is '%s'", tmp_str);
  }

  TRACE_LEAVE2("nodeGroupParentDn_ '%s', rc = %s",
               nodeGroupParentDn_.c_str(), rc == true? "true": "false");
done:
  return rc;
}

/// Create a SmfAdminStateHandler instance node group with the nodes in
/// the nodeList.
/// The saAmfNGAdminState attribute will be given fromState. The initState
/// must be set to a state so that a state change to the wanted state for the
/// nodes can be done.
/// Return false if Fail. Note: errno_ is not set
///
bool SmfAdminStateHandler::createNodeGroup(SaAmfAdminStateT i_fromState) {
  TRACE_ENTER();

  errno_ = SA_AIS_OK;  // Dummy; Not used

  TRACE("%s: unique Node name '%s'", __FUNCTION__,
        instanceNodeGroupName_.c_str());

  // --------------------------------------
  // Attribute descriptor for the node name
  // Attribute: safAmfNodeGroup
  //
  modelmodify::AttributeDescriptor nodegroup_name;
  nodegroup_name.attribute_name = "safAmfNodeGroup";
  nodegroup_name.value_type = SA_IMM_ATTR_SASTRINGT;
  nodegroup_name.AddValue(instanceNodeGroupName_);

  // ---------------------------------------
  // Attribute descriptor for the init state
  // Attribute: saAmfNGAdminState
  //
  modelmodify::AttributeDescriptor init_state;
  init_state.attribute_name = "saAmfNGAdminState";
  init_state.value_type = SA_IMM_ATTR_SAUINT32T;
  init_state.AddValue(std::to_string(i_fromState));

  // ------------------------------------
  // Attribute descriptor for node list (list of nodes in node group)
  // Attrubute: saAmfNGNodeList (multi value))
  //
  modelmodify::AttributeDescriptor node_list;
  node_list.attribute_name = "saAmfNGNodeList";
  node_list.value_type = SA_IMM_ATTR_SANAMET;
  for (auto &node : nodeList_) {
    node_list.AddValue(node.name);
  }

  // ------------------------------------
  // Get Parent DN for node groups
  // This could be moved to some init handling since it only has to be done
  // once. The parent is always the same
  bool rc = true;
  if (nodeGroupParentDn_.empty()) {
    if (setNodeGroupParentDn() == false) {
      LOG_NO("%s: setNodeGroupParentDn() Fail", __FUNCTION__);
      rc = false;
    }
  }

  if (rc == true) {
    // ------------------------------------
    // Create descriptor for the node group
    modelmodify::CreateDescriptor ng_creator;
    ng_creator.class_name = "SaAmfNodeGroup";
    ng_creator.parent_name = nodeGroupParentDn_;
    ng_creator.AddAttribute(nodegroup_name);
    ng_creator.AddAttribute(init_state);
    ng_creator.AddAttribute(node_list);

    // ----------------------------------------
    // CCB descriptor for creating a node group
    modelmodify::CcbDescriptor ng_create_ccb;
    ng_create_ccb.AddCreate(ng_creator);

    // ----------------------------------------------------------
    // Must handle a situation where a node group from an earlier
    // failed upgrade exist as a "stray" object
    // This is handled by just try to delete the node group we are
    // going to create. If it does exist it will be deleted
    // Note: For this function to work nodeGroupParentDn_ must have
    //       a value so dont move this section before the
    //       "Get Parent DN for node groups" section
    if (deleteNodeGroup() == false) {
      TRACE("%s: deleteNodeGroup() Fail. Ok group probably not exist",
            __FUNCTION__);
    } else {
      TRACE("%s: deleteNodeGroup() Deleted! The node group we are "
            "about to create already existed but is now deleted", __FUNCTION__);
    }

    // ---------------
    // Do the creation
    modelmodify::ModelModification model_modify;
    if (model_modify.DoModelModification(ng_create_ccb) == false) {
      LOG_NO("%s: DoModelModification() Fail", __FUNCTION__);
      rc = false;
    }
  }

  TRACE_LEAVE();
  return rc;
}

/// Delete the SmfAdminStateHandler instance specific node group
/// Return false if Fail. errno_ is NOT set
///
bool SmfAdminStateHandler::deleteNodeGroup() {
  TRACE_ENTER();
  errno_ = SA_AIS_OK;  // Dummy; Not used

  std::string nodeGroupName =
      instanceNodeGroupName_ + "," + nodeGroupParentDn_;

  modelmodify::DeleteDescriptor ng_delete;
  ng_delete.object_name = nodeGroupName;

  modelmodify::CcbDescriptor ng_delete_ccb;
  ng_delete_ccb.AddDelete(ng_delete);

  modelmodify::ModelModification model_modify;
  bool rc = true;
  if (model_modify.DoModelModification(ng_delete_ccb) == false) {
    LOG_NO("%s: DoModelModification() Fail", __FUNCTION__);
    rc = false;
  }

  return rc;
}

/// Request given admin operation to the SmfAdminStateHandler instance node group
/// Return false if Fail. errno_ is set
///
bool SmfAdminStateHandler::nodeGroupAdminOperation(
    SaAmfAdminOperationIdT adminOp) {

  bool method_rc = true;

  TRACE_ENTER();

  // Create the object name
  std::string nodeGroupName_s =
      instanceNodeGroupName_ + "," + nodeGroupParentDn_;
  SaNameT nodeGroupName;
  osaf_extended_name_lend(nodeGroupName_s.c_str(), &nodeGroupName);

  TRACE("\t Setting '%s' to admin state %d", nodeGroupName_s.c_str(), adminOp);

  // There are no operation parameters
  const SaImmAdminOperationParamsT_2 *params[1] = {NULL};

  // Become admin ownership of the node group
  bool admset_rc = becomeAdminOwnerOf(nodeGroupName_s);

  if (admset_rc == true) {
    // ===================================
    // Invoke admin operation
    const SaTimeT kNanoMillis = 1000000;
    SaAisErrorT oi_rc = SA_AIS_OK;
    SaAisErrorT imm_rc = SA_AIS_OK;
    errno_ = SA_AIS_OK;
    base::Timer adminOpTimer(smfd_cb->adminOpTimeout / kNanoMillis);

    while (adminOpTimer.is_timeout() == false) {
      TRACE("%s: saImmOmAdminOperationInvoke_2 time left = %" PRIu64,
            __FUNCTION__, adminOpTimer.time_left());
      imm_rc =
          saImmOmAdminOperationInvoke_2(ownerHandle_, &nodeGroupName, 0,
                                        adminOp, params, &oi_rc,
                                        smfd_cb->adminOpTimeout);
      if ((imm_rc == SA_AIS_ERR_TRY_AGAIN) ||
          (imm_rc == SA_AIS_OK && oi_rc == SA_AIS_ERR_TRY_AGAIN)) {
        base::Sleep(base::MillisToTimespec(2000));
        continue;
      } else if (imm_rc != SA_AIS_OK) {
        LOG_NO(
            "%s: saImmOmAdminOperationInvoke_2 Fail %s",
            __FUNCTION__, saf_error(imm_rc));
        errno_ = imm_rc;
        break;
      } else if (oi_rc != SA_AIS_OK) {
        LOG_NO("%s: SaAmfAdminOperationId %d Fail %s", __FUNCTION__, adminOp,
               saf_error(oi_rc));
        errno_ = oi_rc;
        break;
      } else {
        // Operation success
        method_rc = true;
        break;
      }
    }
    if (adminOpTimer.is_timeout()) {
      if ((imm_rc == SA_AIS_OK) && (oi_rc == SA_AIS_OK)) {
        // Timeout is passed but operation is ok. This is Ok
        method_rc = true;
      } else if (imm_rc != SA_AIS_OK) {
        LOG_NO("%s adminOpTimeout Fail %s", __FUNCTION__, saf_error(imm_rc));
        errno_ = imm_rc;
        method_rc = false;
      } else {
        LOG_NO("%s adminOpTimeout Fail %s", __FUNCTION__, saf_error(oi_rc));
        errno_ = oi_rc;
        method_rc = false;
      }
    }
  } else {
    LOG_NO("%s: becomeAdminOwnerOf(%s) Fail", __FUNCTION__,
           nodeGroupName_s.c_str());
    method_rc = false;
  }

  if (method_rc == true) {
    TRACE("%s Admin operation is done. Release ownership if nodegroup",
          __FUNCTION__);
    if (releaseAdminOwnerOf(nodeGroupName_s) == false) {
      LOG_NO("%s: releaseAdminOwnerOf(%s) Fail", __FUNCTION__,
             nodeGroupName_s.c_str());
    }
  }


  TRACE_LEAVE2("%s", method_rc ? "OK" : "FAIL");
  return method_rc;
}

/// Set given admin state to one unit
/// Return false if Fail. errno_ is set
///
bool SmfAdminStateHandler::adminOperation(SaAmfAdminOperationIdT adminOperation,
                                       const std::string &unitName) {
  bool rc = true;
  errno_ = SA_AIS_OK;

  TRACE_ENTER();

  SmfImmUtils immUtil;
  const SaImmAdminOperationParamsT_2 *params[1] = {NULL};

  TRACE("\t Unit name '%s', adminOperation=%d", unitName.c_str(),
        adminOperation);
  errno_ = immUtil.callAdminOperation(unitName, adminOperation, params,
                                       smfd_cb->adminOpTimeout);

  if (errno_ != SA_AIS_OK) {
    LOG_NO(
        "%s: immUtil.callAdminOperation() Fail %s, Failed unit is '%s'",
        __FUNCTION__, saf_error(errno_), unitName.c_str());
    rc = false;
  }

  TRACE_LEAVE2("%s", rc ? "OK" : "FAIL");
  return rc;
}
