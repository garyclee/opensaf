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
"""
IMM OI common utilities

Supported functions:
- Set/clear/release implementer for class/object
- Create/delete/update runtime object
- Get class/object attributes
- Get IMM error strings
"""
from __future__ import print_function
from copy import deepcopy
from ctypes import c_char_p, c_void_p, cast, pointer

from pyosaf.saAis import SaStringT, SaVersionT, SaNameT, SaSelectionObjectT, \
    eSaDispatchFlagsT, eSaAisErrorT
from pyosaf import saImmOi
from pyosaf.saImm import SaImmAttrNameT, SaImmAttrValuesT_2, SaImmClassNameT, \
    SaImmSearchParametersT_2, eSaImmValueTypeT, SaImmAttrModificationT_2, \
    eSaImmAttrModificationTypeT
from pyosaf.saImmOi import SaImmOiHandleT
from pyosaf.utils import log_err, bad_handle_retry, decorate, \
    initialize_decorate
from pyosaf.utils.immom.object import ImmObject
from pyosaf.utils.immom.ccb import marshal_c_array
from pyosaf.utils.immom.iterator import SearchIterator
from pyosaf.utils.immom.agent import ImmOmAgent
from pyosaf.utils.immom.accessor import ImmOmAccessor


OPENSAF_IMM_OBJECT = "opensafImm=opensafImm,safApp=safImmService"


# Decorate pure saImmOi* API's with error-handling retry and exception raising
saImmOiInitialize_2 = initialize_decorate(saImmOi.saImmOiInitialize_2)
saImmOiSelectionObjectGet = decorate(saImmOi.saImmOiSelectionObjectGet)
saImmOiDispatch = decorate(saImmOi.saImmOiDispatch)
saImmOiFinalize = decorate(saImmOi.saImmOiFinalize)
saImmOiImplementerSet = decorate(saImmOi.saImmOiImplementerSet)
saImmOiImplementerClear = decorate(saImmOi.saImmOiImplementerClear)
saImmOiClassImplementerSet = decorate(saImmOi.saImmOiClassImplementerSet)
saImmOiClassImplementerRelease = \
    decorate(saImmOi.saImmOiClassImplementerRelease)
saImmOiObjectImplementerSet = decorate(saImmOi.saImmOiObjectImplementerSet)
saImmOiObjectImplementerRelease = \
    decorate(saImmOi.saImmOiObjectImplementerRelease)
saImmOiRtObjectCreate_2 = decorate(saImmOi.saImmOiRtObjectCreate_2)
saImmOiRtObjectDelete = decorate(saImmOi.saImmOiRtObjectDelete)
saImmOiRtObjectUpdate_2 = decorate(saImmOi.saImmOiRtObjectUpdate_2)
saImmOiAdminOperationResult = decorate(saImmOi.saImmOiAdminOperationResult)
saImmOiAdminOperationResult_o2 = \
    decorate(saImmOi.saImmOiAdminOperationResult_o2)
saImmOiAugmentCcbInitialize = decorate(saImmOi.saImmOiAugmentCcbInitialize)
saImmOiCcbSetErrorString = decorate(saImmOi.saImmOiCcbSetErrorString)


