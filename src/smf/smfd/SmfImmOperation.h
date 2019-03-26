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

#ifndef SMF_SMFD_SMFIMMOPERATION_H_
#define SMF_SMFD_SMFIMMOPERATION_H_

#include <string>
#include <list>

#include "ais/include/saImmOm.h"
#include "ais/include/saImmOi.h"

#include "smf/smfd/SmfRollback.h"
#include "smf/smfd/imm_modify_config/immccb.h"

// =========================================
// Holds information about one IMM attribute
class SmfImmAttribute {
 public:
  SmfImmAttribute() {};
  ~SmfImmAttribute() {};

  void SetAttributeName(const std::string& i_name) {
    m_name = i_name;
    attribute_.attribute_name = i_name;
  }

  const std::string& GetAttributeName() { return attribute_.attribute_name; }

  void SetAttributeType(const std::string& i_type) {
    m_type = i_type;
    attribute_.value_type = modelmodify::StringToSaImmValueType(i_type);
  }

  // Note: More than one value can be added (multi-value)
  void AddAttributeValue(const std::string& i_value) {
    m_values.push_back(i_value);
    if (i_value.compare("<_empty_>") != 0)
      attribute_.AddValue(i_value);
  }

  // Note: This is the legacy function that returns a list of strings. Is kept
  //       for backwards compatibility
  //       This class also contains an attribute descriptor that has a vector
  //       of values which is used with the model modifier (immccb.h)
  const std::list<std::string>& GetAttributeValues() { return m_values; }

  const modelmodify::AttributeDescriptor
  GetAttributeDescriptor() { return attribute_; }

#if 1
  // Note: Should be refactored. Use GetAttributeDescriptor instead.
  // Must be kept until further refactoring is done
  friend class SmfImmOperation;
  friend class SmfImmCreateOperation; // Using m_values, m_name, m_type
  friend class SmfImmModifyOperation; // Using m_values, m_name, m_type
  friend class SmfImmDeleteOperation;
#endif
  friend class SmfImmRTCreateOperation; // Using m_values, m_name, m_type
  friend class SmfImmRTUpdateOperation; // Using m_values, m_name, m_type

 private:
  // Note: Used directly by (friend) SmfImmRT operations
  std::string m_name;              /* Attribute name */
  std::string m_type;              /* Attribute type */
  std::list<std::string> m_values; /* Attribute values */

  // Also store in an attribute descriptor
  // Note: Above parameters can be removed when we get rid of all the friend
  //       classes
  modelmodify::AttributeDescriptor attribute_;
};

// =============================
// Base class for IMM operations
// Create, Delete and Modify
// =============================

class SmfImmOperation {
 public:
  SmfImmOperation()
    : imm_operation_(NotSet) {}
  virtual ~SmfImmOperation() {}

  // This "Execute" do all IMM operations needed to create and apply a CCB
  // Uses immccb to do this. See immccb.h
  virtual SaAisErrorT Execute(SmfRollbackData* o_rollbackData = NULL) {
    LOG_NO("execute must be specialised");
    return (SaAisErrorT)-1;
  }

  virtual int Rollback() {
    LOG_NO("rollback must be specialised");
    return -1;
  }

  // Add an attribute object of class SmfImmAttribute
  virtual void AddAttributeObject(const SmfImmAttribute& i_value) {
    LOG_NO("addValue must be specialised");
  }

  // Delete all attribute of object_create_ or object_modify_
  void DeleteAttributes() {
    object_create_.attributes.clear();
    object_modify_.modifications.clear();
  }

  // Create and add a new attribute if the attribute does not already
  // exist (based on name). If an attribute with the given name already exist
  // no attribute is created but the value (i_value) is added (multivalue).
  //
  // Note: The attribute must be an attribute defined in the class for the
  //       object to be created.
  virtual void AddOrUpdateAttribute(const std::string& i_name,
                            const std::string& i_type,
                            const std::string& i_value) {
    LOG_NO("addAttrValue must be specialised");
  }

  // get the correct operation descriptor
  enum OperationType { NotSet, Create, Delete, Modify };

