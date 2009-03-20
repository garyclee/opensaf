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
#include <unistd.h>
#include "tet_ntf.h"
#include "test.h"

SaVersionT ntfVersion = {'A', 0x02, 0x01};
SaAisErrorT rc;
SaNtfHandleT ntfHandle;
SaNtfCallbacksT ntfCallbacks = {NULL, NULL};
SaSelectionObjectT selectionObject;
int verbose = 0;

struct tet_testlist
{
    void (*testfunc)();
    int icref;
    int tpnum;
};

int main(int argc, char **argv)
{
    int suite = -1, tcase = -1;

    setenv("NTFSV_TRACE_PATHNAME", "/hostfs/repl-opensaf/saflog/ntfa.log", 1);

    srandom(getpid());

    if (argc > 1)
    {
        suite = atoi(argv[1]);
    }

    if (argc > 2)
    {
        tcase = atoi(argv[2]);
    }

    if (suite == 0)
    {
        test_list();
        return 0;
    }

    test_run(suite, tcase);

    return 0;
}


