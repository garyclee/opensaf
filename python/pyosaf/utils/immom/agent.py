############################################################################
#
# (C) Copyright 2014 The OpenSAF Foundation
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
""" IMM OM common utilities """
import uuid
from copy import deepcopy
from ctypes import pointer

from pyosaf import saImmOm, saImm
from pyosaf.saAis import saAis, SaVersionT, SaNameT, SaAisErrorT, \
    eSaAisErrorT, eSaBoolT, unmarshalNullArray
from pyosaf.saImm import eSaImmScopeT, SaImmClassNameT, SaImmAttrNameT
from pyosaf.utils import decorate, initialize_decorate, log_err

# Decorate pure saImmOm* API's with error-handling retry and exception raising
saImmOmInitialize = initialize_decorate(saImmOm.saImmOmInitialize)
saImmOmSelectionObjectGet = decorate(saImmOm.saImmOmSelectionObjectGet)
saImmOmDispatch = decorate(saImmOm.saImmOmDispatch)
saImmOmFinalize = decorate(saImmOm.saImmOmFinalize)
saImmOmClassCreate_2 = decorate(saImmOm.saImmOmClassCreate_2)
saImmOmClassDescriptionGet_2 = decorate(saImmOm.saImmOmClassDescriptionGet_2)
saImmOmClassDescriptionMemoryFree_2 = \
    decorate(saImmOm.saImmOmClassDescriptionMemoryFree_2)
saImmOmClassDelete = decorate(saImmOm.saImmOmClassDelete)
saImmOmSearchInitialize_2 = decorate(saImmOm.saImmOmSearchInitialize_2)
saImmOmSearchNext_2 = decorate(saImmOm.saImmOmSearchNext_2)
saImmOmSearchFinalize = decorate(saImmOm.saImmOmSearchFinalize)
saImmOmAccessorInitialize = decorate(saImmOm.saImmOmAccessorInitialize)
saImmOmAccessorGet_2 = decorate(saImmOm.saImmOmAccessorGet_2)
saImmOmAccessorFinalize = decorate(saImmOm.saImmOmAccessorFinalize)
saImmOmAdminOwnerInitialize = decorate(saImmOm.saImmOmAdminOwnerInitialize)
saImmOmAdminOwnerSet = decorate(saImmOm.saImmOmAdminOwnerSet)
saImmOmAdminOwnerRelease = decorate(saImmOm.saImmOmAdminOwnerRelease)
saImmOmAdminOwnerFinalize = decorate(saImmOm.saImmOmAdminOwnerFinalize)
saImmOmAdminOwnerClear = decorate(saImmOm.saImmOmAdminOwnerClear)
saImmOmCcbInitialize = decorate(saImmOm.saImmOmCcbInitialize)
saImmOmCcbObjectCreate_2 = decorate(saImmOm.saImmOmCcbObjectCreate_2)
saImmOmCcbObjectDelete = decorate(saImmOm.saImmOmCcbObjectDelete)
saImmOmCcbObjectModify_2 = decorate(saImmOm.saImmOmCcbObjectModify_2)
saImmOmCcbApply = decorate(saImmOm.saImmOmCcbApply)
saImmOmCcbFinalize = decorate(saImmOm.saImmOmCcbFinalize)
saImmOmCcbGetErrorStrings = decorate(saImmOm.saImmOmCcbGetErrorStrings)
saImmOmAdminOperationInvoke_2 = decorate(saImmOm.saImmOmAdminOperationInvoke_2)
saImmOmAdminOperationInvokeAsync_2 = \
    decorate(saImmOm.saImmOmAdminOperationInvokeAsync_2)
saImmOmAdminOperationContinue = decorate(saImmOm.saImmOmAdminOperationContinue)
saImmOmAdminOperationContinueAsync = \
    decorate(saImmOm.saImmOmAdminOperationContinueAsync)
saImmOmAdminOperationContinuationClear = \
    decorate(saImmOm.saImmOmAdminOperationContinuationClear)


