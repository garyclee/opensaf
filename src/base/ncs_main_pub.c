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
 * Author(s): Emerson Network Power
 *
 */

/*****************************************************************************
..............................................................................

  MODULE NAME:       ncs_main_pub.c

  DESCRIPTION:       Contains an API that agent starts up NCS.

  TRACE GUIDE:
  Policy is to not use logging/syslog from library code.
  Only the trace part of logtrace is used from library.

  It is possible to turn on trace for the IMMA library used
  by an application process. This is done by the application
  defining the environment variable: IMMA_TRACE_PATHNAME.
  The trace will end up in the file defined by that variable.

  TRACE   debug traces                 - aproximates DEBUG
  TRACE_1 normal but important events  - aproximates INFO.
  TRACE_2 user errors with return code - aproximates NOTICE.
  TRACE_3 unusual or strange events    - aproximates WARNING
  TRACE_4 library errors ERR_LIBRARY   - aproximates ERROR
******************************************************************************
*/

#include "osaf/configmake.h"

#include <pthread.h>
#include <dlfcn.h>

#include "base/ncsgl_defs.h"
#include "mds/mds_papi.h"
#include "base/ncs_main_papi.h"
#include "base/ncs_mda_papi.h"
#include "base/ncs_lib.h"
#include "base/ncssysf_lck.h"
#include "mds/mds_dl_api.h"
#include "sprr_dl_api.h"
#include "mds/mda_dl_api.h"
#include "base/ncssysf_def.h"
#include "base/ncs_main_pub.h"
#include "base/osaf_utility.h"

/**************************************************************************\

       L O C A L      D A T A    S T R U C T U R E S

\**************************************************************************/

typedef uint32_t (*LIB_REQ)(NCS_LIB_REQ_INFO *);

typedef struct ncs_agent_data {
	uint32_t use_count;
	LIB_REQ lib_req;
} NCS_AGENT_DATA;

typedef struct ncs_main_pub_cb {
	void *lib_hdl;

	NCS_LOCK lock;
	bool core_started;
	uint32_t my_nodeid;
	uint32_t my_procid;

	uint32_t core_use_count;
	uint32_t leap_use_count;
	uint32_t mds_use_count;

	NCS_AGENT_DATA mbca;
} NCS_MAIN_PUB_CB;

static uint32_t mainget_node_id(uint32_t *node_id);
static uint32_t ncs_non_core_agents_startup(void);

static NCS_MAIN_PUB_CB gl_ncs_main_pub_cb;

/* mutex for synchronising agent startup and shutdown */
static pthread_mutex_t s_agent_startup_mutex = PTHREAD_MUTEX_INITIALIZER;

/* mutex protecting gl_ncs_main_pub_cb.core_started and
 * gl_ncs_main_pub_cb.core_use_count  */
static pthread_mutex_t s_leap_core_mutex = PTHREAD_MUTEX_INITIALIZER;

/***************************************************************************\

  PROCEDURE    :    ncs_agents_startup

\***************************************************************************/
unsigned int ncs_agents_startup(void)
{
	uint32_t rc = ncs_core_agents_startup();
	if (rc != NCSCC_RC_SUCCESS)
		return rc;

	rc = ncs_non_core_agents_startup();
	return rc;
}

/***************************************************************************\

  PROCEDURE    :    ncs_agents_shutdown

\***************************************************************************/
unsigned int ncs_agents_shutdown(void)
{
	ncs_mbca_shutdown();
	ncs_core_agents_shutdown();

	return NCSCC_RC_SUCCESS;
}

