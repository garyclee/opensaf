#      -*- OpenSAF  -*-
#
# (C) Copyright 2016 The OpenSAF Foundation
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
# Author(s): Ericsson AB
#

EXTRA_DIST += \
	src/osaf/consensus/plugins/etcd.plugin \
	src/osaf/consensus/plugins/etcd3.plugin \
	src/osaf/consensus/plugins/sample.plugin \
	src/osaf/consensus/plugins/README.etcd \
	src/osaf/consensus/plugins/tcp/generate_cert.sh \
	src/osaf/consensus/plugins/tcp/README \
	src/osaf/consensus/plugins/tcp/tcp.plugin \
	src/osaf/consensus/plugins/tcp/tcp_server.py

noinst_HEADERS += \
	src/osaf/immutil/immutil.h \
	src/osaf/saflog/saflog.h \
	src/osaf/consensus/key_value.h \
	src/osaf/consensus/consensus.h \
	src/osaf/consensus/consensus_env.h

# immtools headers
# IMM configuration modifier
noinst_HEADERS += \
	src/osaf/immtools/imm_modify_config/attribute.h \
	src/osaf/immtools/imm_modify_config/add_operation_to_ccb.h \
	src/osaf/immtools/imm_modify_config/immccb.h

# IMM OM C++ APIs
noinst_HEADERS += \
	src/osaf/immtools/imm_om_ccapi/common/common.h \
	src/osaf/immtools/imm_om_ccapi/common/imm_attribute.h \
	src/osaf/immtools/imm_om_ccapi/om_admin_owner_clear.h \
	src/osaf/immtools/imm_om_ccapi/om_admin_owner_handle.h \
	src/osaf/immtools/imm_om_ccapi/om_admin_owner_set.h \
	src/osaf/immtools/imm_om_ccapi/om_ccb_handle.h \
	src/osaf/immtools/imm_om_ccapi/om_ccb_object_create.h \
	src/osaf/immtools/imm_om_ccapi/om_ccb_object_delete.h \
	src/osaf/immtools/imm_om_ccapi/om_ccb_object_modify.h \
	src/osaf/immtools/imm_om_ccapi/om_handle.h

pkglib_LTLIBRARIES += lib/libosaf_common.la

lib_libosaf_common_la_CFLAGS = $(OSAF_LIB_FLAGS) $(AM_CFLAGS)

lib_libosaf_common_la_CXXFLAGS = $(OSAF_LIB_FLAGS) $(AM_CXXFLAGS)

lib_libosaf_common_la_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

lib_libosaf_common_la_LDFLAGS = \
	$(AM_LDFLAGS)

lib_libosaf_common_la_SOURCES = \
	src/osaf/immutil/immutil.c \
	src/osaf/saflog/saflog.c \
	src/osaf/consensus/key_value.cc \
	src/osaf/consensus/consensus.cc \
	src/osaf/consensus/consensus_env.cc

# immtools code
# IMM configuration modifier
lib_libosaf_common_la_SOURCES += \
	src/osaf/immtools/imm_modify_config/attribute.cc \
	src/osaf/immtools/imm_modify_config/add_operation_to_ccb.cc \
	src/osaf/immtools/imm_modify_config/immccb.cc

# IMM OM C++ APIs
lib_libosaf_common_la_SOURCES += \
	src/osaf/immtools/imm_om_ccapi/common/common.cc \
	src/osaf/immtools/imm_om_ccapi/common/imm_attribute.cc \
	src/osaf/immtools/imm_om_ccapi/om_admin_owner_clear.cc \
	src/osaf/immtools/imm_om_ccapi/om_admin_owner_handle.cc \
	src/osaf/immtools/imm_om_ccapi/om_admin_owner_set.cc \
	src/osaf/immtools/imm_om_ccapi/om_ccb_handle.cc \
	src/osaf/immtools/imm_om_ccapi/om_ccb_object_create.cc \
	src/osaf/immtools/imm_om_ccapi/om_ccb_object_delete.cc \
	src/osaf/immtools/imm_om_ccapi/om_ccb_object_modify.cc \
	src/osaf/immtools/imm_om_ccapi/om_handle.cc

