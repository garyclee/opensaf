/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2021 The OpenSAF Foundation
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

#include "base/apitest/basetest.h"
#include <unistd.h>

void usage(const char *progname)
{
	printf("Usage: %s [-h] [--help] [suite [testcase]]\n\n", progname);
	printf("OPTIONS:\n");
	printf("\t-h, --help    this help\n");
	printf("\tsuite         suite for testing. 0 for listing all tests\n");
	printf("\ttestcase      test case for testing in specific suite\n");
}

int main(int argc, char **argv)
{
	int suite = ALL_SUITES, tcase = ALL_TESTS;
	int rc = 0;
	int i;
	int index = 0;
	int failed = 0;
	char *endptr;

	srandom(getpid());

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			usage(basename(argv[0]));
			return 0;
		} else if (index == 0) {
			suite = strtol(argv[i], &endptr, 10);
			if (endptr && *endptr) {
				failed = 1;
			} else {
				index++;
			}
		} else if (index == 1) {
			tcase = strtol(argv[i], &endptr, 10);
			if (endptr && *endptr) {
				failed = 1;
			} else {
				index++;
			}
		} else {
			failed = 1;
		}

		if (failed) {
			fprintf(stderr,
				"Try '%s --help' for more information\n",
				argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (suite == 0) {
		test_list();
	} else {
		rc = test_run(suite, tcase);
	}

	return rc;
}