/***************************************************************************\

  PROCEDURE    :    ncs_leap_startup

\***************************************************************************/
unsigned int ncs_leap_startup(void)
{
	NCS_LIB_REQ_INFO lib_create;
	NCS_LIB_REQ_INFO lib_destroy;

	osaf_mutex_lock_ordie(&s_agent_startup_mutex);

	if (gl_ncs_main_pub_cb.leap_use_count > 0) {
		gl_ncs_main_pub_cb.leap_use_count++;
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return NCSCC_RC_SUCCESS;
	}

	/* Print the process-id for information sakes */
	gl_ncs_main_pub_cb.my_procid = (uint32_t)getpid();
	TRACE("\nNCS:PROCESS_ID=%d\n", gl_ncs_main_pub_cb.my_procid);

	memset(&lib_create, 0, sizeof(lib_create));
	lib_create.i_op = NCS_LIB_REQ_CREATE;
	lib_create.info.create.argc = 0;
	lib_create.info.create.argv = NULL;

	/* Initalize basic services */
	if (leap_env_init() != NCSCC_RC_SUCCESS) {
		TRACE_4("\nERROR: Couldn't initialised LEAP basic services \n");
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return NCSCC_RC_FAILURE;
	}

	if (sprr_lib_req(&lib_create) != NCSCC_RC_SUCCESS) {
		TRACE_4("\nERROR: SPRR lib_req NCS_LIB_REQ_CREATE failed \n");
		goto lib_req_fail;
	}

	/* Get & Update system specific arguments */
	if (mainget_node_id(&gl_ncs_main_pub_cb.my_nodeid) !=
	    NCSCC_RC_SUCCESS) {
		TRACE_4("Not able to get the NODE ID\n");
		goto get_node_fail;
	}
	TRACE("NCS:NODE_ID=0x%08X\n", gl_ncs_main_pub_cb.my_nodeid);
	gl_ncs_main_pub_cb.leap_use_count = 1;

	osaf_mutex_unlock_ordie(&s_agent_startup_mutex);

	/* start initializing all the required agents */
	gl_ncs_main_pub_cb.lib_hdl = dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL);

	return NCSCC_RC_SUCCESS;

get_node_fail:
	memset(&lib_destroy, 0, sizeof(lib_destroy));
	lib_destroy.i_op = NCS_LIB_REQ_DESTROY;
	lib_destroy.info.destroy.dummy = 0;
	if (sprr_lib_req(&lib_destroy) != NCSCC_RC_SUCCESS)
		TRACE_4("\nERROR: SPRR lib_req NCS_LIB_REQ_DESTROY failed\n");

lib_req_fail:
	leap_env_destroy();
	osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
	return NCSCC_RC_FAILURE;
}

/***************************************************************************\

  PROCEDURE    :    ncs_mds_startup

\***************************************************************************/
unsigned int ncs_mds_startup(void)
{
	NCS_LIB_REQ_INFO lib_create;

	osaf_mutex_lock_ordie(&s_agent_startup_mutex);

	if (!gl_ncs_main_pub_cb.leap_use_count) {
		TRACE_4("\nLEAP core not yet started.... \n");
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return NCSCC_RC_FAILURE;
	}

	if (gl_ncs_main_pub_cb.mds_use_count > 0) {
		gl_ncs_main_pub_cb.mds_use_count++;
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return NCSCC_RC_SUCCESS;
	}

	memset(&lib_create, 0, sizeof(lib_create));
	lib_create.i_op = NCS_LIB_REQ_CREATE;
	lib_create.info.create.argc = 0;
	lib_create.info.create.argv = NULL;

	/* STEP : Initialize the MDS layer */
	if (mds_lib_req(&lib_create) != NCSCC_RC_SUCCESS) {
		TRACE_4("ERROR: MDS lib_req failed \n");
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return NCSCC_RC_FAILURE;
	}

	/* STEP : Initialize the ADA/VDA layer */
	if (mda_lib_req(&lib_create) != NCSCC_RC_SUCCESS) {
		TRACE_4("ERROR: MDA lib_req failed \n");
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return NCSCC_RC_FAILURE;
	}

	gl_ncs_main_pub_cb.mds_use_count = 1;

	osaf_mutex_unlock_ordie(&s_agent_startup_mutex);

	return NCSCC_RC_SUCCESS;
}

/***************************************************************************\

  PROCEDURE    :    ncs_non_core_agents_startup

\***************************************************************************/
uint32_t ncs_non_core_agents_startup(void)
{
	uint32_t rc = ncs_mbca_startup();

	return rc;
}

