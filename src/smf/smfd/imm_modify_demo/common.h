/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2018 - All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.
 *
 * Author(s): Ericsson AB
 *
 */

// This file is a place to put functions etc. used in more than one program in
// this directory

#ifndef COMMON_H
#define COMMON_H

// Execute a shell command using system
bool ExecuteCommand(const char* command);

// Enable long DN handling in IMM by setting longDnsAllowed=1 in IMM
// configuration object
bool EnableImmLongDn(void);

// Installs the IMM class defined in democlass.xml
// Note: The democlass.xml file must be installed in
//       /usr/local/share/opensaf/immxml/services/democlass.xml
//       Is done automatically if build is setup with --enable-immxml
bool InstallDemoClass(void);


#endif /* COMMON_H */

