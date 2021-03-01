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
""" IMM Object representation """
from pyosaf.saAis import eSaAisErrorT
from pyosaf.saImm import saImm

from pyosaf.utils import SafException
from pyosaf.utils.immom.agent import ImmOmAgent


class ImmObject(object):
    """ Instance of this class represents an IMM object.
    Created by iterators and accessor reading from IMM or when creating objects
    in IMM.
    Cache class descriptions for the purpose to know if an attribute is
    multi-value and also its type.
    """
    class_desc = {}

    def __init__(self, dn=None, attributes=None, class_name=None):
        """ Constructor for IMM object
        Attributes is a map where attribute name is key, value is a tuple of
        (valueType, values), values is a list.
        class_name and attributes are mutually exclusive.

        Raises:
            SafException: If the return code of the corresponding API call(s)
                is not SA_AIS_OK
        """
        self.__dict__["dn"] = dn
        _imm_om = ImmOmAgent()
        rc = _imm_om.init()
        if rc != eSaAisErrorT.SA_AIS_OK:
            raise SafException(rc)
        # mutually exclusive for read vs create
        if class_name is not None:
            assert attributes is None
            self.__dict__["attrs"] = {}
            self.__dict__["class_name"] = class_name
            rc, class_desc = _imm_om.get_class_description(class_name)
            if rc != eSaAisErrorT.SA_AIS_OK:
                raise SafException(rc)
            self.class_desc[str(class_name)] = class_desc
        elif attributes is not None:
            assert class_name is None
            self.__dict__["attrs"] = attributes
            class_name = attributes["SaImmAttrClassName"][1][0]
            self.__dict__["class_name"] = class_name
            if class_name not in self.class_desc:
                rc, class_desc = _imm_om.get_class_description(class_name)
                if rc != eSaAisErrorT.SA_AIS_OK:
                    raise SafException(rc)
                self.class_desc[str(class_name)] = class_desc
        else:
            raise ValueError("Class and attributes are None")

        self.__dict__["rdn_attribute"] = \
            _imm_om.get_rdn_attribute_for_class(class_name)

    def get_value_type(self, attr_name):
        """ Return IMM value type of the named attribute

        Args:
            attr_name (str): Attribute name

        Returns:
            SaImmAttrNameT: Attribute value type
        """
        for attr_def in self.class_desc[str(self.class_name)]:
            if str(attr_def.attrName) == attr_name:
                return attr_def.attrValueType

    def __attr_is_multi_value(self, attr_name):
        """ Return True if attribute is multi-value (internal)

        Args:
            attr_name (str): Attribute name

        Returns:
            SaImmAttrFlagsT: Attribute flag
        """
        for attr_def in self.class_desc[str(self.class_name)]:
            if str(attr_def.attrName) == attr_name:
                return attr_def.attrFlags & saImm.SA_IMM_ATTR_MULTI_VALUE

    def __getattr__(self, name):
        if name not in self.attrs:
            raise KeyError("%s is not a valid attribute name" % name)

        if self.__attr_is_multi_value(name):
            return self.attrs[name][1]  # return a list of values
        else:
            if self.attrs[name][1]:
                return self.attrs[name][1][0]  # return a single value

        return None

    def __getitem__(self, item):
        return self.__getattr__(item)

    def __contains__(self, attr_name):
        if attr_name in self.attrs:
            if self.attrs[attr_name][1]:
                return True

        return False

    def __setattr__(self, key, value):
        # Correct RDN assignments missing the RDN attribute name in value

        if key == self.rdn_attribute:
            if str(value).find(self.rdn_attribute) != 0:
                value = '%s=%s' % (self.rdn_attribute, value)

        value_type = self.get_value_type(key)
        if isinstance(value, list):
            attr_value = (value_type, value)
        else:
            attr_value = (value_type, [value])

        self.attrs[key] = attr_value