/***************************************************************************\

  PROCEDURE    :    ncs_core_agents_startup

\***************************************************************************/
unsigned int ncs_core_agents_startup(void)
{
	osaf_mutex_lock_ordie(&s_leap_core_mutex);

	if (gl_ncs_main_pub_cb.core_use_count) {
		gl_ncs_main_pub_cb.core_use_count++;
		osaf_mutex_unlock_ordie(&s_leap_core_mutex);
		return NCSCC_RC_SUCCESS;
	}

	if (ncs_leap_startup() != NCSCC_RC_SUCCESS) {
		TRACE_4("ERROR: LEAP svcs startup failed \n");
		osaf_mutex_unlock_ordie(&s_leap_core_mutex);
		return m_LEAP_DBG_SINK(NCSCC_RC_FAILURE);
	}

	if (ncs_mds_startup() != NCSCC_RC_SUCCESS) {
		TRACE_4("ERROR: MDS startup failed \n");
		osaf_mutex_unlock_ordie(&s_leap_core_mutex);
		return m_LEAP_DBG_SINK(NCSCC_RC_FAILURE);
	}

	gl_ncs_main_pub_cb.core_started = true;
	gl_ncs_main_pub_cb.core_use_count = 1;

	osaf_mutex_unlock_ordie(&s_leap_core_mutex);
	return NCSCC_RC_SUCCESS;
}

/***************************************************************************\

  PROCEDURE    :    ncs_mbca_startup

\***************************************************************************/
unsigned int ncs_mbca_startup(void)
{
	NCS_LIB_REQ_INFO lib_create;

	if (!gl_ncs_main_pub_cb.core_started) {
		TRACE_4("\nNCS core not yet started.... \n");
		return NCSCC_RC_FAILURE;
	}

	memset(&lib_create, 0, sizeof(lib_create));
	lib_create.i_op = NCS_LIB_REQ_CREATE;
	lib_create.info.create.argc = 0;
	lib_create.info.create.argv = NULL;

	if (gl_ncs_main_pub_cb.lib_hdl == NULL)
		return NCSCC_RC_SUCCESS; /* No agents to load */

	osaf_mutex_lock_ordie(&s_agent_startup_mutex);

	if (gl_ncs_main_pub_cb.mbca.use_count > 0) {
		/* Already created, so just increment the use_count */
		gl_ncs_main_pub_cb.mbca.use_count++;
	} else {
		gl_ncs_main_pub_cb.mbca.lib_req =
		    (LIB_REQ)dlsym(gl_ncs_main_pub_cb.lib_hdl, "mbcsv_lib_req");
		if (gl_ncs_main_pub_cb.mbca.lib_req == NULL) {
			TRACE_4("\nMBCSV:MBCA:OFF");
		} else {
			if ((*gl_ncs_main_pub_cb.mbca.lib_req)(&lib_create) !=
			    NCSCC_RC_SUCCESS) {
				osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
				return m_LEAP_DBG_SINK(NCSCC_RC_FAILURE);
			} else {
				TRACE("\nMBCSV:MBCA:ON");
				gl_ncs_main_pub_cb.mbca.use_count = 1;
			}
		}
	}

	osaf_mutex_unlock_ordie(&s_agent_startup_mutex);

	return NCSCC_RC_SUCCESS;
}

/***************************************************************************\

  PROCEDURE    :    ncs_mbca_shutdown

\***************************************************************************/
unsigned int ncs_mbca_shutdown(void)
{
	NCS_LIB_REQ_INFO lib_destroy;
	uint32_t rc = NCSCC_RC_SUCCESS;

	osaf_mutex_lock_ordie(&s_agent_startup_mutex);
	if (gl_ncs_main_pub_cb.mbca.use_count > 1) {
		/* Still users extis, so just decrement the use_count */
		gl_ncs_main_pub_cb.mbca.use_count--;
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return rc;
	}

	memset(&lib_destroy, 0, sizeof(lib_destroy));
	lib_destroy.i_op = NCS_LIB_REQ_DESTROY;
	lib_destroy.info.destroy.dummy = 0;

	if (gl_ncs_main_pub_cb.mbca.lib_req != NULL)
		rc = (*gl_ncs_main_pub_cb.mbca.lib_req)(&lib_destroy);

	gl_ncs_main_pub_cb.mbca.use_count = 0;
	gl_ncs_main_pub_cb.mbca.lib_req = NULL;

	osaf_mutex_unlock_ordie(&s_agent_startup_mutex);

	return rc;
}

