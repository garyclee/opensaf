/*
 *
 * (C) Copyright 2009 The OpenSAF Foundation
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

#include "SmfExecControlHdl.h"

#include <string>
#include <vector>

#include "base/osaf_extended_name.h"
#include <saAis.h>
#include "base/saf_error.h"
#include "osaf/immutil/immutil.h"
#include "smf/smfd/SmfUtils.h"
#include "smf/smfd/SmfCampaign.h"
#include "smf/smfd/imm_modify_config/immccb.h"

// Note: Info about public methods can be found in .h file

SmfExecControlObjHandler::SmfExecControlObjHandler()
    : m_smfProtectExecControlDuringInit(0),
      m_smfProtectExecControlDuringInit_valid(false),
      m_procExecMode(0),
      m_procExecMode_valid(false),
      m_numberOfSingleSteps(0),
      m_numberOfSingleSteps_valid(false),
      m_nodesForSingleStep_valid(false),
      m_attributes(0) {
  p_immutil_object = new SmfImmUtils;  // Deleted by uninstall and in destructor
}

SmfExecControlObjHandler::~SmfExecControlObjHandler() {
  // Note 1:
  // The exec control object copy cannot be removed here since it may
  // be needed after restart or SI-swap. Use uninstall() to cleanup and remove
  // the exec control object copy.
  // Note 2:
  // If the exec control object copy is not removed it will remain as a "stray"
  // IMM object, however this is not a resource leek since it will be
  // overwritten the next time a copy has to be created
  //
  TRACE_ENTER();
  if (p_immutil_object != NULL) {
    delete p_immutil_object;
    p_immutil_object = NULL;
  }
  TRACE_LEAVE();
}

bool SmfExecControlObjHandler::install() {
  TRACE_ENTER();

  // Read smf configuration data needed for handling exec control
  //
  if (readOpenSafSmfConfig() == false) {
    LOG_NO("%s: readOpenSafSmfConfig Fail", __FUNCTION__);
    return false;
  }

  // Read the original exec control object and save attribute values in member
  // variables and handle valid flags.
  //
  const char *exec_ctrl_name = c_openSafSmfExecControl;
  if (readExecControlObject(exec_ctrl_name) == false) {
    LOG_NO("%s: readExecControlObject Fail", __FUNCTION__);
    return false;
  }

  // Create a copy of the exec control object. If already exist it's overwritten
  removeExecControlObjectCopy();
  if (copyExecControlObject() == false) {
    LOG_NO("%s: copyExecControlObject Fail", __FUNCTION__);
    return false;
  }

  TRACE_LEAVE();
  return true;
}

void SmfExecControlObjHandler::uninstall() {
  TRACE_ENTER();
  if (p_immutil_object != NULL) {
    delete p_immutil_object;
    p_immutil_object = NULL;
  }
  removeExecControlObjectCopy();
  TRACE_LEAVE();
}

SaUint32T SmfExecControlObjHandler::procExecMode(bool *errinfo) {
  TRACE_ENTER();
  bool i_errinfo = true;  // No error

  if (m_procExecMode_valid == false) {
    TRACE("%s has no valid value, try to read the IMM copy", __FUNCTION__);
    i_errinfo = getValuesFromImmCopy();
  }

  if (errinfo != NULL) {
    *errinfo = i_errinfo;
  }

  TRACE_LEAVE();
  return m_procExecMode;
}

SaUint32T SmfExecControlObjHandler::numberOfSingleSteps(bool *errinfo) {
  TRACE_ENTER();
  bool i_errinfo = true;  // No error

  if (m_numberOfSingleSteps_valid == false) {
    TRACE("%s has no valid value, try to read the IMM copy", __FUNCTION__);
    i_errinfo = getValuesFromImmCopy();
  }

  if (errinfo != NULL) {
    *errinfo = i_errinfo;
  }

  TRACE_LEAVE();
  return m_numberOfSingleSteps;
}

std::vector<std::string> SmfExecControlObjHandler::nodesForSingleStep(
    bool *errinfo) {
  TRACE_ENTER();
  bool i_errinfo = true;  // No error

  if (m_nodesForSingleStep_valid == false) {
    TRACE("%s has no valid value, try to read the IMM copy", __FUNCTION__);
    i_errinfo = getValuesFromImmCopy();
  }

  if (errinfo != NULL) {
    *errinfo = i_errinfo;
  }

  TRACE_LEAVE();
  return m_nodesForSingleStep;
}

bool SmfExecControlObjHandler::smfProtectExecControlDuringInit(
    bool *errinfo = NULL) {
  TRACE_ENTER();
  bool i_errinfo = true;

  if (m_smfProtectExecControlDuringInit_valid == false) {
    i_errinfo = readOpenSafSmfConfig();
  }

  if (errinfo != NULL) {
    *errinfo = i_errinfo;
  }

  if (m_smfProtectExecControlDuringInit > 0) {
    return true;
  } else {
    return false;
  }
}

/**
 * Read all values from the exec control object copy if one exist
 *
 * @return
 */