  const OperationType GetOperationType() { return imm_operation_; }
  const modelmodify::CreateDescriptor GetCreateDescriptor() {
    return object_create_;
  }
  const modelmodify::DeleteDescriptor GetDeleteDescriptor() {
    return object_delete_;
  }
  const modelmodify::ModifyDescriptor GetModifyDescriptor() {
    return object_modify_;
  }

 protected:
  OperationType imm_operation_;
  modelmodify::CreateDescriptor object_create_;
  modelmodify::DeleteDescriptor object_delete_;
  modelmodify::ModifyDescriptor object_modify_;

};

// Create an IMM object.
// Holds information about one create operation
// The interface is documented in (virtual) class SmfImmOperation
class SmfImmCreateOperation : public SmfImmOperation {
 public:
  SmfImmCreateOperation()
      : SmfImmOperation(),
      class_name_(""),
      parent_dn_(""),
      attributes_(0) { imm_operation_ = Create; }
      //m_immAttrValues(0) { imm_operation_ = Create; }

  ~SmfImmCreateOperation() {};

  // Verifies parameters in a create descriptor and adds the attributes to
  // the create descriptor
  SaAisErrorT Execute(SmfRollbackData* o_rollbackData = NULL);

  ///
  /// Purpose: Rollback the operation.
  /// @param   None.
  /// @return  None.
  ///
  int Rollback() {
    LOG_NO("Rollback not implemented yet");
    return -1;
  }

  // Set the class name of the object to be created.
  void SetClassName(const std::string& i_name) {
    class_name_ = i_name;
    object_create_.class_name = i_name;
  }

  // Get the class name of the object to be created.
  const std::string& GetClassName() {
    return object_create_.class_name;
  }

  // Set parent DN for object to be created
  void SetParentDn(const std::string& i_dn) {
    parent_dn_ = i_dn;
    object_create_.parent_name = i_dn;
  }

  // Get parent DN for object to be created
  const std::string& GetParentDn() {
    return object_create_.parent_name;
  }

  void AddAttributeObject(const SmfImmAttribute& i_attribute) {
    attributes_.push_back(i_attribute);
  }

  void AddOrUpdateAttribute(const std::string& i_name,
                               const std::string& i_type,
                               const std::string& i_value);

  // Get the list of attributes given for creation of this object
  const std::list<SmfImmAttribute>& GetAttributeObjects() {
    return attributes_;
  }

 private:
  ///
  /// Purpose: Prepare rollback of the operation.
  /// @param   None.
  /// @return  None.
  ///
  SaAisErrorT PrepareRollback(SmfRollbackData* o_rollbackData);

  // Note: The following two variables are used in PrepareRollback.
  // PrepareRollback() could be refactored to use attributes_ instead
  std::string class_name_; /* class name for the object to be created */
  std::string parent_dn_;  /* dn to the parent object */

  // Note: Used in PrepareRollback
  std::list<SmfImmAttribute> attributes_;  /* Attributes set at creation */
};

// Delete an IMM object.
// Holds information about one delete operation
// The interface is documented in (virtual) class SmfImmOperation
class SmfImmDeleteOperation : public SmfImmOperation {
 public:
  SmfImmDeleteOperation() : SmfImmOperation() { imm_operation_ = Delete; }
  ~SmfImmDeleteOperation() {}

  ///
  /// Purpose: Execute the operation.
  /// @param   None.
  /// @return  None.
  ///
  SaAisErrorT Execute(SmfRollbackData* o_rollbackData = NULL);

  ///
  /// Purpose: Rollback the operation.
  /// @param   None.
  /// @return  None.
  ///
  int Rollback() {
    LOG_NO("Rollback not implemented yet");
    return -1;
  }

  ///
  /// Purpose: Set the DN for the object to be deleted.
  /// @param   i_dn The DN.
  /// @return  None.
  ///
  void SetDn(const std::string& i_dn) {
    m_dn = i_dn;
    object_delete_.object_name = i_dn;
  }

