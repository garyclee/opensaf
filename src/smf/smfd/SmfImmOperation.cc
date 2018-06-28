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

#include "smf/smfd/SmfImmOperation.h"

#include <list>
#include <string>
#include <string.h>
#include "stdio.h"
#include "base/logtrace.h"
#include "smf/smfd/SmfImmOperation.h"
#include "smf/smfd/SmfRollback.h"
#include "smf/smfd/SmfUtils.h"

#include <saAis.h>
#include <saImmOm.h>
#include "osaf/immutil/immutil.h"
#include <saImm.h>
#include "base/saf_error.h"
#include "base/osaf_extended_name.h"
#include "base/osaf_time.h"
#include "smf/smfd/smfd_long_dn.h"

//============================
// Class SmfImmCreateOperation
//============================

//------------------------------------------------------------------------------
// addAttrValue()
//------------------------------------------------------------------------------
void SmfImmCreateOperation::AddOrUpdateAttribute(const std::string &i_name,
                                         const std::string &i_type,
                                         const std::string &i_value) {
  /* Check if attribute already exists (MULTI_VALUE) then add the
     value to this attribute */

  // For all attributes
  for (auto &elem : attributes_) {
    if ((elem).m_name == i_name) {
      (elem).AddAttributeValue(i_value);
      return;
    }
  }
  /* Attribute don't exists, create a new one and add to list */
  SmfImmAttribute newAttr;
  newAttr.SetAttributeName(i_name);
  newAttr.SetAttributeType(i_type);
  newAttr.AddAttributeValue(i_value);

  attributes_.push_back(newAttr);
}

SaAisErrorT SmfImmCreateOperation::Execute(SmfRollbackData *o_rollbackData) {
  TRACE_ENTER();
  SaAisErrorT result = SA_AIS_OK;

  // Fill in a create descriptor. The create descriptor already declared in the
  // inherited SmfImmOperation class shall be used
  // Keep rollback handling "as is"
  // Note: This Execute() operation can no longer fail except when doing
  // PrepareRollback()

  //object_create_
  // Verify that the create descriptor is filled in correctly,
  // except attributes
  if (object_create_.parent_name.size() > GetSmfMaxDnLength()) {
    LOG_NO("%s: Verify parent name Fail, Too long", __FUNCTION__);
    return SA_AIS_ERR_NAME_TOO_LONG;
  } else if (object_create_.class_name.empty()) {
    LOG_NO("%s: Verify class name Fail, No class name", __FUNCTION__);
    return SA_AIS_ERR_INVALID_PARAM;
  }

  // Fill in attributes from the list of SmfImmAttribute objects
  for (auto& attribute : attributes_) {
    object_create_.AddAttribute(attribute.GetAttributeDescriptor());
  }

  // Prepare a corresponding rollback operation
  if (o_rollbackData != NULL) {
    SaAisErrorT rollbackResult;
    if ((rollbackResult = this->PrepareRollback(o_rollbackData)) != SA_AIS_OK) {
      LOG_NO(
        "SmfImmCreateOperation::Execute, Failed to prepare rollback data rc=%s",
        saf_error(rollbackResult));
      result = SA_AIS_ERR_FAILED_OPERATION;
    }
  }

  TRACE_LEAVE();
  return result;
}

