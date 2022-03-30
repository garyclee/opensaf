/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
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

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include "test_cpsv.h"
#include "osaf/apitest/utest.h"
#include "osaf/apitest/util.h"

extern void fill_testcase_data(void);

char *ckpt_valid_name;
char *ckpt_invalid_name;

int main(int argc, char **argv)
{
	int suite = ALL_SUITES, tcase = ALL_TESTS;
	int rc = 0;

	/* Initialize Test Data */
	fill_testcase_data();

	if (argc > 1) {
		suite = atoi(argv[1]);
	}

	if (argc > 2) {
		tcase = atoi(argv[2]);
	}

	if (suite == 0) {
		test_list();
		return 0;
	}

	rc = test_run(suite, tcase);

	free_testase_data();

	return rc;
}
