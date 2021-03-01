############################################################################
#
# (C) Copyright 2015 The OpenSAF Foundation
# (C) Copyright 2017 Ericsson AB. All rights reserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
# under the GNU Lesser General Public License Version 2.1, February 1999.
# The complete license can be accessed from the following location:
# http://opensource.org/licenses/lgpl-license.php
# See the Copying file included with the OpenSAF distribution for full
# licensing terms.
#
# Author(s): Ericsson
#
############################################################################
# pylint: disable=unused-argument
""" Class representing an IMM Object Implementer """
from __future__ import print_function
import select
import itertools

from pyosaf.saAis import eSaAisErrorT, unmarshalNullArray, \
    unmarshalSaStringTArray
from pyosaf import saImm, saImmOi
from pyosaf.saImm import eSaImmValueTypeT, eSaImmAttrModificationTypeT, \
    eSaImmClassCategoryT, SaImmClassNameT, unmarshalSaImmValue, SaStringT, PY3
from pyosaf.saImmOi import SaImmOiImplementerNameT
from pyosaf.utils import SafException, decorate, bad_handle_retry, log_err
from pyosaf.utils.immoi import OiAgent
from pyosaf.utils.immom.object import ImmObject


saImmOiClassImplementerSet = decorate(saImmOi.saImmOiClassImplementerSet)
saImmOiImplementerSet = decorate(saImmOi.saImmOiImplementerSet)


class AdminOperationParameter(object):
    """ This class represents an admin operation parameter """
    def __init__(self, name, param_type, value):
        """ Create an instance of an admin operation parameter """
        self.name = name
        self.type = param_type
        self.value = value


def admin_operation_decorate(class_name, op_id):
    """ Admin operation decorator
    Decorate the specified function with the provided class name and admin
    operation id

    Args:
        class_name (str): Class name
        op_id (str): Operation id

    Returns:
        func: The decorated function
    """
    def inner_admin_op_decorator(func):
        """ Inner decorator which actually sets the admin_op id """

        setattr(func, 'AdminOperationOperationId', op_id)
        setattr(func, 'AdminOperationClassName', class_name)
        return func

    return inner_admin_op_decorator


class AdminOperationFunction(object):
    """ Encapsulation of an admin operation and its id """
    def __init__(self, class_name, operation_id, func):
        """ Create a pair of an operation id and a function """
        self.operation_id = operation_id
        self.class_name = class_name
        self.function = func

    def execute(self, name, parameters):
        """ Execute the admin operation """
        return self.function(name, parameters)

    def matches(self, class_name, operation_id):
        """ Return true if this admin operation pair matches the given admin
        operation id
        """
        return \
            operation_id == self.operation_id and class_name == self.class_name


class _ContainmentConstraint(object):
    """ Class defining a containment constraint """
    def __init__(self, parent_class, child_class, lower, upper):
        """ Create a containment constraint with optional lower and upper
        cardinality
        """
        self.parent_class = parent_class
        self.child_class = child_class
        self.lower = lower
        self.upper = upper


