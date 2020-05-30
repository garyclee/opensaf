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
	src/osaf/consensus/consensus.h

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
	src/osaf/consensus/consensus.cc

nodist_EXTRA_lib_libosaf_common_la_SOURCES = dummy.cc

lib_libosaf_common_la_LIBADD = \
	lib/libSaLog.la \
	lib/libSaImmOi.la \
	lib/libSaImmOm.la

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