/***************************************************************************\

  PROCEDURE    :    ncs_leap_shutdown

\***************************************************************************/
void ncs_leap_shutdown()
{
	NCS_LIB_REQ_INFO lib_destroy;

	osaf_mutex_lock_ordie(&s_agent_startup_mutex);

	if (gl_ncs_main_pub_cb.leap_use_count > 1) {
		/* Still users extis, so just decrement the use_count */
		gl_ncs_main_pub_cb.leap_use_count--;
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return;
	}

	memset(&lib_destroy, 0, sizeof(lib_destroy));
	lib_destroy.i_op = NCS_LIB_REQ_DESTROY;
	lib_destroy.info.destroy.dummy = 0;

	dlclose(gl_ncs_main_pub_cb.lib_hdl);
	gl_ncs_main_pub_cb.lib_hdl = NULL;

	sprr_lib_req(&lib_destroy);

	leap_env_destroy();

	gl_ncs_main_pub_cb.leap_use_count = 0;
	gl_ncs_main_pub_cb.core_started = false;

	osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
}

/***************************************************************************\

  PROCEDURE    :    ncs_mds_shutdown

\***************************************************************************/
void ncs_mds_shutdown()
{
	NCS_LIB_REQ_INFO lib_destroy;

	osaf_mutex_lock_ordie(&s_agent_startup_mutex);

	if (gl_ncs_main_pub_cb.mds_use_count > 1) {
		/* Still users extis, so just decrement the use_count */
		gl_ncs_main_pub_cb.mds_use_count--;
		osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
		return;
	}

	memset(&lib_destroy, 0, sizeof(lib_destroy));
	lib_destroy.i_op = NCS_LIB_REQ_DESTROY;
	lib_destroy.info.destroy.dummy = 0;

	mda_lib_req(&lib_destroy);
	mds_lib_req(&lib_destroy);

	gl_ncs_main_pub_cb.mds_use_count = 0;
	gl_ncs_main_pub_cb.core_started = false;

	osaf_mutex_unlock_ordie(&s_agent_startup_mutex);
}

/***************************************************************************\

  PROCEDURE    :    ncs_core_agents_shutdown

\***************************************************************************/
unsigned int ncs_core_agents_shutdown()
{
	osaf_mutex_lock_ordie(&s_leap_core_mutex);

	if (!gl_ncs_main_pub_cb.core_use_count) {
		TRACE_4("\nNCS core not yet started.... \n");
		osaf_mutex_unlock_ordie(&s_leap_core_mutex);
		return NCSCC_RC_FAILURE;
	}

	if (gl_ncs_main_pub_cb.core_use_count > 1) {
		/* Decrement the use count */
		gl_ncs_main_pub_cb.core_use_count--;
		osaf_mutex_unlock_ordie(&s_leap_core_mutex);
		return NCSCC_RC_SUCCESS;
	}

	/* Shutdown basic services */
	ncs_mds_shutdown();
	ncs_leap_shutdown();
	gl_ncs_main_pub_cb.core_use_count = 0;

	osaf_mutex_unlock_ordie(&s_leap_core_mutex);
	return NCSCC_RC_SUCCESS;
}

/***************************************************************************\

  PROCEDURE    :    ncs_get_node_id

\***************************************************************************/
NCS_NODE_ID ncs_get_node_id(void)
{
	if (!gl_ncs_main_pub_cb.leap_use_count) {
		return m_LEAP_DBG_SINK(0);
	}

	return gl_ncs_main_pub_cb.my_nodeid;
}

uint32_t mainget_node_id(uint32_t *node_id)
{
	uint32_t res = NCSCC_RC_FAILURE;

	FILE *fp = fopen(PKGLOCALSTATEDIR "/node_id", "r");

	if (fp != NULL) {
		if (fscanf(fp, "%x", node_id) == 1) {
			res = NCSCC_RC_SUCCESS;
		}
		fclose(fp);
	}

	return res;
}

/***************************************************************************\

  PROCEDURE    :    ncs_util_search_argv_list

\***************************************************************************/
char *ncs_util_search_argv_list(int argc, char *argv[], char *arg_prefix)
{
	/* Check   argv[argc-1] through argv[1] */
	for (; argc > 1; argc--) {
		char *tmp = strstr(argv[argc - 1], arg_prefix);
		if (tmp != NULL)
			return tmp;
	}
	return NULL;
}
