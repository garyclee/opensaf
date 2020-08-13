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
from ctypes import pointer

from pyosaf.saAis import SaNameT, eSaAisErrorT, unmarshalNullArray
from pyosaf.saImm import unmarshalSaImmValue, SaImmAttrNameT, \
    SaImmAttrValuesT_2
from pyosaf import saImmOm

from pyosaf.utils import bad_handle_retry, log_err
from pyosaf.utils.immom import agent
from pyosaf.utils.immom.object import ImmObject


class ImmOmAccessor(agent.OmAgentManager):
    """ This class provides functions of the ImmOmAccessor interface """
    def __init__(self, version=None):
        """ Constructor for ImmOmAccessor class

        Args:
            version (SaVersionT): IMM OM version
        """
        super(ImmOmAccessor, self).__init__(version)
        self.accessor_handle = None

    def __enter__(self):
        """ Enter method for ImmOmAccessor class """
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        """ Exit method for ImmOmAccessor class

        Finalize the accessor handle and the IMM OM agent handle
        """
        if self.accessor_handle is not None:
            agent.saImmOmAccessorFinalize(self.accessor_handle)
            self.accessor_handle = None
        if self.handle is not None:
            agent.saImmOmFinalize(self.handle)
            self.handle = None

    def __del__(self):
        """ Destructor for ImmOmAccessor class

        Finalize the accessor handle and the IMM OM agent handle
        """
        if self.accessor_handle is not None:
            saImmOm.saImmOmAccessorFinalize(self.accessor_handle)
            self.accessor_handle = None
        if self.handle is not None:
            saImmOm.saImmOmFinalize(self.handle)
            self.handle = None

    @bad_handle_retry
    def init(self):
        """ Initialize the accessor handle

        Returns:
            SaAisErrorT: Return code of the saImmOmAccessorInitialize() call
        """
        self.finalize()
        rc = self.initialize()
        if rc == eSaAisErrorT.SA_AIS_OK:
            self.accessor_handle = saImmOm.SaImmAccessorHandleT()
            # Initialize ImmOmAccessor Handle
            rc = agent.saImmOmAccessorInitialize(self.handle,
                                                 self.accessor_handle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmAccessorInitialize FAILED - %s" %
                        eSaAisErrorT.whatis(rc))
        return rc

    @bad_handle_retry
    def get(self, object_name, attr_name_list=None, class_name=None):
        """ Obtain values of some attributes of the given object

        Args:
            object_name (str): Object name
            attr_name_list (list): List of attributes
            class_name (str): Class name

        Returns:
            SaAisErrorT: Return code of the corresponding IMM API call(s)
            ImmObject: Imm object
        """
        imm_obj = None
        # Always request the SaImmAttrClassName attribute if needed
        if attr_name_list and not class_name \
                and 'SaImmAttrClassName' not in attr_name_list \
                and not attr_name_list == \
                    ['SA_IMM_SEARCH_GET_CONFIG_ATTR']:
            attr_name_list.append('SaImmAttrClassName')

        attr_names = [SaImmAttrNameT(attr) for attr in attr_name_list] \
            if attr_name_list else None

        attributes = pointer(pointer(SaImmAttrValuesT_2()))

        rc = agent.saImmOmAccessorGet_2(self.accessor_handle,
                                        SaNameT(object_name),
                                        attr_names, attributes)

        if rc == eSaAisErrorT.SA_AIS_OK:
            attrs = {}
            attr_list = unmarshalNullArray(attributes)
            for attr in attr_list:
                attr_range = list(range(attr.attrValuesNumber))
                attrs[str(attr.attrName)] = [
                    attr.attrValueType,
                    [unmarshalSaImmValue(attr.attrValues[val],
                                         attr.attrValueType)
                     for val in attr_range]]
            if 'SaImmAttrClassName' not in attrs and class_name:
                attrs['SaImmAttrClassName'] = class_name
            imm_obj = ImmObject(dn=object_name, attributes=attrs)

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the users, so that they would re-try the
            # failed operation. Otherwise, the true error code is returned
            # to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmAccessorGet_2 FAILED - %s" %
                        eSaAisErrorT.whatis(rc))
                rc = init_rc

        return rc, imm_obj