SaAisErrorT SmfImmCreateOperation::PrepareRollback(
    SmfRollbackData *o_rollbackData) {
  SmfImmUtils immUtil;
  SaImmAttrDefinitionT_2 **attributeDefs = NULL;

  if (immUtil.getClassDescription(class_name_, &attributeDefs) == false) {
    LOG_NO("SmfImmCreateOperation::PrepareRollback, Could not find class %s",
           class_name_.c_str());
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  // Find the SA_IMM_ATTR_RDN attribute
  std::string rdnAttrName;
  for (int i = 0; attributeDefs[i] != 0; i++) {
    SaImmAttrFlagsT flags = attributeDefs[i]->attrFlags;
    if ((flags & SA_IMM_ATTR_RDN) == SA_IMM_ATTR_RDN) {
      rdnAttrName = (char *)attributeDefs[i]->attrName;
      break;
    }
  }

  immUtil.classDescriptionMemoryFree(attributeDefs);
  if (rdnAttrName.length() == 0) {
    LOG_NO(
        "SmfImmCreateOperation::PrepareRollback, could not find RDN attribute in class %s",
        class_name_.c_str());
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  // Find the value of the RDN
  std::string rdnAttrValue;
  for (auto &elem : attributes_) {
    if (rdnAttrName == (elem).m_name) {
      if ((elem).m_values.size() != 1) {
        LOG_NO(
            "SmfImmCreateOperation::PrepareRollback, attribute %s contain %zu values, must contain exactly one value",
            rdnAttrName.c_str(), (elem).m_values.size());
        return SA_AIS_ERR_FAILED_OPERATION;
      }
      rdnAttrValue = (elem).m_values.front();
      break;
    }
  }
  if (rdnAttrValue.length() == 0) {
    LOG_NO(
        "SmfImmCreateOperation::PrepareRollback, could not find RDN value for %s, class %s",
        rdnAttrName.c_str(), class_name_.c_str());
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  TRACE("SmfImmCreateOperation::PrepareRollback, Found RDN %s=%s",
        rdnAttrName.c_str(), rdnAttrValue.c_str());

  /* Prepare deletion of created object at rollback */
  o_rollbackData->setType("DELETE");
  std::string delDn;
  delDn = rdnAttrValue;
  if (parent_dn_.length() > 0) {
    delDn += ",";
    delDn += parent_dn_;
  }
  o_rollbackData->setDn(delDn);

  return SA_AIS_OK;
}

//============================
// Class SmfImmDeleteOperation
//============================

SaAisErrorT SmfImmDeleteOperation::Execute(SmfRollbackData *o_rollbackData) {
  TRACE_ENTER();

  SaAisErrorT result = SA_AIS_OK;

  // Verify that the delete descriptor is filled in correctly
  if (object_delete_.object_name.size() > GetSmfMaxDnLength()) {
    LOG_NO("%s: Fail, Object Dn too long", __FUNCTION__);
    return SA_AIS_ERR_NAME_TOO_LONG;
  }

  // Prepare a corresponding rollback operation
  if (o_rollbackData != NULL) {
    if ((result = this->PrepareRollback(o_rollbackData)) != SA_AIS_OK) {
      LOG_NO(
        "SmfImmDeleteOperation::Execute, failed to prepare rollback data rc=%s",
        saf_error(result));
      result = SA_AIS_ERR_FAILED_OPERATION;
    }
  }

  TRACE_LEAVE();
  return result;
}

SaAisErrorT SmfImmDeleteOperation::PrepareRollback(
    SmfRollbackData *o_rollbackData) {
  SmfImmUtils immUtil;
  SaImmAttrDefinitionT_2 **attributeDefs;
  SaImmAttrValuesT_2 **attributes;
  SaImmAttrValuesT_2 *attr;
  const char *className;
  int i = 0;

  if (immUtil.getObject(m_dn, &attributes) == false) {
    LOG_NO("Could not find object %s", m_dn.c_str());
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  className = immutil_getStringAttr((const SaImmAttrValuesT_2 **)attributes,
                                    SA_IMM_ATTR_CLASS_NAME, 0);
  if (className == NULL) {
    LOG_NO(
        "SmfImmDeleteOperation::PrepareRollback, could not find class name for %s",
        m_dn.c_str());
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  if (immUtil.getClassDescription(className, &attributeDefs) == false) {
    LOG_NO("SmfImmDeleteOperation::PrepareRollback, could not find class %s",
           className);
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  /* For each attribute in the object that is not RUNTIME or IMM internal
     store the current value in rollback data for future use at rollback */

  std::string parentDn;
  const char *tmpParentDn = m_dn.c_str();
  const char *commaPos;

  /* Find first comma not prepended by a \ */
  while ((commaPos = strchr(tmpParentDn, ',')) != NULL) {
    if (*(commaPos - 1) != '\\') {
      parentDn = (commaPos + 1);
      break;
    }
    tmpParentDn = commaPos + 1;
  }

  o_rollbackData->setType("CREATE");
  o_rollbackData->setDn(parentDn);
  o_rollbackData->setClass(className);

  while ((attr = attributes[i++]) != NULL) {
    // Check if IMM internal attribute
    if (!strcmp(attr->attrName, SA_IMM_ATTR_CLASS_NAME)) {
      continue;
    } else if (!strcmp(attr->attrName, SA_IMM_ATTR_ADMIN_OWNER_NAME)) {
      continue;
    } else if (!strcmp(attr->attrName, SA_IMM_ATTR_IMPLEMENTER_NAME)) {
      continue;
    }

    // Exclude RUNTIME attributes without PERSISTENT
    bool saveAttribute = true;
    for (int i = 0; attributeDefs[i] != 0; i++) {
      if (!strcmp(attr->attrName, attributeDefs[i]->attrName)) {
        SaImmAttrFlagsT flags = attributeDefs[i]->attrFlags;
        if ((flags & SA_IMM_ATTR_RUNTIME) == SA_IMM_ATTR_RUNTIME) {
          if ((flags & SA_IMM_ATTR_PERSISTENT) != SA_IMM_ATTR_PERSISTENT) {
            saveAttribute = false;
          }
        }
        break;
      }
    }

    if (saveAttribute == true) {
      if (attr->attrValuesNumber == 0) {
        // No need to re-create originally empty attribute
        continue;
      } else {
        for (unsigned int j = 0; j < attr->attrValuesNumber; j++) {
          o_rollbackData->addAttrValue(
              attr->attrName, smf_immTypeToString(attr->attrValueType),
              smf_valueToString(attr->attrValues[j], attr->attrValueType));
        }
      }
    }
  }

  immUtil.classDescriptionMemoryFree(attributeDefs);

  return SA_AIS_OK;
}

//============================
// Class SmfImmModifyOperation
//============================

// Fill in and add attribute modify descriptors based on the list of attribute
// objects
void SmfImmModifyOperation::CreateAttrMods(void) {
  TRACE_ENTER();

  modelmodify::AttributeModifyDescriptor attribute_modify;
  SaImmAttrModificationTypeT modification_type =
      modelmodify::StringToImmAttrModType(modification_type_);
  for (auto& attribute : attributes_) {
    // Fill in an attribute modify descriptor
    attribute_modify.modification_type = modification_type;
    attribute_modify.attribute_descriptor = attribute.GetAttributeDescriptor();
    // And add it to the modify descriptor
    object_modify_.AddAttributeModification(attribute_modify);
  }
}

void SmfImmModifyOperation::AddOrUpdateAttribute(const std::string &i_name,
                                         const std::string &i_type,
                                         const std::string &i_value) {
  /* Check if attribute already exists (MULTI_VALUE) then add the
     value to this attribute */
  // For all attributes
  for (auto &elem : attributes_) {
    if ((elem).m_name == i_name) {
      (elem).AddAttributeValue(i_value);
      return;
    }
  }
  /* Attribute don't exists, create a new one and add to list */
  SmfImmAttribute newAttr;
  newAttr.SetAttributeName(i_name);
  newAttr.SetAttributeType(i_type);
  newAttr.AddAttributeValue(i_value);

  attributes_.push_back(newAttr);
}

SaAisErrorT SmfImmModifyOperation::Execute(SmfRollbackData *o_rollbackData) {
  TRACE_ENTER();
  SaAisErrorT result = SA_AIS_OK;

  // Verify that the modify descriptor is filled in correctly,
  // except attributes
  if (object_modify_.object_name.size() > GetSmfMaxDnLength()) {
    LOG_NO("%s: Verify object name Fail, Too long", __FUNCTION__);
    return SA_AIS_ERR_NAME_TOO_LONG;
  }

  // Fill in attribute modifies from the list of SmfImmAttribute objects
  CreateAttrMods();

  // Prepare a corresponding rollback operation
  if (o_rollbackData != NULL) {
    if ((result = this->PrepareRollback(o_rollbackData)) != SA_AIS_OK) {
      LOG_NO(
          "SmfImmModifyOperation::Execute, failed to prepare rollback data %s",
          saf_error(result));
      result = SA_AIS_ERR_FAILED_OPERATION;
    }
  }

  TRACE_LEAVE();
  return result;
}

SaAisErrorT SmfImmModifyOperation::PrepareRollback(
    SmfRollbackData *o_rollbackData) {
  SmfImmUtils immUtil;
  SaImmAttrValuesT_2 **attributes;

  if (immUtil.getObject(object_name_, &attributes) == false) {
    LOG_NO("Could not find object %s", object_name_.c_str());
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  o_rollbackData->setType("MODIFY");
  o_rollbackData->setDn(object_name_);

  /* For each modified attribute in the object
     store the current value in rollback data for future use at rollback */

  for (auto &elem : attributes_) {
    int i = 0;
    SaImmAttrValuesT_2 *attr;
    while ((attr = attributes[i++]) != NULL) {
      if (!strcmp((elem).m_name.c_str(), attr->attrName)) {
        if (attr->attrValuesNumber == 0) {
          o_rollbackData->addAttrValue(attr->attrName,
                                       smf_immTypeToString(attr->attrValueType),
                                       "<_empty_>");
        } else {
          for (unsigned int j = 0; j < attr->attrValuesNumber; j++) {
            o_rollbackData->addAttrValue(
                attr->attrName, smf_immTypeToString(attr->attrValueType),
                smf_valueToString(attr->attrValues[j], attr->attrValueType));
          }
        }
        break;
      }
    }
  }
  return SA_AIS_OK;
}

//==============================
// Class SmfImmRTCreateOperation
//==============================

SmfImmRTCreateOperation::SmfImmRTCreateOperation()
    : m_className(""),
      m_parentDn(""),
      m_immHandle(0),
      m_values(0),
      m_immAttrValues(0) {}

SmfImmRTCreateOperation::~SmfImmRTCreateOperation() {}

void SmfImmRTCreateOperation::SetClassName(const std::string &i_name) {
  m_className = i_name;
}

void SmfImmRTCreateOperation::SetParentDn(const std::string &i_dn) {
  m_parentDn = i_dn;
}

void SmfImmRTCreateOperation::SetImmHandle(const SaImmOiHandleT &i_handle) {
  m_immHandle = i_handle;
}

void SmfImmRTCreateOperation::AddValue(const SmfImmAttribute &i_value) {
  m_values.push_back(i_value);
}

bool SmfImmRTCreateOperation::CreateAttrValues(void) {
  TRACE_ENTER();

  // Create space for attributes
  SaImmAttrValuesT_2 **attributeValues =
      (SaImmAttrValuesT_2 **)new (std::nothrow)
          SaImmAttrValuesT_2 *[m_values.size() + 1];
  osafassert(attributeValues != 0);
  int i = 0;  // Index to a SaImmAttrValuesT pointer in the attributeValues
              // array

  // For all attribures to create
  for (auto &elem : m_values) {
    // Create structure for one attribute
    SaImmAttrValuesT_2 *attr = new (std::nothrow) SaImmAttrValuesT_2();
    osafassert(attr != 0);

    attr->attrName = (char *)(elem).m_name.c_str();
    if (smf_stringToImmType((char *)(elem).m_type.c_str(),
                            attr->attrValueType) == false) {
      LOG_NO(
          "SmfImmRTCreateOperation::CreateAttrValues, fail to convert attr "
          "[%s] type to valid IMM type", attr->attrName);
      delete attr;
      for (int k = 0; i > k; k++) {
        delete attributeValues[k];
      }
      delete[] attributeValues;
      TRACE_LEAVE();
      return false;
    }

    TRACE("c=[%s], p=[%s], attr=[%s]", m_className.c_str(), m_parentDn.c_str(),
          attr->attrName);

    attr->attrValuesNumber = (elem).m_values.size();

    if (smf_stringsToValues(attr, (elem).m_values) ==
        false) {  // Convert the string to a SA Forum type
      LOG_NO(
          "SmfImmRTCreateOperation::CreateAttrValues, fail to convert attr "
          "[%s] value string to attr value", attr->attrName);
      free(attr->attrValues);
      delete attr;
      for (int k = 0; i > k; k++) {
        delete attributeValues[k];
      }
      delete[] attributeValues;
      TRACE_LEAVE();
      return false;
    }
    // Add the pointer to the SaImmAttrValuesT_2 structure to the attributes
    // list
    attributeValues[i++] = attr;
  }

  attributeValues[i] = NULL;  // Null terminate the list of attribute pointers
  this->SetAttrValues(attributeValues);

  TRACE_LEAVE();
  return true;
}

void SmfImmRTCreateOperation::SetAttrValues(SaImmAttrValuesT_2 **i_values) {
  m_immAttrValues = i_values;
}

SaAisErrorT SmfImmRTCreateOperation::Execute() {
  TRACE_ENTER();

  SaAisErrorT result = SA_AIS_OK;

  // Convert the strings to structures and types accepted by the IMM interface
  if (this->CreateAttrValues() == false) {
    LOG_NO(
        "SmfImmRTCreateOperation::Execute, fail to convert to IMM attr "
        "structure");
    TRACE_LEAVE();
    return SA_AIS_ERR_FAILED_OPERATION;
  }

  const char *className = m_className.c_str();

  if (m_parentDn.length() > GetSmfMaxDnLength()) {
    LOG_NO(
        "SmfImmRTCreateOperation::Execute, createObject failed Too long "
        "parent name %zu",
        m_parentDn.length());
    TRACE_LEAVE();
    return SA_AIS_ERR_NAME_TOO_LONG;
  }

  if (!m_immAttrValues) {
    LOG_NO("SmfImmRTCreateOperation::Execute, no SaImmAttrValuesT_2** is set");
    TRACE_LEAVE();
    return SA_AIS_ERR_UNAVAILABLE;
  }

  SaNameT parentName;
  osaf_extended_name_lend(m_parentDn.c_str(), &parentName);

  result = immutil_saImmOiRtObjectCreate_2(
      m_immHandle, (char *)className, &parentName,
      (const SaImmAttrValuesT_2 **)m_immAttrValues);

  if (result != SA_AIS_OK) {
    TRACE("saImmOiRtObjectCreate_2 returned %s for %s, parent %s",
          saf_error(result), className, osaf_extended_name_borrow(&parentName));
  }

  // Free the m_immAttrValues mem
  for (int i = 0; m_immAttrValues[i] != 0; i++) {
    SaImmAttrValueT *value = m_immAttrValues[i]->attrValues;
    for (unsigned int k = 0; m_immAttrValues[i]->attrValuesNumber > k; k++) {
      if (m_immAttrValues[i]->attrValueType == SA_IMM_ATTR_SASTRINGT) {
        free(*((SaStringT *)*value));
      } else if (m_immAttrValues[i]->attrValueType == SA_IMM_ATTR_SANAMET) {
        osaf_extended_name_free((SaNameT *)*value);
      } else if (m_immAttrValues[i]->attrValueType == SA_IMM_ATTR_SAANYT) {
        free(((SaAnyT *)*value)->bufferAddr);
      }

      free(*value);
      value++;
    }

    free(m_immAttrValues[i]->attrValues);
    delete m_immAttrValues[i];
  }
  delete[] m_immAttrValues;

  TRACE_LEAVE();
  return result;
}

//==============================
// Class SmfImmRTUpdateOperation
//==============================

SmfImmRTUpdateOperation::SmfImmRTUpdateOperation()
    : m_dn(""), m_op(""), m_immHandle(0), m_values(0), m_immAttrMods(0) {}

SmfImmRTUpdateOperation::~SmfImmRTUpdateOperation() {}

void SmfImmRTUpdateOperation::SetDn(const std::string &i_dn) { m_dn = i_dn; }

void SmfImmRTUpdateOperation::SetOp(const std::string &i_op) { m_op = i_op; }

void SmfImmRTUpdateOperation::SetImmHandle(const SaImmOiHandleT &i_handle) {
  m_immHandle = i_handle;
}

bool SmfImmRTUpdateOperation::CreateAttrMods(void) {
  TRACE_ENTER();

  SaImmAttrModificationT_2 **attributeModifications =
      (SaImmAttrModificationT_2 **)new SaImmAttrModificationT_2
          *[m_values.size() + 1];
  int i = 0;  // Index to a SaImmAttrModificationT_2 pointer in the
              // attributeModificationss array

  for (auto &elem : m_values) {
    SaImmAttrModificationT_2 *mod =
        new (std::nothrow) SaImmAttrModificationT_2();
    osafassert(mod != 0);
    mod->modType = smf_stringToImmAttrModType(
        (char *)m_op.c_str());  // Convert an store the modification type from
                                // string to SA Forum type
    if (mod->modType == 0) {
      delete[] attributeModifications;
      delete mod;
      LOG_NO(
          "SmfImmRTUpdateOperation::CreateAttrMods, fail convert string to IMM attr mod type [%s:%s]",
          m_dn.c_str(), (elem).m_name.c_str());
      TRACE_LEAVE();
      return false;
    }
    mod->modAttr.attrName = (char *)(elem).m_name.c_str();
    if (smf_stringToImmType((char *)(elem).m_type.c_str(),
                            mod->modAttr.attrValueType) == false) {
      delete[] attributeModifications;
      delete mod;
      LOG_NO(
          "SmfImmRTUpdateOperation::CreateAttrMods, fail to convert string to IMM attr type [%s:%s]",
          m_dn.c_str(), (elem).m_name.c_str());
      TRACE_LEAVE();
      return false;
    }
    TRACE("Modifying %s:%s = %s", m_dn.c_str(), (elem).m_name.c_str(),
          (elem).m_values.front().c_str());

    if ((elem).m_values.size() <= 0) {  // Must have at least one value
      LOG_NO(
          "SmfImmRTUpdateOperation::CreateAttrMods, No values %s:%s (size=%zu)",
          m_dn.c_str(), (elem).m_name.c_str(), (elem).m_values.size());
      delete[] attributeModifications;
      delete mod;
      TRACE_LEAVE();
      return false;
    }
    mod->modAttr.attrValuesNumber = (elem).m_values.size();

    if (smf_stringsToValues(&mod->modAttr, (elem).m_values) ==
        false) {  // Convert the string to a SA Forum type
      delete[] attributeModifications;
      delete mod;
      LOG_NO(
          "SmfImmRTUpdateOperation::CreateAttrMods, fail to conv string to value [%s:%s = %s]",
          m_dn.c_str(), (elem).m_name.c_str(), (elem).m_values.front().c_str());
      TRACE_LEAVE();
      return false;
    }
    // Add the pointer to the SaImmAttrModificationT_2 structure to the
    // modifications list
    attributeModifications[i++] = mod;
  }

  attributeModifications[i] =
      NULL;  // Null terminate the list of modification pointers
  m_immAttrMods = attributeModifications;

  TRACE_LEAVE();
  return true;
}

void SmfImmRTUpdateOperation::AddValue(const SmfImmAttribute &i_value) {
  m_values.push_back(i_value);
}

SaAisErrorT SmfImmRTUpdateOperation::Execute() {
  TRACE_ENTER();

  SaAisErrorT result = SA_AIS_OK;

  // Convert the strings to structures and types accepted by the IMM interface
  if (this->CreateAttrMods() == false) {
    LOG_NO(
        "SmfImmRTUpdateOperation::Execute, fail to convert to IMM attr structure");
    result = SA_AIS_ERR_FAILED_OPERATION;
    goto exit;
  }

  if (m_dn.length() > GetSmfMaxDnLength()) {
    LOG_NO(
        "SmfImmRTUpdateOperation::Execute, too long DN [%zu], max=[%zu], dn=[%s]",
        m_dn.length(), static_cast<size_t>(GetSmfMaxDnLength()), m_dn.c_str());
    result = SA_AIS_ERR_NAME_TOO_LONG;
    goto exit;
  }

  if (m_immAttrMods == NULL) {
    LOG_NO("SmfImmRTUpdateOperation::Execute, no SaImmAttrValuesT_2** is set");
    result = SA_AIS_ERR_UNAVAILABLE;
    goto exit;
  }

  SaNameT objectName;
  osaf_extended_name_lend(m_dn.c_str(), &objectName);

  result = immutil_saImmOiRtObjectUpdate_2(
      m_immHandle, &objectName,
      (const SaImmAttrModificationT_2 **)m_immAttrMods);

  if (result != SA_AIS_OK) {
    TRACE("saImmOiRtObjectUpdate_2 returned %s for %s", saf_error(result),
          osaf_extended_name_borrow(&objectName));
  }

exit:
  // Free the m_immAttrMods mem
  if (m_immAttrMods != NULL) {
    for (int i = 0; m_immAttrMods[i] != 0; i++) {
      SaImmAttrValueT *value = m_immAttrMods[i]->modAttr.attrValues;
      for (unsigned int k = 0; m_immAttrMods[i]->modAttr.attrValuesNumber > k;
           k++) {
        if (m_immAttrMods[i]->modAttr.attrValueType == SA_IMM_ATTR_SASTRINGT) {
          free(*((SaStringT *)*value));
        } else if (m_immAttrMods[i]->modAttr.attrValueType ==
                   SA_IMM_ATTR_SAANYT) {
          free(((SaAnyT *)*value)->bufferAddr);
        }

        free(*value);
        value++;
      }

      free(m_immAttrMods[i]->modAttr.attrValues);
      delete m_immAttrMods[i];
    }

    delete[] m_immAttrMods;
  }

  TRACE_LEAVE();
  return result;
}
