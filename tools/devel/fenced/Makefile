#      -*- OpenSAF  -*-
#
# Copyright Ericsson AB 2019 - All Rights Reserved.
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
SYSTEMD_DIR = /lib/systemd/system
OSAF_DIR = /etc/opensaf
INST_DIR = /usr/local/lib/opensaf
CONFIG_FILE=fenced.conf
SERVICE_FILE = osaffenced.service

PGM = osaffenced
OBJS = command.o fenced_main.o node_state_file.o node_state_hdlr.o node_state_hdlr_factory.o node_state_hdlr_pl.o node_state_hdlr_sc.o service.o timer.o watchdog.o

INCLUDES = -I.

#---------------------------------------------------------
# Compiler & linker flags
#---------------------------------------------------------
INCLUDES = -I.
CXXFLAGS = -g -Wall -Werror -std=c++11 $(INCLUDES)
LDFLAGS = -lsystemd -lpthread

#---------------------------------------------------------
# Explicit targets
#---------------------------------------------------------
all: $(PGM)

$(PGM): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

$(OBJECTS): %.o:
	$(CXX) -c $< -o $@

.PHONY: install
install: osaffenced
	cp $< $(INST_DIR)
	cp $(CONFIG_FILE) $(OSAF_DIR)
	cp $(SERVICE_FILE) $(SYSTEMD_DIR)
	systemctl enable $(SERVICE_FILE)
	systemctl start $(SERVICE_FILE)

.PHONY: uninstall
uninstall: osaffenced
	systemctl stop $(SERVICE_FILE)
	systemctl disable $(SERVICE_FILE)
	rm $(INST_DIR)/$<
	rm $(OSAF_DIR)/$(CONFIG_FILE)
	rm $(SYSTEMD_DIR)/$(SERVICE_FILE)

.PHONY: clean
clean:
	-rm -f *.o *~ $(PGM)