nodist_EXTRA_lib_libosaf_common_la_SOURCES = dummy.cc

lib_libosaf_common_la_LIBADD = \
	lib/libSaLog.la \
	lib/libSaImmOi.la \
	lib/libSaImmOm.la \
	lib/libopensaf_core.la

if ENABLE_IMMXML

dist_pkgimmxml_svc_DATA += \
	src/osaf/config/osaf_globalconfig_classes.xml \
	src/osaf/config/osaf_globalconfig_objects.xml

endif

if ENABLE_TESTS

noinst_HEADERS += \
	src/osaf/apitest/utest.h \
	src/osaf/apitest/util.h

noinst_LTLIBRARIES += lib/libapitest.la

lib_libapitest_la_CFLAGS = $(OSAF_LIB_FLAGS) $(AM_CFLAGS)

lib_libapitest_la_CXXFLAGS = $(OSAF_LIB_FLAGS) $(AM_CXXFLAGS)

lib_libapitest_la_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

lib_libapitest_la_SOURCES = \
	src/osaf/apitest/utest.c \
	src/osaf/apitest/util.c

nodist_EXTRA_lib_libapitest_la_SOURCES = dummy.cc

endif

# immtools test and demo. Built if configured with --enable-test
# These tests and demos has to be run manually. Not part of any other test package
# Should probably be changed to include in osaf test
if ENABLE_TESTS

bin_PROGRAMS += \
	bin/ccbdemo_create \
	bin/ccbdemo_delete \
	bin/ccbdemo_modify \
	bin/test_ccbhdl

# Installed if configured with --enable-immxml
if ENABLE_IMMXML

dist_pkgimmxml_svc_DATA += \
	src/osaf/immtools/imm_modify_demo/democlass.xml

endif

noinst_HEADERS += \
	src/osaf/immtools/imm_modify_demo/common.h

# DEMO Create
# -----------
bin_ccbdemo_create_CFLAGS = $(AM_CFLAGS) -Wformat=1

bin_ccbdemo_create_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

bin_ccbdemo_create_SOURCES = \
	src/osaf/immtools/imm_modify_demo/common.cc \
	src/osaf/immtools/imm_modify_demo/ccbdemo_create.cc

bin_ccbdemo_create_LDADD = \
	lib/libosaf_common.la \
	lib/libopensaf_core.la

# DEMO Delete
# -----------
bin_ccbdemo_delete_CFLAGS = $(AM_CFLAGS) -Wformat=1

bin_ccbdemo_delete_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

bin_ccbdemo_delete_SOURCES = \
	src/osaf/immtools/imm_modify_demo/common.cc \
	src/osaf/immtools/imm_modify_demo/ccbdemo_delete.cc

bin_ccbdemo_delete_LDADD = \
	lib/libosaf_common.la \
	lib/libopensaf_core.la

# DEMO Modify
# -----------
bin_ccbdemo_modify_CFLAGS = $(AM_CFLAGS) -Wformat=1

bin_ccbdemo_modify_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

bin_ccbdemo_modify_SOURCES = \
	src/osaf/immtools/imm_modify_demo/common.cc \
	src/osaf/immtools/imm_modify_demo/ccbdemo_modify.cc

bin_ccbdemo_modify_LDADD = \
	lib/libosaf_common.la \
	lib/libopensaf_core.la

# Test CCBHDL
# -----------
bin_test_ccbhdl_CFLAGS = $(AM_CFLAGS) -Wformat=1

bin_test_ccbhdl_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

bin_test_ccbhdl_SOURCES = \
	src/osaf/immtools/imm_modify_demo/common.cc \
	src/osaf/immtools/imm_modify_demo/test_ccbhdl.cc

bin_test_ccbhdl_LDADD = \
	lib/libosaf_common.la \
	lib/libopensaf_core.la

# ENABLE_TESTS
endif
