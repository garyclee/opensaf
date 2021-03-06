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

if ENABLE_AIS_EVT

osaf_lib_LTLIBRARIES += lib/libevt_common.la

lib_libevt_common_la_CFLAGS = $(OSAF_LIB_FLAGS) $(AM_CFLAGS)

lib_libevt_common_la_CXXFLAGS = $(OSAF_LIB_FLAGS) $(AM_CXXFLAGS)

lib_libevt_common_la_CPPFLAGS = \
	$(AM_CPPFLAGS)

lib_libevt_common_la_LDFLAGS = \
	$(AM_LDFLAGS)

lib_libevt_common_la_SOURCES = \
	src/evt/common/edsv_util.c

nodist_EXTRA_lib_libevt_common_la_SOURCES = dummy.cc

lib_LTLIBRARIES += lib/libSaEvt.la

lib_libSaEvt_la_SOURCES = \
	src/evt/agent/eda_util.c \
	src/evt/agent/eda_saf_api.c \
	src/evt/agent/eda_hdl.c \
	src/evt/agent/eda_mds.c \
	src/evt/agent/eda_init.c

nodist_EXTRA_lib_libSaEvt_la_SOURCES = dummy.cc

lib_libSaEvt_la_CFLAGS = $(OSAF_LIB_FLAGS) $(AM_CFLAGS)

lib_libSaEvt_la_CXXFLAGS = $(OSAF_LIB_FLAGS) $(AM_CXXFLAGS)

lib_libSaEvt_la_CPPFLAGS = \
	-DNCS_EDA=1 \
	$(AM_CPPFLAGS)

lib_libSaEvt_la_LDFLAGS = \
	$(AM_LDFLAGS)

if HAVE_LD_VERSION_SCRIPT
lib_libSaEvt_la_LDFLAGS += \
	-Wl,-version-script=$(top_srcdir)/src/ais/lib/libSaEvt.map
endif

lib_libSaEvt_la_LDFLAGS += \
	-version-number 1:0:3

lib_libSaEvt_la_LIBADD = \
	lib/libevt_common.la \
	lib/libais.la \
	lib/libopensaf_core.la

noinst_HEADERS += \
	src/evt/agent/eda.h \
	src/evt/agent/eda_cb.h \
	src/evt/agent/eda_dl_api.h \
	src/evt/agent/eda_hdl.h \
	src/evt/agent/eda_mds.h \
	src/evt/agent/eda_mem.h \
	src/evt/common/edsv_defs.h \
	src/evt/common/edsv_mem.h \
	src/evt/common/edsv_msg.h \
	src/evt/common/edsv_util.h \
	src/evt/evtd/eds.h \
	src/evt/evtd/eds_amf.h \
	src/evt/evtd/eds_cb.h \
	src/evt/evtd/eds_ckpt.h \
	src/evt/evtd/eds_dl_api.h \
	src/evt/evtd/eds_evt.h \
	src/evt/evtd/eds_mds.h \
	src/evt/evtd/eds_mem.h

osaf_execbin_PROGRAMS += bin/osafevtd

nodist_pkgclccli_SCRIPTS += \
	src/evt/evtd/osaf-evtd

if ENABLE_IMMXML

dist_pkgimmxml_svc_DATA += \
	src/evt/config/edsv_classes.xml \
	src/evt/config/edsv_objects.xml \
	src/evt/config/edsv_sc_template.xml

endif

dist_pkgsysconf_DATA += \
	src/evt/evtd/evtd.conf

bin_osafevtd_CXXFLAGS = $(AM_CXXFLAGS)

bin_osafevtd_CPPFLAGS = \
	-DSA_CLM_B01=1 \
	-DNCS_EDS=1 \
	$(AM_CPPFLAGS)

bin_osafevtd_SOURCES = \
	src/evt/evtd/eds_amf.c \
	src/evt/evtd/eds_api.c \
	src/evt/evtd/eds_cb.c \
	src/evt/evtd/eds_ckpt.c \
	src/evt/evtd/eds_debug.c \
	src/evt/evtd/eds_evt.c \
	src/evt/evtd/eds_imm.c \
	src/evt/evtd/eds_ll.c \
	src/evt/evtd/eds_main.c \
	src/evt/evtd/eds_mds.c \
	src/evt/evtd/eds_tmr.c \
	src/evt/evtd/eds_util.c

bin_osafevtd_LDADD = \
	lib/libevt_common.la \
	lib/libosaf_common.la \
	lib/libSaAmf.la \
	lib/libSaClm.la \
	lib/libSaImmOi.la \
	lib/libSaImmOm.la \
	lib/libopensaf_core.la

if ENABLE_TESTS

bin_evttest_CPPFLAGS = \
	$(AM_CPPFLAGS)

bin_PROGRAMS += bin/evttest

noinst_HEADERS += \
	src/evt/apitest/tet_eda.h

bin_evttest_SOURCES = \
  src/evt/apitest/tet_edsv_wrappers.c \
  src/evt/apitest/tet_eda.c \
  src/evt/apitest/tet_edsv_func.c \
  src/evt/apitest/tet_edsv_util.c \
  src/evt/apitest/evttest.c

bin_evttest_LDADD = \
  lib/libSaEvt.la \
  lib/libopensaf_core.la \
  lib/libapitest.la

endif

endif
