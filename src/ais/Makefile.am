#      -*- OpenSAF  -*-
#
# Copyright Ericsson AB 2017 - All Rights Reserved.
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

CORE_INCLUDES += -I$(top_srcdir)/src/ais/include

EXTRA_DIST += \
	src/ais/lib/libSaAmf.map \
	src/ais/lib/libSaCkpt.map \
	src/ais/lib/libSaClm.map \
	src/ais/lib/libSaEvt.map \
	src/ais/lib/libSaImmOi.map \
	src/ais/lib/libSaImmOm.map \
	src/ais/lib/libSaLck.map \
	src/ais/lib/libSaLog.map \
	src/ais/lib/libSaMsg.map \
	src/ais/lib/libSaNtf.map \
	src/ais/lib/libSaPlm.map \
	src/ais/lib/libSaSmf.map

include_HEADERS += \
	src/ais/include/saAis.h \
	src/ais/include/saAis_B_5_14.h \
	src/ais/include/saAmf.h \
	src/ais/include/saAmf_B_04_02.h \
	src/ais/include/saCkpt.h \
	src/ais/include/saCkpt_B_02_03.h \
	src/ais/include/saClm.h \
	src/ais/include/saEvt.h \
	src/ais/include/saImm.h \
	src/ais/include/saImmOi.h \
	src/ais/include/saImmOi_A_2_11.h \
	src/ais/include/saImmOi_A_2_15.h \
	src/ais/include/saImmOm.h \
	src/ais/include/saImmOm_A_2_11.h \
	src/ais/include/saImmOm_A_2_12.h \
	src/ais/include/saImmOm_A_2_13.h \
	src/ais/include/saImmOm_A_2_14.h \
	src/ais/include/saImmOm_A_2_15.h \
	src/ais/include/saImmOm_A_2_16.h \
	src/ais/include/saImmOm_A_2_17.h \
	src/ais/include/saLck.h \
	src/ais/include/saLog.h \
	src/ais/include/saMsg.h \
	src/ais/include/saNtf.h \
	src/ais/include/saPlm.h \
	src/ais/include/saSmf.h

pkgconfig_DATA += \
	src/ais/lib/opensaf-amf.pc \
	src/ais/lib/opensaf-ckpt.pc \
	src/ais/lib/opensaf-clm.pc \
	src/ais/lib/opensaf-evt.pc \
	src/ais/lib/opensaf-imm.pc \
	src/ais/lib/opensaf-lck.pc \
	src/ais/lib/opensaf-log.pc \
	src/ais/lib/opensaf-msg.pc \
	src/ais/lib/opensaf-ntf.pc \
	src/ais/lib/opensaf-plm.pc \
	src/ais/lib/opensaf-smf.pc

pkginclude_HEADERS += \
	src/ais/include/opensaf/cpsv_papi.h

noinst_HEADERS += \
	src/ais/include/saTmr.h

noinst_LTLIBRARIES += lib/libais.la

lib_libais_la_CFLAGS = $(OSAF_LIB_FLAGS) $(AM_CFLAGS)

lib_libais_la_CXXFLAGS = $(OSAF_LIB_FLAGS) $(AM_CXXFLAGS)

lib_libais_la_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

lib_libais_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-static

lib_libais_la_SOURCES = \
	src/ais/aisa_api.c

nodist_EXTRA_lib_libais_la_SOURCES = dummy.cc

noinst_HEADERS += \
	src/ais/try_again_decorator.h

TESTS += bin/testlib

bin_testlib_CXXFLAGS = \
	$(AM_CXXFLAGS)

bin_testlib_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(GTEST_DIR)/include \
	-I$(GMOCK_DIR)/include

bin_testlib_LDFLAGS = \
	$(AM_LDFLAGS)

bin_testlib_SOURCES = \
	src/ais/tests/try_again_decorator_test.cc

bin_testlib_LDADD = \
	$(GTEST_DIR)/lib/libgtest.la \
	$(GTEST_DIR)/lib/libgtest_main.la \
	$(GMOCK_DIR)/lib/libgmock.la \
	$(GMOCK_DIR)/lib/libgmock_main.la \
	lib/libopensaf_core.la
