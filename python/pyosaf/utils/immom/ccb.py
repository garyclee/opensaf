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
""" Class representing an IMM CCB """
from __future__ import print_function
from ctypes import c_void_p, pointer, cast, POINTER

from pyosaf.saAis import eSaAisErrorT, SaNameT, SaStringT, SaFloatT, \
    unmarshalNullArray, SaDoubleT, SaTimeT, SaUint64T, SaInt64T, SaUint32T, \
    SaInt32T
from pyosaf.saImm import eSaImmScopeT, eSaImmValueTypeT, SaImmAttrValuesT_2, \
    SaImmAttrNameT, SaImmClassNameT
from pyosaf import saImm
from pyosaf import saImmOm
from pyosaf.utils.immom import agent
from pyosaf.utils.immom.agent import ImmOmAgent
from pyosaf.utils.immom.accessor import ImmOmAccessor
from pyosaf.utils import log_err, bad_handle_retry


def _value_to_ctype_ptr(value_type, value):
    """ Convert a value to a ctypes value ptr

    Args:
        value_type (eSaImmValueTypeT): Value type
        value (ptr): Value in pointer

    Returns:
        c_void_p: ctype pointer which points to value
    """
    if value_type is eSaImmValueTypeT.SA_IMM_ATTR_SAINT32T:
        ctypeptr = cast(pointer(SaInt32T(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SAUINT32T:
        ctypeptr = cast(pointer(SaUint32T(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SAINT64T:
        ctypeptr = cast(pointer(SaInt64T(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SAUINT64T:
        ctypeptr = cast(pointer(SaUint64T(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SATIMET:
        ctypeptr = cast(pointer(SaTimeT(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SANAMET:
        ctypeptr = cast(pointer(SaNameT(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SAFLOATT:
        ctypeptr = cast(pointer(SaFloatT(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SADOUBLET:
        ctypeptr = cast(pointer(SaDoubleT(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SASTRINGT:
        ctypeptr = cast(pointer(SaStringT(value)), c_void_p)
    elif value_type is eSaImmValueTypeT.SA_IMM_ATTR_SAANYT:
        assert 0
    else:
        assert 0

    return ctypeptr


def marshal_c_array(value_type, value_list):
    """ Create a C array initialized with values from value_list

    Args:
        value_type (eSaImmValueTypeT): Value type
        value_list (list): List of values

    Returns:
        c_void_p: ctype pointer which points to array
    """
    c_array_type = saImm.SaImmAttrValueT * len(value_list)
    c_array = c_array_type()
    for i, value in enumerate(value_list):
        c_array[i] = _value_to_ctype_ptr(value_type, value)
    return c_array


class Ccb(ImmOmAgent):
    """ Class representing an ongoing CCB """
    def __init__(self, flags=saImm.saImm.SA_IMM_CCB_REGISTERED_OI,
                 version=None):
        super(Ccb, self).__init__(version=version)
        self.admin_owner = None
        self.accessor = None
        self.ccb_handle = None
        self.ccb_flags = saImmOm.SaImmCcbFlagsT(flags) if flags else \
            saImmOm.SaImmCcbFlagsT(0)

    def __enter__(self):
        """ Called when Ccb is used in a 'with' statement as follows:
            with Ccb() as ccb:

        The call is invoked before any code within is run.
        """
        return self

    def __exit__(self, exit_type, value, traceback):
        """ Called when Ccb is used in a 'with' statement, just before it exits

        exit_type, value and traceback are only set if the with statement was
        left via an exception
        """
        if exit_type or value or traceback:
            self.__del__()
        else:
            self.apply()
            self.__del__()

    def __del__(self):
        """ Destructor for CBB class

        Finalize the the CCB
        """
        if self.handle is not None:
            saImmOm.saImmOmFinalize(self.handle)
            self.handle = None
            self.admin_owner = None
            self.ccb_handle = None

    @bad_handle_retry
    def init(self, owner_name=""):
        """ Initialize the IMM OM interface needed for CCB operations

        Args:
            owner_name (str): Name of the admin owner

        Return:
            SaAisErrorT: Return code of the corresponding IMM API calls
        """
        rc = super(Ccb, self).init()
        if rc == eSaAisErrorT.SA_AIS_OK:
            self.admin_owner = agent.OmAdminOwner(self.handle, owner_name)
            rc = self.admin_owner.init()

            if rc == eSaAisErrorT.SA_AIS_OK:
                _owner_handle = self.admin_owner.get_handle()
                self.ccb_handle = saImmOm.SaImmCcbHandleT()

                rc = agent.saImmOmCcbInitialize(_owner_handle, self.ccb_flags,
                                                self.ccb_handle)
                if rc != eSaAisErrorT.SA_AIS_OK:
                    log_err("saImmOmCcbInitialize FAILED - %s" %
                            eSaAisErrorT.whatis(rc))
        return rc

    def create(self, obj, parent_name=None):
        """ Create the CCB object

        Args:
            obj (ImmObject): Imm object
            parent_name (str): Parent name

        Return:
            SaAisErrorT: Return code of the corresponding IMM API calls
        """
        rc = eSaAisErrorT.SA_AIS_OK
        if parent_name is not None:
            rc = self.admin_owner.set_owner(parent_name)
            parent_name = SaNameT(parent_name)

        if rc == eSaAisErrorT.SA_AIS_OK:
            attr_values = []
            for attr_name, type_values in obj.attrs.items():
                values = type_values[1]
                attr = SaImmAttrValuesT_2()
                attr.attrName = SaImmAttrNameT(attr_name)
                attr.attrValueType = type_values[0]

                attr.attrValuesNumber = len(values)
                attr.attrValues = marshal_c_array(attr.attrValueType, values)
                attr_values.append(attr)

            rc = agent.saImmOmCcbObjectCreate_2(self.ccb_handle,
                                                SaImmClassNameT(obj.class_name),
                                                parent_name,
                                                attr_values)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmCcbObjectCreate_2 FAILED - %s" %
                        eSaAisErrorT.whatis(rc))

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the users, so that they would re-try the
            # failed operation. Otherwise, the true error code is returned
            # to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    def delete(self, object_name):
        """ Add a delete operation of the object with the given DN to the CCB

        Args:
            object_name (str): Object name

        Return:
            SaAisErrorT: Return code of the corresponding IMM API calls
        """
        if object_name is None:
            return eSaAisErrorT.SA_AIS_ERR_NOT_EXIST

        rc = self.admin_owner.set_owner(object_name)
        if rc == eSaAisErrorT.SA_AIS_OK:
            obj_name = SaNameT(object_name)
            rc = agent.saImmOmCcbObjectDelete(self.ccb_handle, obj_name)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmCcbObjectDelete FAILED - %s" %
                        eSaAisErrorT.whatis(rc))

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the users, so that they would re-try the
            # failed operation. Otherwise, the true error code is returned
            # to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    def _modify(self, object_name, attr_name, values, mod_type):
        """ Modify an existing object

        Args:
            object_name (str): Object name
            attr_name (str): Attribute name
            values (list): List of attribute values
            mod_type (eSaImmAttrModificationTypeT): Modification type
        Return:
            SaAisErrorT: Return code of the corresponding IMM API call(s)
        """
        if object_name is None:
            rc = eSaAisErrorT.SA_AIS_ERR_INVALID_PARAM
        else:
            if not self.accessor:
                self.accessor = ImmOmAccessor(self.init_version)
                self.accessor.init()

            # Get the attribute value type by reading the object's class
            # description
            rc, obj = self.accessor.get(object_name)
            if rc == eSaAisErrorT.SA_AIS_OK:
                class_name = obj.SaImmAttrClassName
                _, attr_def_list = self.get_class_description(class_name)
                value_type = None
                for attr_def in attr_def_list:
                    if str(attr_def.attrName) == attr_name:
                        value_type = attr_def.attrValueType
                        break
                if value_type:
                    rc = self.admin_owner.set_owner(object_name,
                                                    eSaImmScopeT.SA_IMM_ONE)

            if rc == eSaAisErrorT.SA_AIS_OK:
                # Make sure the values field is a list
                if not isinstance(values, list):
                    values = [values]

                attr_mods = []
                attr_mod = saImmOm.SaImmAttrModificationT_2()
                attr_mod.modType = mod_type
                attr_mod.modAttr = SaImmAttrValuesT_2()
                attr_mod.modAttr.attrName = SaImmAttrNameT(attr_name)
                attr_mod.modAttr.attrValueType = value_type
                attr_mod.modAttr.attrValuesNumber = len(values)
                attr_mod.modAttr.attrValues = marshal_c_array(value_type,
                                                              values)
                attr_mods.append(attr_mod)
                object_name = SaNameT(object_name)
                rc = agent.saImmOmCcbObjectModify_2(self.ccb_handle,
                                                    object_name, attr_mods)
                if rc != eSaAisErrorT.SA_AIS_OK:
                    log_err("saImmOmCcbObjectModify_2 FAILED - %s" %
                            eSaAisErrorT.whatis(rc))

            if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                init_rc = self.init()
                # If the re-initialization of agent handle succeeds, we still
                # need to return BAD_HANDLE to the users, so that they would
                # re-try the failed operation. Otherwise, the true error code
                # is returned to the user to decide further actions.
                if init_rc != eSaAisErrorT.SA_AIS_OK:
                    rc = init_rc

        return rc

    def modify_value_add(self, object_name, attr_name, values):
        """ Add to the CCB an ADD modification of an existing object

        Args:
            object_name (str): Object name
            attr_name (str): Attribute name
            values (list): List of attribute values

        Return:
            SaAisErrorT: Return code of the corresponding IMM API calls
        """
        mod_type = \
            saImm.eSaImmAttrModificationTypeT.SA_IMM_ATTR_VALUES_ADD
        return self._modify(object_name, attr_name, values, mod_type)

    def modify_value_replace(self, object_name, attr_name, values):
        """ Add to the CCB an REPLACE modification of an existing object

        Args:
            object_name (str): Object name
            attr_name (str): Attribute name
            values (list): List of attribute values

        Return:
            SaAisErrorT: Return code of the corresponding IMM API calls
        """
        mod_type = \
            saImm.eSaImmAttrModificationTypeT.SA_IMM_ATTR_VALUES_REPLACE
        return self._modify(object_name, attr_name, values, mod_type)

    def modify_value_delete(self, object_name, attr_name, values):
        """ Add to the CCB an DELETE modification of an existing object

        Args:
            object_name (str): Object name
            attr_name (str): Attribute name
            values (list): List of attribute values

        Return:
            SaAisErrorT: Return code of the corresponding IMM API calls
        """
        mod_type = \
            saImm.eSaImmAttrModificationTypeT.SA_IMM_ATTR_VALUES_DELETE
        return self._modify(object_name, attr_name, values, mod_type)

    def apply(self):
        """ Apply the CCB

        Return:
            SaAisErrorT: Return code of the APIs
        """
        rc = agent.saImmOmCcbApply(self.ccb_handle)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saImmOmCcbApply FAILED - %s" % eSaAisErrorT.whatis(rc))

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still
            # need to return BAD_HANDLE to the users, so that they would
            # re-try the failed operation. Otherwise, the true error code
            # is returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    def get_error_strings(self):
        """ Return the current CCB error strings

        Returns:
            SaAisErrorT: Return code of the saImmOmCcbGetErrorStrings() call
            list: List of error strings
        """
        c_strings = POINTER(SaStringT)()

        rc = agent.saImmOmCcbGetErrorStrings(self.ccb_handle, c_strings)

        return rc, unmarshalNullArray(c_strings)