class Constraints(OiAgent):
    """ Class defining constraints for changes to the instances implemented by
    the OI
    """
    def __init__(self):
        """ Create an empty Constraints instance """
        super(Constraints, self).__init__()
        self.containments = {}
        self.cardinality = {}

    def add_allowed_containment(self, parent_class, child_class,
                                lower=None, upper=None):
        """ Add a constraint on which type of classes can be created under
        the parent

        Args:
            parent_class (str): Parent class
            child_class (str): Children class
            lower (int): Lower bound for Parent-Child cardinality
            upper (int): Upper bound for Parent-Child cardinality
        """
        # Store the allowed parent-child relationship
        if parent_class not in self.containments:
            self.containments[parent_class] = []

        self.containments[parent_class].append(child_class)

        # Store the cardinality
        pair = (parent_class, child_class)

        self.cardinality[pair] = [lower, upper]

    def validate(self, all_instances, updated, created, deleted):
        """ Validate the constraints in this Constraints instance

        Args:
            all_instances (list): List of instances to validate
            updated (list): List of updated objects
            created (list): List of created objects
            deleted (list): List of deleted objects
        """
        def get_children_with_class_name(_parent_name, _all_instances,
                                         _class_name):
            """ Helper method to count the number of children of the given
            class in the list of all instances
            """
            _current_children = []

            for child in _all_instances:

                if ',' not in child.dn:
                    continue

                if self.get_parent_name_for_dn(child.dn) == _parent_name \
                        and child.class_name == _class_name:
                    _current_children.append(child)

            return _current_children

        def constraint_exists_for_child(_class_name):
            """ Return True if there exists a constraint for the given class
            as a child
            """
            for _child_classes in list(self.containments.values()):
                if _class_name in _child_classes:
                    return True

            return False

        # Validate containments affected by create or delete
        deleted_objs = [self.get_object_no_runtime(dn) for dn in deleted]

        for obj in itertools.chain(created, deleted_objs):
            parent_name = self.get_parent_name_for_dn(obj.dn)

            # Handle the case where there is no parent
            if not parent_name:
                if obj in created \
                        and constraint_exists_for_child(obj.class_name):
                    error_string = "ERROR: Cannot create %s, %s must have a " \
                                   "parent" % (obj.dn, obj.SaImmAttrClassName)
                    raise SafException(eSaAisErrorT.SA_AIS_ERR_INVALID_PARAM,
                                       error_string)
                # Allow this operation and check the next one
                continue

            # Handle the case where the parent is also deleted
            if parent_name in deleted:
                continue

            # Avoid looking up the parent class in IMM if possible
            parent_obj = [obj for obj in all_instances
                          if obj.dn == parent_name]
            if parent_obj:
                parent_class = parent_obj[0].class_name
            else:
                parent_class = self.get_class_name_for_dn(dn=parent_name)

            # Ignore children where no constraint is defined for the child or
            # the parent
            if parent_class not in self.containments \
                    and not constraint_exists_for_child(obj.class_name):
                continue

            # Validate the containment if there is a parent
            child_classes = self.containments[parent_class]

            # Reject the create if the child's class is not part of the allowed
            # child classes
            if obj.class_name not in child_classes and obj in created:
                error_string = "ERROR: Cannot create %s as a child under %s." \
                               " Possible children are %s" % \
                               (obj.dn, parent_class, child_classes)
                raise SafException(eSaAisErrorT.SA_AIS_ERR_INVALID_PARAM,
                                   error_string)

            # Count current containments
            current_children = get_children_with_class_name(parent_name,
                                                            all_instances,
                                                            obj.class_name)
            # Validate the number of children of the specific class to the
            # given parent
            lower, upper = self.cardinality[(parent_class, obj.class_name)]

            if lower and len(current_children) < lower:
                error_string = ("ERROR: Must have at least %s instances of %s "
                                "under %s") % \
                    (lower, obj.class_name, parent_class)
                raise SafException(eSaAisErrorT.SA_AIS_ERR_FAILED_OPERATION,
                                   error_string)

            if upper and len(current_children) > upper:
                error_string = ("ERROR: Must have at most %s instances of %s "
                                "under %s") % \
                    (upper, obj.class_name, parent_class)
                raise SafException(eSaAisErrorT.SA_AIS_ERR_FAILED_OPERATION,
                                   error_string)


