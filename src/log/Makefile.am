#      -*- OpenSAF  -*-
#
# (C) Copyright 2016 The OpenSAF Foundation
# Copyright Ericsson AB 2016, 2017 - All Rights Reserved.
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
	src/log/README \
	src/log/README-HEADLESS \
	src/log/README_LONGDN \
	src/log/apitest/README \
	src/log/tools/README

lib_LTLIBRARIES += lib/libSaLog.la

lib_libSaLog_la_SOURCES = \
	src/log/agent/lga_client.cc \
	src/log/agent/lga_stream.cc \
	src/log/agent/lga_api.cc \
	src/log/agent/lga_util.cc \
	src/log/agent/lga_mds.cc \
	src/log/agent/lga_state.cc \
	src/log/agent/lga_agent.cc \
	src/log/agent/lga_ref_counter.cc


nodist_EXTRA_lib_libSaLog_la_SOURCES = dummy.cc

lib_libSaLog_la_CFLAGS = $(OSAF_LIB_FLAGS) $(AM_CFLAGS)

lib_libSaLog_la_CXXFLAGS = $(OSAF_LIB_FLAGS) $(AM_CXXFLAGS)

lib_libSaLog_la_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

lib_libSaLog_la_LDFLAGS = \
	$(AM_LDFLAGS)

if HAVE_LD_VERSION_SCRIPT
lib_libSaLog_la_LDFLAGS += \
	-Wl,-version-script=$(top_srcdir)/src/ais/lib/libSaLog.map
endif

lib_libSaLog_la_LDFLAGS += \
	-version-number 1:0:2

lib_libSaLog_la_LIBADD = \
	lib/libais.la \
	lib/libopensaf_core.la

noinst_HEADERS += \
	src/log/agent/lga_mds.h \
	src/log/agent/lga_common.h \
	src/log/agent/lga_util.h \
	src/log/agent/lga_client.h \
	src/log/agent/lga_agent.h \
	src/log/agent/lga_stream.h \
	src/log/agent/lga_state.h \
	src/log/agent/lga_ref_counter.h \
	src/log/common/lgsv_defs.h \
	src/log/common/lgsv_msg.h \
	src/log/logd/lgs.h \
	src/log/logd/lgs_cb.h \
	src/log/logd/lgs_clm.h \
	src/log/logd/lgs_config.h \
	src/log/logd/lgs_evt.h \
	src/log/logd/lgs_file.h \
	src/log/logd/lgs_filehdl.h \
	src/log/logd/lgs_fmt.h \
	src/log/logd/lgs_imm.h \
	src/log/logd/lgs_imm_gcfg.h \
	src/log/logd/lgs_mbcsv.h \
	src/log/logd/lgs_mbcsv_v1.h \
	src/log/logd/lgs_mbcsv_v2.h \
	src/log/logd/lgs_mbcsv_v3.h \
	src/log/logd/lgs_mbcsv_v5.h \
	src/log/logd/lgs_mbcsv_v6.h \
	src/log/logd/lgs_mbcsv_v9.h \
	src/log/logd/lgs_oi_admin.h \
	src/log/logd/lgs_recov.h \
	src/log/logd/lgs_stream.h \
	src/log/logd/lgs_util.h \
	src/log/logd/lgs_dest.h \
	src/log/logd/lgs_nildest.h \
	src/log/logd/lgs_unixsock_dest.h \
	src/log/logd/lgs_common.h \
	src/log/logd/lgs_amf.h \
	src/log/logd/lgs_cache.h \
	src/log/logd/lgs_mbcsv_cache.h


bin_PROGRAMS += bin/saflogger
osaf_execbin_PROGRAMS += bin/osaflogd
TESTS += bin/testlogd

nodist_pkgclccli_SCRIPTS += \
	src/log/logd/osaf-logd

dist_pkgsysconf_DATA += \
	src/log/logd/logd.conf

if ENABLE_IMMXML

dist_pkgimmxml_svc_DATA += \
	src/log/config/logsv_classes.xml \
	src/log/config/logsv_objects.xml \
	src/log/config/logsv_sc_template.xml

endif

bin_osaflogd_CXXFLAGS =$(AM_CXXFLAGS) -Wformat=1

bin_osaflogd_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

# Enable this flag to simulate the case that file system is unresponsive
# during write log record. Mainly for testing the following enhancement:
# log: improve the resilience of log service [#3116].
# When enabled, log handle thread will be suspended 17 seconds every 02 write
# requests and only take affect if the `logMaxPendingWriteRequests` is set
# to an non-zero value.
# bin_osaflogd_CPPFLAGS += -DSIMULATE_NFS_UNRESPONSE