  ///
  /// Purpose: Get the DN for the object to be deleted.
  /// @param   None
  /// @return  const std::string
  ///
  const std::string& GetDn() { return object_delete_.object_name; }

 private:
  ///
  /// Purpose: Prepare rollback of the operation.
  /// @param   None.
  /// @return  None.
  ///
  SaAisErrorT PrepareRollback(SmfRollbackData* o_rollbackData);

  // Used in PrepareRollback
  std::string m_dn; /* dn to the object to delete */
};

// Modify an IMM object.
// Holds information about one modify operation
// The interface is documented in (virtual) class SmfImmOperation
// Note: Only one modification type can be set for the whole object. This means
// that if a list of attributes is given all attributes will be modified
// based on the same type.
// This is a limitation since the IMM API allows each attribute to have its
// own modification type
class SmfImmModifyOperation : public SmfImmOperation {
 public:
  SmfImmModifyOperation()
    : SmfImmOperation(),
    object_name_(""),
    modification_type_(""),
    attributes_(0),
    //m_immAttrMods(0)
    m_rdn("")  { imm_operation_ = Modify; }

  ~SmfImmModifyOperation() {}

  SaAisErrorT Execute(SmfRollbackData* o_rollbackData = NULL);

  int Rollback() {
    LOG_NO("Rollback not implemented yet");
    return -1;
  }

  // Set the DN of the object to be modified.
  void SetObjectDn(const std::string& i_dn) {
    object_name_ = i_dn;
    object_modify_.object_name = i_dn;
  }
  // Get the DN of the object to be modified.
  const std::string& GetObjectDn() { return object_modify_.object_name; }

  // NOTE: Rdn is not used with this imm operation.
  //       This object is only used as a store for this information.
  // Set the RDN of the object to be modified.
  void SetRdn(const std::string& i_rdn) { m_rdn = i_rdn; }
  // Set the RDN of the object to be modified.
  const std::string& GetRdn() { return m_rdn; }

  // Set type of modification as a string
  // The string must correspond with a SaImmAttrModificationTypeT
  // SA_IMM_ATTR_VALUES_ADD, SA_IMM_ATTR_VALUES_DELETE or
  // SA_IMM_ATTR_VALUES_REPLACE
  void SetModificationType(const std::string& i_op) {
    modification_type_ = i_op;
  }

  void AddAttributeObject(const SmfImmAttribute& i_value) {
    attributes_.push_back(i_value);
  }

  void AddOrUpdateAttribute(const std::string& i_name, const std::string& i_type,
                    const std::string& i_value);

 private:
  ///
  /// Purpose: Create the attr modification structure from string
  /// representation.
  /// @param   None.
  /// @return  Bool true if successful, otherwise false.
  ///
  void CreateAttrMods(void);

  ///
  /// Purpose: Prepare rollback of the operation.
  /// @param   None.
  /// @return  None.
  ///
  SaAisErrorT PrepareRollback(SmfRollbackData* o_rollbackData);

  std::string object_name_;
  std::string modification_type_;

  std::list<SmfImmAttribute> attributes_;

  // NOTE: This attribute is not used in this class. Is only stored here
  // rdn is an optional attribute which may be set in the
  // campaign targetEntityTemplate
  std::string m_rdn;
};

///
/// Purpose: Class for creation of run time IMM object.
///

class SmfImmRTCreateOperation {
 public:
  ///
  /// Purpose: Constructor.
  /// @param   None
  /// @return  None
  ///
  SmfImmRTCreateOperation();

  ///
  /// Purpose: Destructor.
  /// @param   None
  /// @return  None
  ///
  ~SmfImmRTCreateOperation();

  ///
  /// Purpose: Execute the operation.
  /// @param   None.
  /// @return  None.
  ///
  SaAisErrorT Execute();

  ///
  /// Purpose: Set the class name of the object to be created.
  /// @param   i_name The name of the class.
  /// @return  None.
  ///
  void SetClassName(const std::string& i_name);

  ///
  /// Purpose: Set the parent DN for the created object.
  /// @param   i_dn The parent DN.
  /// @return  None.
  ///
  void SetParentDn(const std::string& i_dn);