class OmAgentManager(object):
    """ This class manages the life cycle of an IMM OM agent """
    def __init__(self, version=None):
        """ Constructor for OmAgentManager class

        Args:
            version (SaVersionT): IMM OM API version
        """
        self.init_version = version if version else SaVersionT('A', 2, 15)
        self.version = None
        self.handle = None
        self.callbacks = None
        self.selection_object = None

    def __enter__(self):
        """ Enter method for OmAgentManager class """
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        """ Exit method for OmAgentManager class

        Finalize the IMM OM agent handle
        """
        if self.handle is not None:
            saImmOmFinalize(self.handle)
            self.handle = None

    def __del__(self):
        """ Destructor for OmAgentManager class

        Finalize the IMM OM agent handle
        """
        if self.handle is not None:
            saImmOm.saImmOmFinalize(self.handle)
            self.handle = None

    def initialize(self):
        """ Initialize the IMM OM library

        Returns:
            SaAisErrorT: Return code of the saImmOmInitialize() API call
        """
        self.handle = saImmOm.SaImmHandleT()
        self.version = deepcopy(self.init_version)
        rc = saImmOmInitialize(self.handle, None, self.version)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOmInitialize FAILED - %s" % eSaAisErrorT.whatis(rc))

        return rc

    def get_handle(self):
        """ Return the IMM OM agent handle successfully initialized

        Returns:
            SaImmHandleT: IMM OM agent handle
        """
        return self.handle

    def dispatch(self, flags):
        """ Invoke IMM callbacks for queued events

        Args:
            flags (eSaDispatchFlagsT): Flags specifying dispatch mode

        Returns:
            SaAisErrorT: Return code of the saImmOmDispatch() API call
        """
        rc = saImmOmDispatch(self.handle, flags)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOmDispatch FAILED - %s" % eSaAisErrorT.whatis(rc))

        return rc

    def finalize(self):
        """ Finalize the IMM OM agent handle

        Returns:
            SaAisErrorT: Return code of the saImmOmFinalize() API call
        """
        rc = eSaAisErrorT.SA_AIS_OK
        if self.handle:
            rc = saImmOmFinalize(self.handle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmFinalize FAILED - %s" %
                        eSaAisErrorT.whatis(rc))

            if rc == eSaAisErrorT.SA_AIS_OK \
                    or rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                # If the Finalize() call returned BAD_HANDLE, the handle should
                # already become stale and invalid, so we reset it anyway.
                self.handle = None
        return rc


class ImmOmAgent(OmAgentManager):
    """ This class acts as a high-level IMM OM agent, providing IMM OM
    functions to the users at a higher level, and relieving the users of the
    need to manage the life cycle of the IMM OM agent """

    def init(self):
        """ Initialize the IMM OM agent

        Returns:
            SaAisErrorT: Return code of the corresponding IMM API call(s)
        """
        # Clean previous resource if any
        self.finalize()
        return self.initialize()

    def clear_admin_owner(self, obj_name, scope=eSaImmScopeT.SA_IMM_SUBTREE):
        """ Clear the admin owner for the set of object identified by the scope
        and obj_name parameters

        Args:
            obj_name (str): Object name
            scope (SaImmScopeT): Scope of the clear operation

        Returns:
            SaAisErrorT: Return code of the saImmOmAdminOwnerClear() API call
        """
        if not obj_name:
            rc = eSaAisErrorT.SA_AIS_ERR_NOT_EXIST
        else:
            obj_name = SaNameT(obj_name)
            obj_name = [obj_name]

            rc = saImmOmAdminOwnerClear(self.handle, obj_name, scope)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmAdminOwnerClear FAILED - %s" %
                        eSaAisErrorT.whatis(rc))

        return rc

    def get_class_description(self, class_name):
        """ Get class description as a Python list

        Args:
            class_name (str): Class name

        Returns:
            SaAisErrorT: Return code of the corresponding IMM API call(s)
            list: List of class attributes
        """
        # Perform a deep copy
        def attr_def_copy(attr_def):
            """ Deep copy attributes """
            attr_def_cpy = saImm.SaImmAttrDefinitionT_2()
            attr_def_cpy.attrName = SaImmAttrNameT(
                deepcopy(str(attr_def.attrName)))
            attr_def_cpy.attrValueType = attr_def.attrValueType
            attr_def_cpy.attrFlags = attr_def.attrFlags

            return attr_def_cpy

        class_attrs = []
        attr_defs = pointer(pointer(saImm.SaImmAttrDefinitionT_2()))
        category = saImm.SaImmClassCategoryT()
        rc = saImmOmClassDescriptionGet_2(self.handle,
                                          SaImmClassNameT(class_name),
                                          category,
                                          attr_defs)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOmClassDescriptionGet_2 FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        else:
            _class_attrs = unmarshalNullArray(attr_defs)

            # Make copy of attr_defs list
            class_attrs = [attr_def_copy(item) for item in _class_attrs]

            # Free the original memory
            saImmOmClassDescriptionMemoryFree_2(self.handle,
                                                attr_defs.contents)

        return rc, class_attrs

    def get_rdn_attribute_for_class(self, class_name):
        """ Return the RDN attribute for the given class
        This is safe to call from OI callbacks.

        Args:
            class_name (str): Class name

        Returns:
            str: RDN attribute of the class
        """
        rc, class_attrs = self.get_class_description(class_name)
        if rc != eSaAisErrorT.SA_AIS_OK:
            return ""
        else:
            for attr_desc in class_attrs:
                if attr_desc.attrFlags & saImm.saImm.SA_IMM_ATTR_RDN:
                    return str(attr_desc.attrName)

    def invoke_admin_operation(self, dn, op_id, params=None):
        """ Invoke admin op for dn

        Args:
            dn (str): Object dn
            op_id (str): Operation id
            params (list): List of parameters

        Returns:
            SaAisErrorT: Return code of the corresponding IMM API call(s)
        """
        _owner = OmAdminOwner(self.handle)
        rc = _owner.init()
        if rc == eSaAisErrorT.SA_AIS_OK:
            index = dn.rfind(",")
            object_rdn = dn[index+1:]
            rc = _owner.set_owner(object_rdn)

        if rc == eSaAisErrorT.SA_AIS_OK:
            owner_handle = _owner.get_handle()
            if params is None:
                params = []

            object_dn = SaNameT(dn)
            operation_rc = SaAisErrorT()

            rc = saImmOmAdminOperationInvoke_2(owner_handle, object_dn, 0,
                                               op_id, params, operation_rc,
                                               saAis.SA_TIME_ONE_SECOND * 10)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmAdminOperationInvoke_2 FAILED - %s" %
                        eSaAisErrorT.whatis(rc))
            elif operation_rc.value != eSaAisErrorT.SA_AIS_OK:
                log_err("Administrative operation(%s) on %s FAILED - %s" %
                        (op_id, object_dn, eSaAisErrorT.whatis(operation_rc)))

        return rc

    def get_class_category(self, class_name):
        """ Return the category of the given class

        Args:
            class_name (str): Class name

        Returns:
            SaImmClassCategoryT: Class category
        """
        c_attr_defs = pointer(pointer(saImmOm.SaImmAttrDefinitionT_2()))
        c_category = saImmOm.SaImmClassCategoryT()
        c_class_name = saImmOm.SaImmClassNameT(class_name)

        rc = saImmOmClassDescriptionGet_2(self.handle, c_class_name,
                                          c_category, c_attr_defs)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOmClassDescriptionGet_2 FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
            return ""
        return c_category.value