class Implementer(OiAgent):
    """ Class representing an object implementer """
    def __init__(self, class_names=None, name="wrapper", on_create=None,
                 on_delete=None, on_modify=None, on_validate=None,
                 on_apply=None, on_runtime_values_get=None,
                 admin_operations=None, constraints=None, version=None):
        """ Create an Implementer instance """
        # Initialize OI agent
        super(Implementer, self).__init__(version=version)

        self.class_names = class_names
        self.name = name
        self.on_create_cb = on_create
        self.on_delete_cb = on_delete
        self.on_modify_cb = on_modify
        self.on_validate_cb = on_validate
        self.on_apply_cb = on_apply
        self.on_runtime_values_get_cb = on_runtime_values_get
        self.admin_operations = admin_operations
        self.constraints = constraints
        self.completed_ccbs = {}
        self.ccbs = {}
        self.implemented_names = []

        # Register OI callbacks
        self._register_callbacks()

        # Initialize OI API and register as implementer for the classes
        rc = self._register()
        if rc != eSaAisErrorT.SA_AIS_OK:
            raise Exception("ERROR: Can't register a implementer")

    @staticmethod
    def unmarshal_len_array(c_array, length, value_type):
        """ Convert C array with a known length to a Python list

        Args:
            c_array (C array): Array in C
            length (int): Length of array
            value_type (str): Element type in array

        Returns:
            list: The list converted from c_array
        """
        if not c_array:
            return []
        ctype = c_array[0].__class__
        if ctype is str or (PY3 and ctype is SaStringT):
            return unmarshalSaStringTArray(c_array)
        val_list = []
        i = 0
        for ptr in c_array:
            if i == length:
                break
            if not ptr:
                break
            val = unmarshalSaImmValue(ptr, value_type)
            val_list.append(val)
            i = i + 1

        return val_list

    def _register_callbacks(self):
        """ Register OI callbacks """
        # OI callbacks
        self.callbacks = saImmOi.SaImmOiCallbacksT_2()

        self.callbacks.saImmOiCcbAbortCallback = \
            saImmOi.SaImmOiCcbAbortCallbackT(self._ccb_abort_callback)
        self.callbacks.saImmOiCcbApplyCallback = \
            saImmOi.SaImmOiCcbApplyCallbackT(self._ccb_apply_callback)
        self.callbacks.saImmOiCcbCompletedCallback = \
            saImmOi.SaImmOiCcbCompletedCallbackT(self._ccb_completed_callback)
        self.callbacks.saImmOiCcbObjectCreateCallback = \
            saImmOi.SaImmOiCcbObjectCreateCallbackT_2(
                self._ccb_create_callback)
        self.callbacks.saImmOiCcbObjectDeleteCallback = \
            saImmOi.SaImmOiCcbObjectDeleteCallbackT(self._ccb_delete_callback)
        self.callbacks.saImmOiCcbObjectModifyCallback = \
            saImmOi.SaImmOiCcbObjectModifyCallbackT_2(
                self._ccb_modify_callback)
        self.callbacks.saImmOiRtAttrUpdateCallback = \
            saImmOi.SaImmOiRtAttrUpdateCallbackT(self._attr_update_callback)
        self.callbacks.saImmOiAdminOperationCallback = \
            saImmOi.SaImmOiAdminOperationCallbackT_2(
                self._admin_operation_callback)

    def _admin_operation_callback(self, oi_handle, c_invocation_id, c_name,
                                  c_operation_id, c_params):
        """ Callback for administrative operations

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            c_invocation_id (SaInvocationT): Invocation id
            c_name (SaNameT): Pointer to object name
            c_operation_id (SaImmAdminOperationIdT): Operation id
            c_params (SaImmAdminOperationParamsT_2): Pointer to an array of
                pointers to parameter descriptors
        """
        # Unmarshal parameters
        invocation_id = c_invocation_id
        name = \
            saImm.unmarshalSaImmValue(c_name,
                                      eSaImmValueTypeT.SA_IMM_ATTR_SANAMET)
        operation_id = c_operation_id

        params = []

        for param in unmarshalNullArray(c_params):
            param_name = param.paramName
            param_type = param.paramType
            param_buffer = param.paramBuffer

            value = saImm.unmarshalSaImmValue(param_buffer, param_type)
            parameter = AdminOperationParameter(param_name, param_type, value)
            params.append(parameter)

        # Invoke the operation
        result = self.admin_operation(operation_id, name, params)

        # Report the result
        try:
            self.report_admin_operation_result(invocation_id, result)
        except SafException as err:
            print("ERROR: Failed to report that %s::%s returned %s (%s)" %
                  (name, invocation_id, result, err.msg))

    def _ccb_abort_callback(self, oi_handle, ccb_id):
        """ Callback for aborted CCB
        The aborted CCB will be removed from the cache.

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            ccb_id (SaImmOiCcbIdT): CCB id
        """
        del self.ccbs[ccb_id]

    def _ccb_apply_callback(self, oi_handle, ccb_id):
        """ Callback for apply of CCBs

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            ccb_id (SaImmOiCcbIdT): CCB id
        """
        all_instances = []

        for class_name in self.implemented_names:
            dns = self.get_object_names_for_class(class_name)
            for dn in dns:
                obj = self.get_object_no_runtime(dn)
                all_instances.append(obj)

        updated = self.completed_ccbs[ccb_id]['updated']
        created = self.completed_ccbs[ccb_id]['added']
        deleted = self.completed_ccbs[ccb_id]['removed']

        # Remove the CCB from the caches
        del self.ccbs[ccb_id]
        del self.completed_ccbs[ccb_id]

        # Tell the implementer to apply the changes
        return self.on_apply(all_instances, updated, created, deleted)

    def _attr_update_callback(self, oi_handle, c_name, c_attr_names):
        """ Callback for attribute update operation

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            c_name (SaNameT): Pointer to object name
            c_attr_names (SaImmAttrNameT): Pointer to array of attribute names

        Returns:
            SaAisErrorT: Return code of the attribute update callback
        """
        # Unmarshal parameters
        name = \
            saImm.unmarshalSaImmValue(c_name,
                                      eSaImmValueTypeT.SA_IMM_ATTR_SANAMET)
        attr_names = unmarshalNullArray(c_attr_names)

        # Get the class of the object
        class_name = self.get_class_name_for_dn(dn=name)

        # Get the values from the user and report back
        attributes = {}

        for attr_name in attr_names:
            values = self.on_runtime_values_get(name, class_name, attr_name)
            if values is None:
                return eSaAisErrorT.SA_AIS_ERR_UNAVAILABLE
            if not isinstance(values, list):
                values = [values]

            attributes[attr_name] = values

        # Report the updated values for the attributes
        try:
            self.update_runtime_object(name, attributes)
            return eSaAisErrorT.SA_AIS_OK
        except SafException:
            return eSaAisErrorT.SA_AIS_ERR_FAILED_OPERATION

    def _ccb_delete_callback(self, oi_handle, ccb_id, c_name):
        """ Callback for object delete operation

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            ccb_id (SaImmOiCcbIdT): CCB id
            c_name (SaNameT): Pointer to object name

        Returns:
            SaAisErrorT: Return code of the object delete callback
        """
        # Unmarshal the parameters
        name = \
            saImm.unmarshalSaImmValue(c_name,
                                      eSaImmValueTypeT.SA_IMM_ATTR_SANAMET)

        # Create a new CCB in the cache if needed
        if ccb_id not in list(self.ccbs.keys()):
            self.ccbs[ccb_id] = []

        # Cache the operation
        self.ccbs[ccb_id].append({'type': 'DELETE', 'dn': name})

        # Tell the implementer about the operation
        return self.on_delete_added(name)

    def _ccb_modify_callback(self, oi_handle, ccb_id, c_name,
                             c_attr_modification):
        """ Callback for object modify operation

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            ccb_id (SaImmOiCcbIdT): CCB id
            c_name (SaNameT): Pointer to object name
            c_attr_modification (SaImmAttrModificationT_2): Pointer to an array
                of pointers to descriptors of the modifications to perform

        Returns:
            SaAisErrorT: Return code of the object modify callback
        """
        # Unmarshal the parameters
        name = \
            saImm.unmarshalSaImmValue(c_name,
                                      eSaImmValueTypeT.SA_IMM_ATTR_SANAMET)

        attribute_modifications = []
        implementer_objection = None

        for attr in unmarshalNullArray(c_attr_modification):
            attr_name = str(attr.modAttr.attrName)
            attr_type = attr.modAttr.attrValueType
            mod_type = attr.modType
            attr_values = self.unmarshal_len_array(
                attr.modAttr.attrValues, attr.modAttr.attrValuesNumber,
                attr.modAttr.attrValueType)
            attribute_modifications.append({'attribute': attr_name,
                                            'type': attr_type,
                                            'modification': mod_type,
                                            'values': attr_values})

            # Tell the implementer about the modification
            result = self.on_modify_added(attr_name, mod_type, attr_values)
            if result != eSaAisErrorT.SA_AIS_OK:
                implementer_objection = result

        # Create a new CCB in the cache if needed
        if ccb_id not in list(self.ccbs.keys()):
            self.ccbs[ccb_id] = []

        # Store the modifications in the cache
        self.ccbs[ccb_id].append({'type': 'MODIFY',
                                  'dn': name,
                                  'modification': attribute_modifications})

        # Respond and say if this is accepted by the implementer
        if implementer_objection:
            return implementer_objection

        return eSaAisErrorT.SA_AIS_OK

    def _ccb_create_callback(self, oi_handle, ccb_id, class_name,
                             c_parent, c_attr_values):
        """ Callback for object create operation

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            ccb_id (SaImmOiCcbIdT): CCB id
            class_name (SaImmClassNameT): Class name
            c_parent (SaNameT): Pointer to name of object's parent
            c_attr_values (SaImmAttrValuesT_2): Pointer to an array of pointers
                                                to attribute descriptors

        Returns:
            SaAisErrorT: Return code of the object create callback
        """
        # Unmarshal parameters
        parent = saImm.unmarshalSaImmValue(
            c_parent, eSaImmValueTypeT.SA_IMM_ATTR_SANAMET)
        attributes = {}

        for attr in unmarshalNullArray(c_attr_values):
            attr_name = str(attr.attrName)
            attr_type = attr.attrValueType
            nr_values = attr.attrValuesNumber

            attr_values = self.unmarshal_len_array(attr.attrValues,
                                                   nr_values, attr_type)
            if attr_values:
                attributes[attr_name] = attr_values
            else:
                attributes[attr_name] = None

        # Fill in any missing attributes
        _, description = self.imm_om.get_class_description(class_name)

        for attribute in description:
            if str(attribute.attrName) not in attributes:
                attributes[str(attribute.attrName)] = None

        # Create a new CCB in the cache if needed
        if ccb_id not in list(self.ccbs.keys()):
            self.ccbs[ccb_id] = []

        # Cache the create operation
        self.ccbs[ccb_id].append({'type': 'CREATE',
                                  'parent': parent,
                                  'className': str(class_name),
                                  'attributes': attributes})

        # Tell the implementer about the operation
        obj = self.create_non_existing_imm_object(class_name, parent,
                                                  attributes)

        return self.on_create_added(class_name, parent, obj)

    def _ccb_completed_callback(self, oi_handle, ccb_id):
        """ Callback for completed CCB

        Validate any configured containments and call the configured
        on_validate function

        Args:
            oi_handle (SaImmOiHandleT): OI handle
            ccb_id (SaImmOiCcbIdT): CCB id

        Returns:
            SaAisErrorT: Return code of the CCB-completed validation
        """
        # Get a summary of the changes in the CCB
        summary = self._collect_full_transaction(ccb_id)
        instances = summary['instances_after']
        created = summary['created']
        deleted = summary['deleted']
        updated = summary['updated']

        # Store added, removed, updated for apply
        self.completed_ccbs[ccb_id] = {'added': created,
                                       'removed': deleted,
                                       'updated': updated}

        # Perform validation on the full transaction
        return self.validate(ccb_id, instances, updated, created, deleted)

    def _collect_full_transaction(self, ccb_id):
        """ Go through a completed CCB and summarize the full transaction as
            seen by the OI

        Args:
            ccb_id (str): CCB id

        Returns:
        {'instances_after' : instances,
         'created': created,
         'deleted': deleted,
         'updated': updated}
        """
        # Collect current state
        all_objects_now = []

        created = []
        deleted = []
        updated = []

        # Go through current instances
        for class_name in self.implemented_names:
            dns = self.get_object_names_for_class(class_name)
            for dn in dns:
                obj = self.get_object_no_runtime(dn)
                all_objects_now.append(obj)

        # Collect proposed state by applying changes on current state
        for operation in self.ccbs[ccb_id]:
            operation_type = operation['type']

            # Handle object create operation
            if operation_type == 'CREATE':
                parent = operation['parent']
                class_name = operation['className']
                attributes = operation['attributes']
                rdn_attr = self.imm_om.get_rdn_attribute_for_class(
                    class_name=class_name)
                rdn_value = attributes[rdn_attr][0]

                if parent:
                    dn = '%s,%s' % (rdn_value, parent)
                else:
                    dn = rdn_value

                instance = self.create_non_existing_imm_object(
                    class_name, parent, attributes)
                created.append(instance)
                deleted = [obj for obj in deleted if obj.dn != dn]

            # Handle object delete operation
            elif operation_type == 'DELETE':
                dn = operation['dn']
                deleted.append(ImmObject(class_name=class_name, dn=dn))
                created = [obj for obj in created if obj.dn != dn]
                updated = [obj for obj in updated if obj.dn != dn]

            # Handle object modify operation
            elif operation_type == 'MODIFY':
                dn = operation['dn']
                modifications = operation['modification']

                for attr_modification in modifications:
                    mod_type = attr_modification['modification']
                    attribute = attr_modification['attribute']
                    values = attr_modification['values']

                    # Find affected object
                    affected_instance = None
                    affected_instances = [obj for obj in all_objects_now
                                          if obj.dn == dn]

                    if not affected_instances:
                        print('ERROR: Failed to find object %s affected by '
                              'modify operation' % dn)
                    else:
                        affected_instance = affected_instances[0]
                        if affected_instance not in updated:
                            updated.append(affected_instance)

                    if mod_type == \
                            eSaImmAttrModificationTypeT.SA_IMM_ATTR_VALUES_ADD:
                        curr_value = affected_instance.__getattr__(attribute)
                        curr_value.append(values)
                        affected_instance.__setattr__(attribute, curr_value)

                    elif mod_type == eSaImmAttrModificationTypeT.\
                            SA_IMM_ATTR_VALUES_DELETE:
                        for value in values:
                            curr_value = affected_instance.__getattr__(
                                attribute)
                            curr_value.remove(value)
                            affected_instance.__setattr__(
                                attribute, curr_value)

                    elif mod_type == eSaImmAttrModificationTypeT.\
                            SA_IMM_ATTR_VALUES_REPLACE:
                        affected_instance.__setattr__(attribute, values)

        # Return the summary
        instances_after = []

        for modify_obj in itertools.chain(all_objects_now, created):
            is_deleted = False
            for obj in deleted:
                if obj.dn == modify_obj.dn:
                    is_deleted = True

            if not is_deleted:
                instances_after.append(modify_obj)

        out = {'instances_after': instances_after,
               'created': created,
               'deleted': deleted,
               'updated': updated}

        return out

    def get_implemented_classes(self):
        """ Return a list of the classes this implementer implements

        Returns:
            list: List of class name
        """
        return self.implemented_names

    def set_class_implementer(self, class_name):
        """ Add the given class_name to the list of classes this implementer
        implements

        Args:
            class_name (str): Class name

        Returns:
            SaAisErrorT: Return code of class implementer set
        """
        c_class_name = SaImmClassNameT(class_name)
        rc = saImmOiClassImplementerSet(self.handle, c_class_name)
        if rc == eSaAisErrorT.SA_AIS_OK:
            self.implemented_names.append(class_name)
        else:
            log_err("saImmOiClassImplementerSet FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def set_constraints(self, constraints):
        """ Set constraints to be verified by the OI """
        self.constraints = constraints

    def set_admin_operations(self, admin_operations):
        """ Set the admin operations to be executed by the OI """
        self.admin_operations = admin_operations

    def on_runtime_values_get(self, name, class_name, attribute_name):
        """ Retrieve values for the requested attribute in the given instance

        Args:
            name (str): Object name
            class_name (str): Class name
            attribute_name (str): Object name

        Returns:
            SaAisErrorT: Return code of runtime values get
        """
        if self.on_runtime_values_get_cb:
            try:
                return self.on_runtime_values_get_cb(name, class_name,
                                                     attribute_name)
            except SafException as err:
                return err.value
        else:
            return None

    def on_modify_added(self, attribute_name, modification_type, values):
        """ Called when an object modify operation has been added to an
        ongoing CCB.
        Will call the on_modify parameter if it has been set. This method can
        also be overridden by a subclass that wants to handle on_modify_added.

        Args:
            attribute_name (str): Attribute name
            modification_type (str): Modification type
            values (list): List of values

        Returns:
            SaAisErrorT: Return code of the attr values addition
        """
        if self.on_modify_cb:
            try:
                return self.on_modify_cb(attribute_name,
                                         modification_type, values)
            except SafException as err:
                return err.value

        return eSaAisErrorT.SA_AIS_OK

    def on_delete_added(self, dn):
        """ Called when an object delete operation has been added to the
        active CCB.
        Will call the on_delete parameter if it has been set. This method can
        also be overridden by a subclass that wants to handle on_delete_added.

        Args:
            dn (str): Object dn

        Returns:
            SaAisErrorT: Return code of the attr values deletion
        """
        if self.on_delete_cb:
            try:
                return self.on_delete_cb(dn)
            except SafException as err:
                return err.value

        return eSaAisErrorT.SA_AIS_OK

    def on_create_added(self, class_name, parent_name, obj):
        """ Called when an object create operation has been performed.
        Will call the on_create parameter if it has been set. This method can
        also be overridden by a subclass that wants to handle on_create_added.

        Args:
            class_name (str): Class name
            parent_name (str): Parent name
            obj (ImmObject): Imm object
        """
        if self.on_create_cb:
            try:
                return self.on_create_cb(class_name, parent_name, obj)
            except SafException as err:
                return err.value

        return eSaAisErrorT.SA_AIS_OK

    def on_apply(self, instances, updated, created, deleted):
        """ Call the registered on_apply callback function if it exists
        Another way to perform apply is to override this method.

        Args:
            instances (list): List instances of class
            updated (list): List updated objects
            created (list): List created objects
            deleted (list): List deleted objects

        Returns:
            SaAisErrorT: Return code of the applying
        """
        if self.on_apply_cb:
            try:
                self.on_apply_cb(instances, updated, created, deleted)
            except SafException as err:
                return err.value

        return eSaAisErrorT.SA_AIS_OK

    def on_validate(self, instances, updated, created, deleted):
        """ Call the registered on_validate callback function if it exists.
        Another way to perform validation is to override this method.

        Args:
            instances (list): List of instances to validate
            updated (list): List of updated objects
            created (list): List of created objects
            deleted (list): List of deleted objects

        Returns:
            SaAisErrorT: Return code of the validation
        """
        if self.on_validate_cb:
            self.on_validate_cb(instances, updated, created, deleted)

    def validate(self, ccb_id, instances, updated, created, deleted):
        """ Perform any configured constraints validation """
        try:
            # Validate constraints on the containments (if configured)
            self._validate_constraints(instances, updated, created, deleted)

            # Let the user code validate the CCB (if configured)
            self.on_validate(instances, updated, created, deleted)
        except SafException as err:
            self.set_error_string(ccb_id, err.msg)
            return err.value
        except Exception:
            return eSaAisErrorT.SA_AIS_ERR_FAILED_OPERATION

        return eSaAisErrorT.SA_AIS_OK

    def enter_dispatch_loop(self):
        """ Activate the OI and start an infinite dispatch loop """
        self._start_dispatch_loop()

    def admin_operation(self, operation_id, object_name, parameters):
        """ Execute the specified admin operation and return the result
        Return eSaAisErrorT.SA_AIS_ERR_NOT_SUPPORTED if no corresponding
        operation is registered

        Args:
            operation_id (str): Operation id
            object_name (str): Object name
            parameters (list): List of parameters

        Returns:
            SaAisErrorT: Return code of admin operation
        """
        # Get the class name
        class_name = self.get_class_name_for_dn(object_name)

        # Find and execute a matching admin operation
        if self.admin_operations:
            for admin_op in self.admin_operations:
                if admin_op.matches(class_name, operation_id):
                    try:
                        admin_op.execute(object_name, parameters)
                        return eSaAisErrorT.SA_AIS_OK
                    except SafException as err:
                        print("ERROR: Admin operation %s caused exception %s" %
                              (operation_id, err))
                        return err.value

        # Scan for AdminOperation-decorated functions in subclasses
        for member_name in dir(self):
            func = getattr(self, member_name)

            tmp_id = getattr(func, 'AdminOperationOperationId', None)
            tmp_class_name = getattr(func, 'AdminOperationClassName', None)

            if tmp_id == operation_id and tmp_class_name == class_name:
                try:
                    func(object_name, parameters)
                    return eSaAisErrorT.SA_AIS_OK
                except SafException as err:
                    print("ERROR: Admin operation %s caused exception %s" %
                          (operation_id, err))
                    return err.value

        # Report that the operation is not supported
        return eSaAisErrorT.SA_AIS_ERR_NOT_SUPPORTED

    @bad_handle_retry
    def _register(self):
        """ Initialize IMM OI and register as an OI for the configured classes

        Returns:
            SaAisErrorT: Return code of Implementer register
        """
        # Initialize the OI API
        rc = self.re_initialize()

        # Ensure that all classes are configuration classes
        runtime_classes = None
        if self.class_names is not None:
            runtime_classes = [item for item in self.class_names
                               if self.imm_om.get_class_category(item) ==
                               eSaImmClassCategoryT.SA_IMM_CLASS_RUNTIME]
        if runtime_classes:
            raise Exception("ERROR: Can't be an applier for runtime "
                            "classes %s" % runtime_classes)

        # Become an implementer
        if rc == eSaAisErrorT.SA_AIS_OK:
            rc = self._register_implementer(self.name)

        available_classes = self.get_available_classes_in_imm()

        if rc == eSaAisErrorT.SA_AIS_OK:
            if self.class_names is not None:
                for class_name in self.class_names:
                    if class_name in available_classes:
                        rc = self.set_class_implementer(class_name)
                        if rc != eSaAisErrorT.SA_AIS_OK:
                            break
                    else:
                        print("WARNING: %s is missing in IMM. Not becoming "
                              "implementer." % class_name)
        return rc

    def _register_implementer(self, oi_name):
        """ Register as an implementer

        Args:
            oi_name (str): Implementer name

        Returns:
            SaAisErrorT: Return code of implementer set
        """
        implementer_name = SaImmOiImplementerNameT(oi_name)
        rc = saImmOiImplementerSet(self.handle, implementer_name)

        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiClassImplementerSet FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def update_runtime_attributes(self, dn, attributes):
        """ Update the given runtime attributes for the specified dn

        Args:
            dn (str): Object dn
            attributes (dict): Dictionary of object attributes
        """
        # Report the updates
        try:
            self.update_runtime_object(dn, attributes)
        except SafException as err:
            print("ERROR: Failed to update runtime attributes of %s: %s" %
                  (dn, err))

    def create(self, obj):
        """ Create the runtime object with provided information

        Args:
            obj (ImmObject): Object to create

        Returns:
            SaAisErrorT: Return code of implementer object create
        """
        # Get the parent name for the object
        parent_name = self.get_parent_name_for_dn(obj.dn)
        class_name = obj.class_name

        # Create the object
        return self.create_runtime_object(class_name, parent_name, obj)

    def delete(self, dn):
        """ Delete a runtime object with the given dn

        Args:
            dn (str): Object dn

        Returns:
            SaAisErrorT: Return code of implementer object delete
        """
        return self.delete_runtime_object(dn)

    def _start_dispatch_loop(self):
        """ Start an infinite dispatch loop """
        read_fds = [self.selection_object.value]

        # Handle updates
        while read_fds:
            read_evt, _, _ = select.select(read_fds, [], read_fds)
            if read_evt:
                rc = self.dispatch()
                if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                    rc = self._register()
                    if rc != eSaAisErrorT.SA_AIS_OK:
                        raise Exception("ERROR: Can't re-register as applier")
                    read_fds = [self.selection_object.value]

    def _validate_constraints(self, all_instances, updated, created, deleted):
        """ Validate configured constraints

       Args:
            all_instances (list): List of instances to validate
            updated (list): List of updated objects
            created (list): List of created objects
            deleted (list): List of deleted objects

        Returns:
            SaAisErrorT: Return code of the validation of constraints
        """
        if self.constraints:
            self.constraints.validate(all_instances, updated, created, deleted)


class Applier(Implementer):
    """ Class representing an applier """
    def __init__(self, class_names, name="wrapper", on_create=None,
                 on_delete=None, on_modify=None, on_apply=None,
                 version=None):
        """ Constructor for Applier instance """
        # Initialize the base class
        Implementer.__init__(self, class_names=class_names, name=name,
                             on_create=on_create, on_delete=on_delete,
                             on_modify=on_modify, on_apply=on_apply,
                             version=version)

    @staticmethod
    def _validate(ccb_id, instances, updated, created, deleted):
        """ Empty validate handler as appliers cannot validate """
        return eSaAisErrorT.SA_AIS_OK

    @bad_handle_retry
    def _register(self):
        """ Initialize IMM-OI interface and register as an applier for the
        configured classes

        Returns:
            SaAisErrorT: Return code of applier register
        """
        # Initialize the OI API
        rc = self.re_initialize()

        # Ensure that all classes are configuration classes
        runtime_classes = [item for item in self.class_names
                           if self.imm_om.get_class_category(item) ==
                           eSaImmClassCategoryT.SA_IMM_CLASS_RUNTIME]
        if runtime_classes:
            raise Exception("ERROR: Can't be an applier for runtime classes %s"
                            % runtime_classes)
        # Become an applier
        if rc == eSaAisErrorT.SA_AIS_OK:
            rc = self._register_applier(self.name)

        # Register as applier for each class
        available_classes = self.get_available_classes_in_imm()

        for class_name in self.class_names:
            if class_name in available_classes:
                self.set_class_implementer(class_name)
            else:
                print("WARNING: %s is missing in IMM. Not becoming applier." %
                      class_name)
        return rc

    def _register_applier(self, app_name):
        """ Register as an applier OI

        Args:
            app_name (str): Applier name

        Returns:
            SaAisErrorT: Return code of applier set
        """
        applier_name = "@" + app_name
        rc = self._register_implementer(applier_name)
        return rc