  ///
  /// Purpose: Set the IMM OI handle.
  /// @param   i_handle The SaImmOiHandleT.
  /// @return  None.
  ///
  void SetImmHandle(const SaImmOiHandleT& i_handle);

  ///
  /// Purpose: Add a value for an attribute of the object to be created.
  /// @param   i_value A SmfImmAttribute containing the attribute information.
  /// @return  None.
  ///
  void AddValue(const SmfImmAttribute& i_value);

  ///
  /// Purpose: Create the attr values structure from previously added string
  /// values.
  /// @param   None.
  /// @return  Bool true if successful, otherwise false.
  ///
  bool CreateAttrValues(void);

 private:
  ///
  /// Purpose: Set the SaImmAttrValuesT_2**, pointing to the array of pointers
  /// to SaImmAttrValuesT_2.
  /// @param   i_values A SaImmAttrValuesT_2** pointing to a null terminated
  /// array of SaImmAttrValuesT_2 pointers.
  /// @return  None.
  ///
  void SetAttrValues(SaImmAttrValuesT_2** i_values);

  std::string m_className;    /* class name for the object to be created */
  std::string m_parentDn;     /* dn to the parent object */
  SaImmOiHandleT m_immHandle; /* The IMM OI handle */
  std::list<SmfImmAttribute> m_values;  /* Attribute creation values */
  SaImmAttrValuesT_2** m_immAttrValues; /* The array of opinters to
                                           SaImmAttrValuesT_2 structures */
};

///
/// Purpose: Class for update of run time IMM objects.
///

class SmfImmRTUpdateOperation {
 public:
  ///
  /// Purpose: Constructor.
  /// @param   None
  /// @return  None
  ///
  SmfImmRTUpdateOperation();

  ///
  /// Purpose: Destructor.
  /// @param   None
  /// @return  None
  ///
  ~SmfImmRTUpdateOperation();

  ///
  /// Purpose: Execute the operation.
  /// @param   None.
  /// @return  None.
  ///
  SaAisErrorT Execute();

  ///
  /// Purpose: Set the DN for the object to modify
  /// @param   i_dn The object DN.
  /// @return  None.
  ///
  void SetDn(const std::string& i_dn);

  ///
  /// Purpose: Set the type of modification operation.
  /// @param   i_op The name of the operation
  /// (SA_IMM_ATTR_VALUES_ADD/SA_IMM_ATTR_VALUES_DELETE/SA_IMM_ATTR_VALUES_REPLACE).
  /// @return  None.
  ///
  void SetOp(const std::string& i_op);

  ///
  /// Purpose: Set the IMM OI handle.
  /// @param   i_handle The SaImmOiHandleT.
  /// @return  None.
  ///
  void SetImmHandle(const SaImmOiHandleT& i_handle);

  ///
  /// Purpose: Add a value for an attribute of the object to be created.
  /// @param   i_value A SmfImmAttribute containing the attribute information.
  /// @return  None.
  ///
  void AddValue(const SmfImmAttribute& i_value);

  ///
  /// Purpose: Create the attr values structure from previously added string
  /// values.
  /// @param   None.
  /// @return  Bool true if successful, otherwise false.
  ///
  bool CreateAttrMods(void);

 private:
  ///
  /// Purpose: Set the SaImmAttrValuesT_2**, pointing to the array of pointers
  /// to SaImmAttrValuesT_2.
  /// @param   i_values A SaImmAttrValuesT_2** pointing to a null terminated
  /// array of SaImmAttrValuesT_2 pointers.
  /// @return  None.
  ///
  std::string m_dn;                         /* dn to the parent object */
  std::string m_op;                         /* type of modification operation */
  SaImmOiHandleT m_immHandle;               /* The IMM OI handle */
  std::list<SmfImmAttribute> m_values;      /* Attribute creation values */
  SaImmAttrModificationT_2** m_immAttrMods; /* The array of opinters to
                                               SaImmAttrValuesT_2 structures */
};

#endif  // SMF_SMFD_SMFIMMOPERATION_H_