class OmAdminOwner(object):
    """ This class encapsulates the ImmOm Admin Owner interface """
    def __init__(self, imm_handle, owner_name=""):
        """ Constructor for OmAdminOwner class

        Args:
            imm_handle (SaImmHandleT): IMM OM agent handle
            owner_name (str): Name of the admin owner
        """
        self.imm_handle = imm_handle
        if owner_name:
            self.owner_name = owner_name
        else:
            self.owner_name = "pyosaf_" + str(uuid.uuid4())

        self.owner_name = saImmOm.SaImmAdminOwnerNameT(self.owner_name)
        self.owner_handle = None

    def __enter__(self):
        """ Enter method for OmAdminOwner class """
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        """ Exit method for OmAdminOwner class

        Finalize the admin owner handle
        """
        if self.owner_handle is not None:
            saImmOm.saImmOmAdminOwnerFinalize(self.owner_handle)
            self.owner_handle = None

    def __del__(self):
        """ Destructor for OmAdminOwner class

        Finalize the admin owner handle
        """
        if self.owner_handle is not None:
            saImmOm.saImmOmAdminOwnerFinalize(self.owner_handle)
            self.owner_handle = None

    def init(self):
        """ Initialize the IMM admin owner interface

        Return:
            SaAisErrorT: Return code of the saImmOmAdminOwnerInitialize() call
        """
        self.owner_handle = saImmOm.SaImmAdminOwnerHandleT()
        rc = saImmOmAdminOwnerInitialize(self.imm_handle, self.owner_name,
                                         eSaBoolT.SA_TRUE, self.owner_handle)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOmAdminOwnerInitialize FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def get_handle(self):
        """ Return the admin owner handle successfully initialized """
        return self.owner_handle

    def set_owner(self, obj_name, scope=eSaImmScopeT.SA_IMM_SUBTREE):
        """ Set admin owner for the set of objects identified by the scope and
        the obj_name parameters.

        Args:
            obj_name (str): Object name
            scope (SaImmScopeT): Scope of the admin owner set operation

        Return:
            SaAisErrorT: Return code of the saImmOmAdminOwnerSet() API call
        """
        if not obj_name:
            rc = eSaAisErrorT.SA_AIS_ERR_NOT_EXIST
        else:
            obj_name = SaNameT(obj_name)
            obj_name = [obj_name]

            rc = saImmOmAdminOwnerSet(self.owner_handle, obj_name, scope)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmAdminOwnerSet FAILED - %s" %
                        eSaAisErrorT.whatis(rc))

        return rc

    def release_owner(self, obj_name, scope=eSaImmScopeT.SA_IMM_SUBTREE):
        """ Release the admin owner for the set of objects identified by the
        scope and obj_name parameters

        Args:
            obj_name (str): Object name
            scope (SaImmScopeT): Scope of the admin owner release operation

        Return:
            SaAisErrorT: Return code of the saImmOmAdminOwnerRelease() API call
        """
        if not obj_name:
            rc = eSaAisErrorT.SA_AIS_ERR_NOT_EXIST
        else:
            obj_name = SaNameT(obj_name)
            obj_name = [obj_name]

            rc = saImmOmAdminOwnerRelease(self.owner_handle, obj_name, scope)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmAdminOwnerRelease FAILED - %s" %
                        eSaAisErrorT.whatis(rc))

        return rc