bin_osaflogd_SOURCES = \
	src/log/logd/lgs_amf.cc \
	src/log/logd/lgs_clm.cc \
	src/log/logd/lgs_config.cc \
	src/log/logd/lgs_evt.cc \
	src/log/logd/lgs_file.cc \
	src/log/logd/lgs_filehdl.cc \
	src/log/logd/lgs_fmt.cc \
	src/log/logd/lgs_imm.cc \
	src/log/logd/lgs_imm_gcfg.cc \
	src/log/logd/lgs_main.cc \
	src/log/logd/lgs_mbcsv.cc \
	src/log/logd/lgs_mbcsv_v1.cc \
	src/log/logd/lgs_mbcsv_v2.cc \
	src/log/logd/lgs_mbcsv_v3.cc \
	src/log/logd/lgs_mbcsv_v5.cc \
	src/log/logd/lgs_mbcsv_v6.cc \
	src/log/logd/lgs_mbcsv_v9.cc \
	src/log/logd/lgs_mds.cc \
	src/log/logd/lgs_oi_admin.cc \
	src/log/logd/lgs_recov.cc \
	src/log/logd/lgs_stream.cc \
	src/log/logd/lgs_util.cc \
	src/log/logd/lgs_dest.cc \
	src/log/logd/lgs_nildest.cc \
	src/log/logd/lgs_unixsock_dest.cc \
	src/log/logd/lgs_cache.cc \
	src/log/logd/lgs_mbcsv_cache.cc

bin_osaflogd_LDADD = \
	lib/libosaf_common.la \
	lib/libSaAmf.la \
	lib/libSaImmOm.la \
	lib/libSaImmOi.la \
	lib/libSaClm.la \
	lib/libopensaf_core.la

bin_saflogger_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

bin_saflogger_SOURCES = \
	src/log/tools/saf_logger.c

bin_saflogger_LDADD = \
	lib/libSaLog.la \
	lib/libopensaf_core.la

if ENABLE_TESTS

bin_PROGRAMS += bin/logtest bin/saflogtest bin/logtestfr

noinst_HEADERS += \
	src/log/apitest/logtest.h \
	src/log/apitest/logutil.h \
	src/log/apitest/imm_tstutil.h \
	src/log/apitest/log_server.h

bin_logtest_CFLAGS = $(AM_CFLAGS) -Wformat=1

bin_logtest_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

# Enable this flag to add test cases for following enhancement:
# log: improve the resilience of log service [#3116].
# bin_logtest_CPPFLAGS += -DSIMULATE_NFS_UNRESPONSE

bin_logtest_SOURCES = \
	src/log/apitest/logtest.c \
	src/log/apitest/logutil.c \
	src/log/apitest/imm_tstutil.c \
	src/log/apitest/tet_saLogInitialize.c \
	src/log/apitest/tet_saLogSelectionObjectGet.c \
	src/log/apitest/tet_saLogDispatch.c \
	src/log/apitest/tet_saLogFinalize.c \
	src/log/apitest/tet_saLogStreamOpen_2.c \
	src/log/apitest/tet_saLogStreamOpenAsync_2.c \
	src/log/apitest/tet_saLogStreamOpenCallbackT.c \
	src/log/apitest/tet_saLogWriteLog.c \
	src/log/apitest/tet_saLogWriteLogAsync.c \
	src/log/apitest/tet_saLogWriteLogCallbackT.c \
	src/log/apitest/tet_saLogFilterSetCallbackT.c \
	src/log/apitest/tet_saLogStreamClose.c \
	src/log/apitest/tet_saLogLimitGet.c \
	src/log/apitest/tet_LogOiOps.c \
	src/log/apitest/tet_Log_misc.c \
	src/log/apitest/tet_Log_recov.c \
	src/log/apitest/tet_log_runtime_cfgobj.c \
	src/log/apitest/tet_log_longDN.c \
	src/log/apitest/tet_Log_clm.c \
	src/log/apitest/tet_cfg_destination.c \
	src/log/apitest/tet_multiple_thread.c \
	src/log/apitest/tet_saLogWriteLogAsync_cache.c \
	src/log/apitest/tet_saLogStreamConfigFacilityId.c \
	src/log/apitest/log_server.cc

bin_logtest_LDADD = \
	lib/libapitest.la \
	lib/libosaf_common.la \
	lib/libSaLog.la \
	lib/libSaImmOi.la \
	lib/libSaImmOm.la \
	lib/libopensaf_core.la

bin_saflogtest_SOURCES = \
	src/log/apitest/saflogtest.c

bin_saflogtest_LDADD = \
	lib/libapitest.la \
	lib/libosaf_common.la \
	lib/libSaLog.la \
	lib/libopensaf_core.la

bin_logtestfr_CPPFLAGS = \
	$(AM_CPPFLAGS)

bin_logtestfr_SOURCES = \
	src/log/apitest/logtestfr.c

bin_logtestfr_LDADD = \
	lib/libapitest.la \
	lib/libosaf_common.la \
	lib/libSaLog.la \
	lib/libSaImmOi.la \
	lib/libSaImmOm.la \
	lib/libopensaf_core.la

endif

bin_testlogd_CXXFLAGS =$(AM_CXXFLAGS)

bin_testlogd_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS) \
	-I$(GTEST_DIR)/include \
	-I$(GMOCK_DIR)/include

bin_testlogd_LDFLAGS = \
	$(AM_LDFLAGS)


bin_testlogd_SOURCES = \
	src/log/tests/lgs_dest_test.cc


bin_testlogd_LDADD = \
	lib/libopensaf_core.la \
	$(GTEST_DIR)/lib/libgtest.la \
	$(GTEST_DIR)/lib/libgtest_main.la \
	$(GMOCK_DIR)/lib/libgmock.la \
	$(GMOCK_DIR)/lib/libgmock_main.la \
	src/log/logd/bin_osaflogd-lgs_unixsock_dest.o \
	src/log/logd/bin_osaflogd-lgs_dest.o \
	src/log/logd/bin_osaflogd-lgs_nildest.o