bool SmfExecControlObjHandler::getValuesFromImmCopy() {
  TRACE_ENTER();
  bool errinfo = true;

  std::string copydn = kOpenSafSmfExecControl_copy;
  SaImmAttrValuesT_2 **attributes;
  if (!p_immutil_object->getObject(copydn, &attributes)) {
    // We do not have a copy so create it. This can happen when upgrading from
    // an earlier version of SMF. SMF will only read the copy when it has been
    // restarted.
    LOG_NO("No copy existing for execControl, read execControl and create it");
    if (!install()) {
      errinfo = false;
    }
  }

  if (readExecControlObject(kOpenSafSmfExecControl_copy) == false) {
    LOG_NO("%s readExecControlObject(c_openSafSmfExecControl_copy) Fail",
           __FUNCTION__);
    errinfo = false;
  }

  TRACE_LEAVE();
  return errinfo;
}

/**
 * Read the exec control object and save its values into member variables
 * The attribute descriptors are saved so that a copy can be made
 *
 * @return false on Fail
 */
bool SmfExecControlObjHandler::readExecControlObject(
    const std::string& exec_ctrl_name) {
  TRACE_ENTER();

  TRACE("%s: Name of exec control object '%s'", __FUNCTION__,
        exec_ctrl_name.c_str());

  // Get all attributes of the exec control object
  if (p_immutil_object->getObject(exec_ctrl_name, &m_attributes) == false) {
    TRACE("%s: Failed to get object from attribute %s", __FUNCTION__,
          OPENSAF_SMF_EXEC_CONTROL);
    return false;
  }

  const SaUint32T *p_procExecMode = immutil_getUint32Attr(
      (const SaImmAttrValuesT_2 **)m_attributes, "procExecMode", 0);
  if (p_procExecMode == 0) {
    LOG_NO("%s: Could not read procExecMode", __FUNCTION__);
  } else {
    m_procExecMode = *p_procExecMode;
    m_procExecMode_valid = true;
  }

  const SaUint32T *p_numberOfSingleSteps = immutil_getUint32Attr(
      (const SaImmAttrValuesT_2 **)m_attributes, "numberOfSingleSteps", 0);
  if (p_numberOfSingleSteps == 0) {
    LOG_NO("%s: Could not read numberOfSingleSteps", __FUNCTION__);
  } else {
    m_numberOfSingleSteps = *p_numberOfSingleSteps;
    m_numberOfSingleSteps_valid = true;
  }

  // Fill in node name vector
  SaAisErrorT ais_rc;
  SaUint32T number_of_nodes;
  ais_rc = immutil_getAttrValuesNumber(
      const_cast<char *>("nodesForSingleStep"),
      (const SaImmAttrValuesT_2 **)m_attributes, &number_of_nodes);
  if (ais_rc != SA_AIS_OK) {
    LOG_NO("%s: No nodesForSingleStep found", __FUNCTION__);
    return true;
  }

  for (unsigned int i = 0; i < number_of_nodes; i++) {
    const char *p_node_name = immutil_getStringAttr(
        (const SaImmAttrValuesT_2 **)m_attributes, "nodesForSingleStep", i);
    if (p_node_name == NULL) {
      break;
    }
    m_nodesForSingleStep.push_back(p_node_name);
    m_nodesForSingleStep_valid = true;
  }

  TRACE_LEAVE();
  return true;
}
/**
 * Read the smf config object and save openSafSmfExecControl and
 * smfProtectExecControlDuringInit
 *
 * @return false on fail
 */
