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
""" IMM Search iterators """
from __future__ import print_function
from collections import Iterator
from ctypes import pointer, cast, c_void_p

from pyosaf.saAis import eSaAisErrorT, SaNameT, unmarshalNullArray, SaStringT
from pyosaf.saImm import saImm, eSaImmScopeT, eSaImmValueTypeT, \
    unmarshalSaImmValue, SaImmAttrNameT, SaImmSearchParametersT_2, \
    SaImmAttrValuesT_2
from pyosaf import saImmOm
from pyosaf.utils import log_err, bad_handle_retry
from pyosaf.utils.immom import agent
from pyosaf.utils.immom.object import ImmObject


class SearchIterator(agent.OmAgentManager, Iterator):
    """ General search iterator """
    def __init__(self, root_name=None, scope=eSaImmScopeT.SA_IMM_SUBTREE,
                 attribute_names=None, search_param=None, version=None):
        """ Constructor for SearchIterator class """
        super(SearchIterator, self).__init__(version)
        self.search_handle = None
        if root_name is not None:
            self.root_name = SaNameT(root_name)
        else:
            self.root_name = None
        self.scope = scope
        self.attribute_names = [SaImmAttrNameT(attr_name)
                                for attr_name in attribute_names] \
            if attribute_names else None

        self.search_options = saImm.SA_IMM_SEARCH_ONE_ATTR
        if attribute_names is None:
            self.search_options |= saImm.SA_IMM_SEARCH_GET_ALL_ATTR
        else:
            self.search_options |= saImm.SA_IMM_SEARCH_GET_SOME_ATTR

        if search_param is None:
            self.search_param = SaImmSearchParametersT_2()
        else:
            self.search_param = search_param

    def __enter__(self):
        """ Enter method for SearchIterator class """
        return self

    def __exit__(self, exit_type, value, traceback):
        """ Called when Iterator is used in a 'with' statement
            just before it exits

        exit_type, value and traceback are only set if the with statement was
        left via an exception
        """
        if self.search_handle is not None:
            agent.saImmOmSearchFinalize(self.search_handle)
            self.search_handle = None
        if self.handle is not None:
            agent.saImmOmFinalize(self.handle)
            self.handle = None

    def __del__(self):
        """ Destructor for SearchIterator class """
        if self.search_handle is not None:
            saImmOm.saImmOmSearchFinalize(self.search_handle)
            self.search_handle = None
        if self.handle is not None:
            saImmOm.saImmOmFinalize(self.handle)
            self.handle = None

    def __iter__(self):
        return self

    def next(self):
        """ Override next() method of Iterator class """
        return self.__next__()

    def __next__(self):
        obj_name = SaNameT()
        attributes = pointer(pointer(SaImmAttrValuesT_2()))
        rc = agent.saImmOmSearchNext_2(self.search_handle, obj_name,
                                       attributes)
        if rc != eSaAisErrorT.SA_AIS_OK:
            if rc != eSaAisErrorT.SA_AIS_ERR_NOT_EXIST:
                log_err("saImmOmSearchNext_2 FAILED - %s" %
                        eSaAisErrorT.whatis(rc))
            raise StopIteration
        attrs = {}
        attr_list = unmarshalNullArray(attributes)
        for attr in attr_list:
            attr_range = list(range(attr.attrValuesNumber))
            attrs[str(attr.attrName)] = [
                attr.attrValueType,
                [unmarshalSaImmValue(attr.attrValues[val],
                                     attr.attrValueType)
                 for val in attr_range]]
        return ImmObject(str(obj_name), attrs)

    @bad_handle_retry
    def init(self):
        """ Initialize the IMM OM search iterator

        Returns:
            SaAisErrorT: Return code of the IMM OM APIs
        """
        # Cleanup previous resources if any
        self.finalize()
        rc = self.initialize()

        self.search_handle = saImmOm.SaImmSearchHandleT()
        if rc == eSaAisErrorT.SA_AIS_OK:
            rc = agent.saImmOmSearchInitialize_2(
                self.handle, self.root_name, self.scope, self.search_options,
                self.search_param, self.attribute_names, self.search_handle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saImmOmSearchInitialize_2 FAILED - %s" %
                        eSaAisErrorT.whatis(rc))
                self.search_handle = None
        return rc


class InstanceIterator(SearchIterator):
    """ Iterator over instances of a class """
    def __init__(self, class_name, root_name=None):
        name = SaStringT(class_name)
        search_param = SaImmSearchParametersT_2()
        search_param.searchOneAttr.attrName = SaStringT("SaImmAttrClassName")
        search_param.searchOneAttr.attrValueType = \
            eSaImmValueTypeT.SA_IMM_ATTR_SASTRINGT
        search_param.searchOneAttr.attrValue = cast(pointer(name), c_void_p)
        SearchIterator.__init__(self, root_name, search_param=search_param)
