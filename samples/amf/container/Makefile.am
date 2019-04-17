#      -*- OpenSAF  -*-
#
# (C) Copyright 2010 The OpenSAF Foundation
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
# Author(s): Wind River Systems
#

include $(top_srcdir)/Makefile.common

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = \
	amf_container_demo_script \
	AppConfig-container.xml \
	AppConfig-contained-2N.xml \
	README

bin_PROGRAMS = amf_container_demo

amf_container_demo_CPPFLAGS = \
	-DSA_EXTENDED_NAME_SOURCE \
	$(AM_CPPFLAGS)

amf_container_demo_SOURCES = \
	amf_container_demo.c

amf_container_demo_LDADD = \
	@SAF_AIS_AMF_LIBS@

install-data-hook:
	$(mkinstalldirs) $(DESTDIR)/opt/amf_demo
	cp amf_container_demo $(DESTDIR)/opt/amf_demo
	cp amf_container_script $(DESTDIR)/opt/amf_demo

uninstall-hook:
	rm -rf $(DESTDIR)/opt/amf_demo