bool SmfExecControlObjHandler::readOpenSafSmfConfig() {
  // Get the name of the exec control object
  if (p_immutil_object->getObject(SMF_CONFIG_OBJECT_DN, &m_attributes) ==
      false) {
    LOG_WA("%s: Could not get SMF config object from IMM %s", __FUNCTION__,
           SMF_CONFIG_OBJECT_DN);
    return false;
  }

  const char *exec_ctrl_name = immutil_getStringAttr(
      (const SaImmAttrValuesT_2 **)m_attributes, OPENSAF_SMF_EXEC_CONTROL, 0);
  if (exec_ctrl_name == NULL || strcmp(exec_ctrl_name, "") == 0) {
    LOG_WA("%s: Could not get %s attrs from SmfConfig", __FUNCTION__,
           OPENSAF_SMF_EXEC_CONTROL);
    return false;
  }
  c_openSafSmfExecControl = exec_ctrl_name;

  const SaUint32T *p_protect_flag =
      immutil_getUint32Attr((const SaImmAttrValuesT_2 **)m_attributes,
                            "smfProtectExecControlDuringInit", 0);
  if (p_protect_flag == NULL) {
    LOG_ER("could not read smfProtectExecControlDuringInit");
    return false;
  }
  m_smfProtectExecControlDuringInit = *p_protect_flag;
  m_smfProtectExecControlDuringInit_valid = true;

  return true;
}

/**
 * Delete the exec control object copy if exist
 */
void SmfExecControlObjHandler::removeExecControlObjectCopy() {
  TRACE_ENTER();
  modelmodify::DeleteDescriptor copy_delete;
  copy_delete.object_name = kOpenSafSmfExecControl_copy;
  modelmodify::CcbDescriptor ccb_for_delete;
  ccb_for_delete.AddDelete(copy_delete);

  modelmodify::ModelModification modifier;
  if (modifier.DoModelModification(ccb_for_delete) == false) {
    TRACE("%s: DoModelModification() Fail", __FUNCTION__);
  }

  TRACE_LEAVE();
}

/**
 * Create a copy of the exec control object. The exec control object must have
 * been read before a copy can be made.
 *
 * @return false on fail
 */
bool SmfExecControlObjHandler::copyExecControlObject() {
  TRACE_ENTER();

  modelmodify::AttributeDescriptor exec_control_attribute;
  modelmodify::CreateDescriptor exec_control_object_create;
  // Created as a top level object so no parent name is given
  exec_control_object_create.class_name = c_class_name;
  // Add the attributes and their values to the create descriptor
  exec_control_attribute.attribute_name = "procExecMode";
  exec_control_attribute.value_type = SA_IMM_ATTR_SAUINT32T;
  exec_control_attribute.AddValue(std::to_string(m_procExecMode));
  exec_control_object_create.AddAttribute(exec_control_attribute);
  exec_control_attribute.attribute_name = "numberOfSingleSteps";
  exec_control_attribute.value_type = SA_IMM_ATTR_SAUINT32T;
  exec_control_attribute.values_as_strings.clear();
  exec_control_attribute.AddValue(std::to_string(m_numberOfSingleSteps));
  exec_control_object_create.AddAttribute(exec_control_attribute);
  exec_control_attribute.attribute_name = "nodesForSingleStep";
  exec_control_attribute.value_type = SA_IMM_ATTR_SASTRINGT;
  exec_control_attribute.values_as_strings.clear();
  for (auto& node : m_nodesForSingleStep) {
    exec_control_attribute.AddValue(node);
  }
  exec_control_object_create.AddAttribute(exec_control_attribute);

  // Create the object
  modelmodify::CcbDescriptor create_ccb;
  create_ccb.AddCreate(exec_control_object_create);
  modelmodify::ModelModification modifier;
  bool rc = modifier.DoModelModification(create_ccb);

  TRACE_LEAVE();
  return rc;
}