class OiAgent(object):
    """ This class acts as a high-level OI agent, providing OI functions to
    the users as a higher level, and relieving the users of the need to manage
    the life cycle of the OI agent and providing general interface for
    Implementer or Applier used
    """
    def __init__(self, version=None):
        """ Constructor for OiAgent class

        Args:
            version (SaVersionT): OI API version
        """
        self.handle = None
        self.init_version = version if version is not None \
            else SaVersionT('A', 2, 15)
        self.version = None
        self.selection_object = None
        self.callbacks = None
        self.imm_om = None
        self.accessor = None

    def _fetch_sel_obj(self):
        """ Obtain a selection object (OS file descriptor)

        Returns:
            SaAisErrorT: Return code of the saImmOiSelectionObjectGet() API
        """
        rc = saImmOiSelectionObjectGet(self.handle, self.selection_object)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiSelectionObjectGet FAILED - %s" %
                    eSaAisErrorT.whatis(rc))

        return rc

    def initialize(self, callbacks=None):
        """ Initialize the IMM OI agent

        Args:
            callbacks (SaImmOiCallbacksT_2): OI callbacks to register with IMM

        Returns:
            SaAisErrorT: Return code of OI initialize
        """
        self.imm_om = ImmOmAgent(self.init_version)
        rc = self.imm_om.init()

        if rc == eSaAisErrorT.SA_AIS_OK:
            if not self.accessor:
                self.accessor = ImmOmAccessor(self.init_version)
                rc = self.accessor.init()
                if rc != eSaAisErrorT.SA_AIS_OK:
                    log_err("saImmOmAccessorInitialize FAILED - %s" %
                            eSaAisErrorT.whatis(rc))
                    return rc

            self.handle = SaImmOiHandleT()
            self.selection_object = SaSelectionObjectT()
            if callbacks is not None:
                self.callbacks = callbacks
            self.version = deepcopy(self.init_version)
            rc = saImmOiInitialize_2(self.handle, self.callbacks,
                                     self.version)
            if rc == eSaAisErrorT.SA_AIS_OK:
                rc = self._fetch_sel_obj()
                if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                    rc = self.re_initialize()
            else:
                log_err("saImmOiInitialize_2 FAILED - %s" %
                        eSaAisErrorT.whatis(rc))
        else:
            log_err("saImmOmInitialize FAILED - %s" % eSaAisErrorT.whatis(rc))

        return rc

    @bad_handle_retry
    def re_initialize(self):
        """ Re-initialize the IMM OI agent

        Returns:
            SaAisErrorT: Return code of OI initialize
        """
        self.finalize()
        rc = self.initialize()

        return rc

    def finalize(self):
        """ Finalize IMM OI agent handle

        Returns:
            SaAisErrorT: Return code of OI finalize
        """
        rc = eSaAisErrorT.SA_AIS_OK
        if self.handle is not None:
            rc = saImmOiFinalize(self.handle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOiFinalize FAILED - %s" %
                        eSaAisErrorT.whatis(rc))

            if rc == eSaAisErrorT.SA_AIS_OK \
                    or rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                # If the Finalize() call returned BAD_HANDLE, the handle should
                # already become stale and invalid, so we reset it anyway.
                self.handle = None
        return rc

    def get_selection_object(self):
        """ Return the selection object associating with the OI handle

        Returns:
            SaSelectionObjectT: Selection object associated with the OI handle
        """
        return self.selection_object

    def dispatch(self, flags=eSaDispatchFlagsT.SA_DISPATCH_ALL):
        """ Dispatch all queued callbacks

        Args:
            flags (eSaDispatchFlagsT): Flags specifying dispatch mode

        Returns:
            SaAisErrorT: Return code of OI dispatch
        """
        rc = saImmOiDispatch(self.handle, flags)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiDispatch FAILED - %s" % eSaAisErrorT.whatis(rc))
        return rc

    def create_runtime_object(self, class_name, parent_name, runtime_obj):
        """ Create a runtime object

        Args:
            class_name (str): Class name
            parent_name (str): Parent name
            runtime_obj (ImmObject): Runtime object to create

        Returns:
            SaAisErrorT: Return code of OI create runtime object
        """
        # Marshall parameters
        c_class_name = SaImmClassNameT(class_name)
        if parent_name:
            c_parent_name = SaNameT(parent_name)
        else:
            c_parent_name = None

        c_attr_values = []

        for name, (c_attr_type, values) in runtime_obj.attrs.items():
            if values is None:
                values = []
            elif values == [None]:
                values = []

            # Make sure all values are in lists
            if not isinstance(values, list):
                values = [values]

            # Create the values struct
            c_attr = SaImmAttrValuesT_2()
            c_attr.attrName = SaImmAttrNameT(name)
            c_attr.attrValueType = c_attr_type
            c_attr.attrValuesNumber = len(values)

            if not values:
                c_attr.attrValues = None
            else:
                c_attr.attrValues = marshal_c_array(c_attr_type, values)

            c_attr_values.append(c_attr)

        rc = saImmOiRtObjectCreate_2(self.handle, c_class_name,
                                     c_parent_name, c_attr_values)

        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiRtObjectCreate_2 FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def delete_runtime_object(self, dn):
        """ Delete a runtime object

        Args:
            dn (str): Runtime object dn

        Returns:
            SaAisErrorT: Return code of OI delete runtime object
        """
        # Marshall the parameter
        c_dn = SaNameT(dn)
        rc = saImmOiRtObjectDelete(self.handle, c_dn)

        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiRtObjectDelete FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def update_runtime_object(self, dn, attributes):
        """ Update the specified object with the requested attribute
        modifications

        Args:
            dn (str): Object dn
            attributes (dict): Dictionary of attribute modifications

        Returns:
            SaAisErrorT: Return code of OI update runtime object
        """
        # Get the class name for the object
        class_name = self.get_class_name_for_dn(dn)

        # Create and marshall attribute modifications
        attr_mods = []

        for name, values in attributes.items():
            if values is None:
                print("WARNING: Received no values for %s in %s" % (name, dn))
                continue
            if not isinstance(values, list):
                values = [values]

            attr_type = self.get_attribute_type(name, class_name)
            c_attr_mod = SaImmAttrModificationT_2()
            c_attr_mod.modType = \
                eSaImmAttrModificationTypeT.SA_IMM_ATTR_VALUES_REPLACE
            c_attr_mod.modAttr = SaImmAttrValuesT_2()
            c_attr_mod.modAttr.attrName = SaImmAttrNameT(name)
            c_attr_mod.modAttr.attrValueType = attr_type
            c_attr_mod.modAttr.attrValuesNumber = len(values)
            c_attr_mod.modAttr.attrValues = marshal_c_array(attr_type, values)
            attr_mods.append(c_attr_mod)

        rc = saImmOiRtObjectUpdate_2(self.handle, SaNameT(dn), attr_mods)

        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiRtObjectUpdate_2 FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def report_admin_operation_result(self, invocation_id, result):
        """ Report the result of an administrative operation

        Args:
            invocation_id (SaInvocationT): Invocation id
            result (SaAisErrorT): Result of admin operation

        Returns:
            SaAisErrorT: Return code of OI admin operation
        """
        rc = saImmOiAdminOperationResult(self.handle, invocation_id, result)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiAdminOperationResult FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def set_error_string(self, ccb_id, error_string):
        """ Set the error string
        This can only be called from within OI callbacks of a real implementer.

        Args:
            ccb_id (SaImmOiCcbIdT): CCB id
            error_string (str): Error string

        Returns:
            SaAisErrorT: Return code of OI CCB set error string
        """
        c_error_string = SaStringT(error_string)
        rc = saImmOiCcbSetErrorString(self.handle, ccb_id, c_error_string)

        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOiCcbSetErrorString FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
        return rc

    def get_attribute_type(self, attribute, class_name):
        """ Return the type of the attribute in the given class

        Args:
            attribute (str): Attribute name
            class_name (str): Class name

        Returns:
            str: Attribute type
        """
        _, class_desc = self.imm_om.get_class_description(class_name)
        attr_desc = [attr for attr in class_desc if
                     str(attr.attrName) == attribute][0]

        return attr_desc.attrValueType

    @staticmethod
    def get_parent_name_for_dn(dn):
        """ Return the parent's dn of the given object's dn

        Args:
            dn (str): Object dn

        Returns:
            str: DN of the object's parent
        """
        if ',' in dn:
            return dn.split(',', 1)[1]

        return ""

    @staticmethod
    def get_object_names_for_class(class_name):
        """ Return instances of the given class

        Args:
            class_name (str): Class name

        Returns:
            list: List of object names
        """
        # Marshall the search parameter
        c_class_name = c_char_p(class_name)
        c_search_param = SaImmSearchParametersT_2()
        c_search_param.searchOneAttr.attrName = "SaImmAttrClassName"
        c_search_param.searchOneAttr.attrValueType = \
            eSaImmValueTypeT.SA_IMM_ATTR_SASTRINGT
        c_search_param.searchOneAttr.attrValue = \
            cast(pointer(c_class_name), c_void_p)

        # Create the search iterator
        found_objs = SearchIterator(search_param=c_search_param,
                                    attribute_names=['SaImmAttrClassName'])
        found_objs.init()
        # Return the dn's of found objects
        object_names = []
        for obj in found_objs:
            object_names.append(obj.dn)

        return object_names

    def get_class_name_for_dn(self, dn):
        """ Return the class name for an instance with the given dn

        Args:
            dn (str): Object dn

        Returns:
            str: Class name
        """
        _, obj = self.accessor.get(dn, ["SaImmAttrClassName"])
        if not obj:
            return None

        return obj.SaImmAttrClassName

    def get_object_no_runtime(self, dn):
        """ Return the IMM object with the given dn

        Args:
            dn (str): Object dn

        Returns:
            ImmObject: Imm object
        """
        _, obj = self.accessor.get(dn, ['SA_IMM_SEARCH_GET_CONFIG_ATTR'])
        return obj

    def get_available_classes_in_imm(self):
        """ Return a list of all available classes in IMM

        Returns:
            list: List of available classes
        """
        _, opensaf_imm = self.accessor.get(OPENSAF_IMM_OBJECT)
        return opensaf_imm.opensafImmClassNames

    def create_non_existing_imm_object(self, class_name, parent_name,
                                       attributes):
        """ Create an ImmObject instance for an object that has not yet existed
        in IMM

        Args:
            class_name (str): Class name
            parent_name (str): Parent name
            attributes (dict): Dictionary of class attributes

        Returns:
            ImmObject: Imm object
        """
        rdn_attribute = self.imm_om.get_rdn_attribute_for_class(class_name)
        rdn_value = attributes[rdn_attribute][0]

        if parent_name:
            dn = '%s,%s' % (rdn_value, parent_name)
        else:
            dn = rdn_value

        obj = ImmObject(class_name=class_name, dn=dn)

        for name, values in attributes.items():
            obj.__setattr__(name, values)

        obj.__setattr__('SaImmAttrClassName', class_name)
        obj.__setattr__('dn', dn)

        return obj
