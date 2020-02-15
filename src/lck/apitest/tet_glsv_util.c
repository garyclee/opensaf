/******************************************************************************************
#
# modification:
# 2008-11   Jony <jiangnan_hw@huawei.com>  Huawei Technologies CO., LTD.
#   1.  Provide test case tet_startup and tet_cleanup functions. The tet_startup
function #       is defined as tet_glsv_startup(), as well as the tet_cleanup
function is defined #       as tet_glsv_end(). #   2.  If "tetware-patch" is not
used, change the name of test case array from #       glsv_onenode_testlist[] to
tet_testlist[]. #   3.  If "tetware-patch" is not used, delete the third
parameter of each item in this #       array. And add some functions to
implement the function of those parameters. #   4.  If "tetware-patch" is not
used, delete tet_glsv_start_instance() function and parts #       of
tet_run_glsv_app(), which invoke the functions in the patch. #   5.  If
"tetware-patch" is required, one can add a macro definition "TET_PATCH=1" in #
compilation process.
#
********************************************************************************************/

/*#include "tet_startup.h"*/
#include "base/ncs_main_papi.h"
#include "tet_glsv.h"
#include "osaf/apitest/utest.h"

void tet_run_gld();
void tet_run_glnd();
void tet_run_glsv_app();

void tet_glsv_startup(void);
void tet_glsv_end(void);

void (*tet_startup)() = tet_glsv_startup;
void (*tet_cleanup)() = tet_glsv_end;

#if (TET_PATCH == 1)

/* ******** TET_LIST Arrays ********* */

struct tet_testlist glsv_onenode_testlist[] = {
    {glsv_it_init_01, 1},
    {glsv_it_init_02, 2},
    {glsv_it_init_03, 3},
    {glsv_it_init_04, 4},
    {glsv_it_init_05, 5},
    {glsv_it_init_06, 6},
    {glsv_it_init_07, 7},
    {glsv_it_init_08, 8},
    {glsv_it_init_09, 9},
    {glsv_it_init_10, 10},

    {glsv_it_selobj_01, 11},
    {glsv_it_selobj_02, 12},
    {glsv_it_selobj_03, 13},
    {glsv_it_selobj_04, 14},
    {glsv_it_selobj_05, 15},

    {glsv_it_option_chk_01, 16},
    {glsv_it_option_chk_02, 17},
    {glsv_it_option_chk_03, 18},

    {glsv_it_dispatch_01, 19},
    {glsv_it_dispatch_02, 20},
    {glsv_it_dispatch_03, 21},
    {glsv_it_dispatch_04, 22},
    {glsv_it_dispatch_05, 23},
    {glsv_it_dispatch_06, 24},
    {glsv_it_dispatch_07, 25},
    {glsv_it_dispatch_08, 26},
    {glsv_it_dispatch_09, 27},

    {glsv_it_finalize_01, 28},
    {glsv_it_finalize_02, 29},
    {glsv_it_finalize_03, 30},
    {glsv_it_finalize_04, 31},
    {glsv_it_finalize_05, 32},
    {glsv_it_finalize_06, 33},

    {glsv_it_res_open_01, 34},
    {glsv_it_res_open_02, 35},
    {glsv_it_res_open_03, 36},
    {glsv_it_res_open_04, 37},
    {glsv_it_res_open_05, 38},
    {glsv_it_res_open_06, 39},
    {glsv_it_res_open_07, 40},
    {glsv_it_res_open_08, 41},
    {glsv_it_res_open_09, 42},
    {glsv_it_res_open_10, 43},

    {glsv_it_res_open_async_01, 44},
    {glsv_it_res_open_async_02, 45},
    {glsv_it_res_open_async_03, 46},
    {glsv_it_res_open_async_04, 47},
    {glsv_it_res_open_async_05, 48},
    {glsv_it_res_open_async_06, 49},
    {glsv_it_res_open_async_07, 50},
    {glsv_it_res_open_async_08, 51},
    {glsv_it_res_open_async_09, 52},
    {glsv_it_res_open_async_10, 53},

    {glsv_it_res_close_01, 54},
    {glsv_it_res_close_02, 55},
    {glsv_it_res_close_03, 56},
    {glsv_it_res_close_04, 57},
    {glsv_it_res_close_05, 58},
    {glsv_it_res_close_06, 59},
    {glsv_it_res_close_07, 60},
    {glsv_it_res_close_08, 61},
    {glsv_it_res_close_09, 62},
    {glsv_it_res_close_10, 63},
    {glsv_it_res_close_11, 64},

    {glsv_it_res_lck_01, 65},
    {glsv_it_res_lck_02, 66},
    {glsv_it_res_lck_03, 67},
    {glsv_it_res_lck_04, 68},
    {glsv_it_res_lck_05, 69},
    {glsv_it_res_lck_06, 70},
    {glsv_it_res_lck_07, 71},
    {glsv_it_res_lck_08, 72},
    {glsv_it_res_lck_09, 73},
    {glsv_it_res_lck_10, 74},
    {glsv_it_res_lck_11, 75},
    {glsv_it_res_lck_12, 76},
    {glsv_it_res_lck_13, 77},
    {glsv_it_res_lck_14, 78},
    {glsv_it_res_lck_15, 79},
    {glsv_it_res_lck_16, 80},
    {glsv_it_res_lck_17, 81},
    {glsv_it_res_lck_18, 82},

    {glsv_it_res_lck_async_01, 83},
    {glsv_it_res_lck_async_02, 84},
    {glsv_it_res_lck_async_03, 85},
    {glsv_it_res_lck_async_04, 86},
    {glsv_it_res_lck_async_05, 87},
    {glsv_it_res_lck_async_06, 88},
    {glsv_it_res_lck_async_07, 89},
    {glsv_it_res_lck_async_08, 90},
    {glsv_it_res_lck_async_09, 91},
    {glsv_it_res_lck_async_10, 92},
    {glsv_it_res_lck_async_11, 93},
    {glsv_it_res_lck_async_12, 94},
    {glsv_it_res_lck_async_13, 95},
    {glsv_it_res_lck_async_14, 96},
    {glsv_it_res_lck_async_15, 97},
    {glsv_it_res_lck_async_16, 98},
    {glsv_it_res_lck_async_17, 99},
    {glsv_it_res_lck_async_18, 100},
    {glsv_it_res_lck_async_19, 101},
    {glsv_it_res_lck_async_20, 102},

    {glsv_it_res_unlck_01, 103},
    {glsv_it_res_unlck_02, 104},
    {glsv_it_res_unlck_03, 105},
    {glsv_it_res_unlck_04, 106},
    {glsv_it_res_unlck_05, 107},
    {glsv_it_res_unlck_06, 108},
    {glsv_it_res_unlck_07, 109},
    {glsv_it_res_unlck_08, 110},

    {glsv_it_res_unlck_async_01, 111},
    {glsv_it_res_unlck_async_02, 112},
    {glsv_it_res_unlck_async_03, 113},
    {glsv_it_res_unlck_async_04, 114},
    {glsv_it_res_unlck_async_05, 115},
    {glsv_it_res_unlck_async_06, 116},
    {glsv_it_res_unlck_async_07, 117},
    {glsv_it_res_unlck_async_08, 118},
    {glsv_it_res_unlck_async_09, 119},
    {glsv_it_res_unlck_async_10, 120},

    {glsv_it_lck_purge_01, 121},
    {glsv_it_lck_purge_02, 122},
    {glsv_it_lck_purge_03, 123},
    {glsv_it_lck_purge_04, 124},
    {glsv_it_lck_purge_05, 125},

    {glsv_it_res_cr_del_01, 126},
    {glsv_it_res_cr_del_02, 127},
    {glsv_it_res_cr_del_03, 128},
    {glsv_it_res_cr_del_04, 129},
    {glsv_it_res_cr_del_05, 130},
    {glsv_it_res_cr_del_06, 131, 0},
    {glsv_it_res_cr_del_06, 132, 1},

    {glsv_it_lck_modes_wt_clbk_01, 133, 0},
    {glsv_it_lck_modes_wt_clbk_01, 134, 1},
    {glsv_it_lck_modes_wt_clbk_02, 135, 0},
    {glsv_it_lck_modes_wt_clbk_02, 136, 1},
    {glsv_it_lck_modes_wt_clbk_03, 137, 0},
    {glsv_it_lck_modes_wt_clbk_03, 138, 1},
    {glsv_it_lck_modes_wt_clbk_04, 139, 0},
    {glsv_it_lck_modes_wt_clbk_04, 140, 1},
    {glsv_it_lck_modes_wt_clbk_05, 141},
    {glsv_it_lck_modes_wt_clbk_06, 142},
    {glsv_it_lck_modes_wt_clbk_07, 143},
    {glsv_it_lck_modes_wt_clbk_08, 144},
    {glsv_it_lck_modes_wt_clbk_09, 145},

    {glsv_it_ddlcks_orplks_01, 146},
    {glsv_it_ddlcks_orplks_02, 147, 0},
    {glsv_it_ddlcks_orplks_02, 148, 1},
    {glsv_it_ddlcks_orplks_03, 149, 0},
    {glsv_it_ddlcks_orplks_03, 150, 1},
    {glsv_it_ddlcks_orplks_04, 151, 0},
    {glsv_it_ddlcks_orplks_04, 152, 1},
    {glsv_it_ddlcks_orplks_05, 153},
    {glsv_it_ddlcks_orplks_06, 154},
    {glsv_it_ddlcks_orplks_07, 155, 0},
    {glsv_it_ddlcks_orplks_07, 156, 1},

    {glsv_it_lck_strip_purge_01, 157},
    {glsv_it_lck_strip_purge_02, 158},
    {glsv_it_lck_strip_purge_03, 159},
    {NULL, -1}};

void tet_glsv_start_instance(TET_GLSV_INST *inst)
{
	int iter;
	int num_of_iter = 1;
	int test_case_num = -1;
	struct tet_testlist *glsv_testlist = glsv_onenode_testlist;

	if (inst->tetlist_index == GLSV_ONE_NODE_LIST)
		glsv_testlist = glsv_onenode_testlist;

	if (inst->num_of_iter)
		num_of_iter = inst->num_of_iter;

	if (inst->test_case_num)
		test_case_num = inst->test_case_num;

	for (iter = 0; iter < num_of_iter; iter++)
		tet_test_start(test_case_num, glsv_testlist);
}

#else

/* ******** TET_LIST Arrays ********* */
struct tet_testlist tet_testlist[] = {{glsv_it_init_01, 1},
				      {glsv_it_init_02, 2},
				      {glsv_it_init_03, 3},
				      {glsv_it_init_04, 4},
				      {glsv_it_init_05, 5},
				      {glsv_it_init_06, 6},
				      {glsv_it_init_07, 7},
				      {glsv_it_init_08, 8},
				      {glsv_it_init_09, 9},
				      {glsv_it_init_10, 10},

				      {glsv_it_selobj_01, 11},
				      {glsv_it_selobj_02, 12},
				      {glsv_it_selobj_03, 13},
				      {glsv_it_selobj_04, 14},
				      {glsv_it_selobj_05, 15},

				      {glsv_it_option_chk_01, 16},
				      {glsv_it_option_chk_02, 17},
				      {glsv_it_option_chk_03, 18},

				      {glsv_it_dispatch_01, 19},
				      {glsv_it_dispatch_02, 20},
				      {glsv_it_dispatch_03, 21},
				      {glsv_it_dispatch_04, 22},
				      {glsv_it_dispatch_05, 23},
				      {glsv_it_dispatch_06, 24},
				      {glsv_it_dispatch_07, 25},
				      {glsv_it_dispatch_08, 26},
				      {glsv_it_dispatch_09, 27},

				      {glsv_it_finalize_01, 28},
				      {glsv_it_finalize_02, 29},
				      {glsv_it_finalize_03, 30},
				      {glsv_it_finalize_04, 31},
				      {glsv_it_finalize_05, 32},
				      {glsv_it_finalize_06, 33},

				      {glsv_it_res_open_01, 34},
				      {glsv_it_res_open_02, 35},
				      {glsv_it_res_open_03, 36},
				      {glsv_it_res_open_04, 37},
				      {glsv_it_res_open_05, 38},
				      {glsv_it_res_open_06, 39},
				      {glsv_it_res_open_07, 40},
				      {glsv_it_res_open_08, 41},
				      {glsv_it_res_open_09, 42},
				      {glsv_it_res_open_10, 43},

				      {glsv_it_res_open_async_01, 44},
				      {glsv_it_res_open_async_02, 45},
				      {glsv_it_res_open_async_03, 46},
				      {glsv_it_res_open_async_04, 47},
				      {glsv_it_res_open_async_05, 48},
				      {glsv_it_res_open_async_06, 49},
				      {glsv_it_res_open_async_07, 50},
				      {glsv_it_res_open_async_08, 51},
				      {glsv_it_res_open_async_09, 52},
				      {glsv_it_res_open_async_10, 53},

				      {glsv_it_res_close_01, 54},
				      {glsv_it_res_close_02, 55},
				      {glsv_it_res_close_03, 56},
				      {glsv_it_res_close_04, 57},
				      {glsv_it_res_close_05, 58},
				      {glsv_it_res_close_06, 59},
				      {glsv_it_res_close_07, 60},
				      {glsv_it_res_close_08, 61},
				      {glsv_it_res_close_09, 62},
				      {glsv_it_res_close_10, 63},
				      {glsv_it_res_close_11, 64},

				      {glsv_it_res_lck_01, 65},
				      {glsv_it_res_lck_02, 66},
				      {glsv_it_res_lck_03, 67},
				      {glsv_it_res_lck_04, 68},
				      {glsv_it_res_lck_05, 69},
				      {glsv_it_res_lck_06, 70},
				      {glsv_it_res_lck_07, 71},
				      {glsv_it_res_lck_08, 72},
				      {glsv_it_res_lck_09, 73},
				      {glsv_it_res_lck_10, 74},
				      {glsv_it_res_lck_11, 75},
				      {glsv_it_res_lck_12, 76},
				      {glsv_it_res_lck_13, 77},
				      {glsv_it_res_lck_14, 78},
				      {glsv_it_res_lck_15, 79},
				      {glsv_it_res_lck_16, 80},
				      {glsv_it_res_lck_17, 81},
				      {glsv_it_res_lck_18, 82},

				      {glsv_it_res_lck_async_01, 83},
				      {glsv_it_res_lck_async_02, 84},
				      {glsv_it_res_lck_async_03, 85},
				      {glsv_it_res_lck_async_04, 86},
				      {glsv_it_res_lck_async_05, 87},
				      {glsv_it_res_lck_async_06, 88},
				      {glsv_it_res_lck_async_07, 89},
				      {glsv_it_res_lck_async_08, 90},
				      {glsv_it_res_lck_async_09, 91},
				      {glsv_it_res_lck_async_10, 92},
				      {glsv_it_res_lck_async_11, 93},
				      {glsv_it_res_lck_async_12, 94},
				      {glsv_it_res_lck_async_13, 95},
				      {glsv_it_res_lck_async_14, 96},
				      {glsv_it_res_lck_async_15, 97},
				      {glsv_it_res_lck_async_16, 98},
				      {glsv_it_res_lck_async_17, 99},
				      {glsv_it_res_lck_async_18, 100},
				      {glsv_it_res_lck_async_19, 101},
				      {glsv_it_res_lck_async_20, 102},

				      {glsv_it_res_unlck_01, 103},
				      {glsv_it_res_unlck_02, 104},
				      {glsv_it_res_unlck_03, 105},
				      {glsv_it_res_unlck_04, 106},
				      {glsv_it_res_unlck_05, 107},
				      {glsv_it_res_unlck_06, 108},
				      {glsv_it_res_unlck_07, 109},
				      {glsv_it_res_unlck_08, 110},

				      {glsv_it_res_unlck_async_01, 111},
				      {glsv_it_res_unlck_async_02, 112},
				      {glsv_it_res_unlck_async_03, 113},
				      {glsv_it_res_unlck_async_04, 114},
				      {glsv_it_res_unlck_async_05, 115},
				      {glsv_it_res_unlck_async_06, 116},
				      {glsv_it_res_unlck_async_07, 117},
				      {glsv_it_res_unlck_async_08, 118},
				      {glsv_it_res_unlck_async_09, 119},
				      {glsv_it_res_unlck_async_10, 120},

				      {glsv_it_lck_purge_01, 121},
				      {glsv_it_lck_purge_02, 122},
				      {glsv_it_lck_purge_03, 123},
				      {glsv_it_lck_purge_04, 124},
				      {glsv_it_lck_purge_05, 125},

				      {glsv_it_res_cr_del_01, 126},
				      {glsv_it_res_cr_del_02, 127},
				      {glsv_it_res_cr_del_03, 128},
				      {glsv_it_res_cr_del_04, 129},
				      {glsv_it_res_cr_del_05, 130},
				      //{glsv_it_res_cr_del_06_01, 131},
				      //{glsv_it_res_cr_del_06_02, 132},

				      //{glsv_it_lck_modes_wt_clbk_01_01, 133},
				      //{glsv_it_lck_modes_wt_clbk_01_02, 134},
				      //{glsv_it_lck_modes_wt_clbk_02_01, 135},
				      //{glsv_it_lck_modes_wt_clbk_02_02, 136},
				      //{glsv_it_lck_modes_wt_clbk_03_01, 137},
				      //{glsv_it_lck_modes_wt_clbk_03_02, 138},
				      //{glsv_it_lck_modes_wt_clbk_04_01, 139},
				      //{glsv_it_lck_modes_wt_clbk_04_02, 140},
				      {glsv_it_lck_modes_wt_clbk_05, 141},
				      {glsv_it_lck_modes_wt_clbk_06, 142},
				      {glsv_it_lck_modes_wt_clbk_07, 143},
				      {glsv_it_lck_modes_wt_clbk_08, 144},
				      {glsv_it_lck_modes_wt_clbk_09, 145},

				      {glsv_it_ddlcks_orplks_01, 146},
				      {glsv_it_ddlcks_orplks_05, 153},
				      {glsv_it_ddlcks_orplks_06, 154},

				      {glsv_it_lck_strip_purge_01, 157},
				      {glsv_it_lck_strip_purge_02, 158},
				      {glsv_it_lck_strip_purge_03, 159},

				      {NULL, -1}};

#endif

void tet_run_glsv_app()
{
	TET_GLSV_INST inst;

	tet_glsv_get_inputs(&inst);
	init_glsv_test_env();
	tet_glsv_fill_inputs(&inst);

#if (TET_PATCH == 1)

#ifndef TET_ALL

#ifdef TET_SMOKE_TEST
#endif

	tet_glsv_start_instance(&inst);

	m_TET_GLSV_PRINTF("\n ##### End of Testcase #####\n");
	printf("\n PRESS ENTER TO GET THE MEM DUMP\n");
	getchar();

	sleep(5);

	tet_test_cleanup();

#else

	while (1) {
		tet_test_red_lckInitialize(LCK_INIT_SUCCESS_T, TEST_CONFIG_MODE);
		tet_test_red_lckInitialize(LCK_INIT_SUCCESS_HDL2_T,
				       TEST_CONFIG_MODE);
		glsv_createthread_all_loop(1);
		glsv_createthread_all_loop(2);
		tet_test_red_lckFinalize(LCK_FINALIZE_SUCCESS_T, TEST_CONFIG_MODE);
		tet_test_red_lckFinalize(LCK_FINALIZE_SUCCESS_HDL2_T,
				     TEST_CONFIG_MODE);

		m_TET_GLSV_PRINTF(
		    "\n\n ******  Starting the API testcases ****** \n");
		tet_test_start(-1, glsv_onenode_testlist);
		m_TET_GLSV_PRINTF(
		    "\n ****** Ending the API testcases ****** \n");
	}

#endif

#endif
}

void tet_run_glsv_dist_cases() {}

void tet_glsv_startup(void)
{
	/* Using the default mode for startup */
	ncs_agents_startup();

#if (TET_D == 1)
	tet_run_gld();
#endif

#if (TET_ND == 1)
	tet_run_glnd();
#endif

	tet_run_glsv_app();
}

void tet_glsv_end() { printf(" Ending the agony.. "); }

__attribute__((constructor)) static void glsv_constructor(void)
{
  tet_glsv_startup();

  test_suite_add(1, "saLckInitialize Test Suite");
  test_case_add(1, glsv_it_init_01,
          "saLckInitialize with valid parameters");
  test_case_add(1, glsv_it_init_02,
      "saLckInitialize with NULL callback structure");
  test_case_add(1, glsv_it_init_03,
      "saLckInitialize with NULL version parameter");
  test_case_add(1, glsv_it_init_04,
      "saLckInitialize with NULL lock handle");
  test_case_add(1, glsv_it_init_05,
      "saLckInitialize with NULL callback and version paramters");
  test_case_add(1, glsv_it_init_06,
      "saLckInitialize with release code > supported release code");
  test_case_add(1, glsv_it_init_07,
      "saLckInitialize with invalid release code in version");
  test_case_add(1, glsv_it_init_08,
      "saLckInitialize with major version > supported major version");
  test_case_add(1, glsv_it_init_09,
      "saLckInitialize returns supported version when called with invalid version");
  test_case_add(1, glsv_it_init_10,
      "saLckInitialize without registering any callback");


  test_suite_add(2, "saLckSelectionObjectGet Test Suite");
  test_case_add(2,
                glsv_it_selobj_01,
                "saLckSelectionObjectGet with valid parameters");
  test_case_add(2,
                glsv_it_selobj_02,
                "saLckSelectionObjectGet with NULL selection object parameter");
  test_case_add(2,
                glsv_it_selobj_03,
                "saLckSelectionObjectGet with uninitialized lock handle");
  test_case_add(2,
                glsv_it_selobj_04,
                "saLckSelectionObjectGet with finalized lock handle");
  test_case_add(2,
                glsv_it_selobj_05,
                "saLckSelectionObjectGet when called twice with same lock handle");

  test_suite_add(3, "saLckOptionCheck Test Suite");
  test_case_add(3,
                glsv_it_option_chk_01,
                "saLckOptionCheck with invalid lock handle");
  test_case_add(3,
                glsv_it_option_chk_02,
                "saLckOptionCheck with NULL pointer to lckOptions parameter");
  test_case_add(3,
                glsv_it_option_chk_03,
                "saLckOptionCheck with valid parameters");

  test_suite_add(4, "saLckDispatch Test Suite");
  test_case_add(4,
                glsv_it_dispatch_01,
                "saLckDispatch invokes pending callbacks - SA_DISPATCH_ONE");
  test_case_add(4,
                glsv_it_dispatch_02,
                "saLckDispatch invokes pending callbacks - SA_DISPATCH_ALL");
  test_case_add(4,
                glsv_it_dispatch_03,
                "saLckDispatch invokes pending callbacks - SA_DISPATCH_BLOCKING");
  test_case_add(4,
                glsv_it_dispatch_04,
                "saLckDispatch with invalid dispatch flags");
  test_case_add(4,
                glsv_it_dispatch_05,
                "saLckDispatch with invalid lock handle - SA_DISPATCH_ONE");
  test_case_add(4,
                glsv_it_dispatch_06,
                "saLckDispatch with invalid lock handle - SA_DISPATCH_ALL");
  test_case_add(4,
                glsv_it_dispatch_07,
                "saLckDispatch with invalid lock handle - SA_DISPATCH_BLOCKING");
  test_case_add(4,
                glsv_it_dispatch_08,
                "saLckDispatch in case of no pending callbacks - SA_DISPATCH_ONE");
  test_case_add(4,
                glsv_it_dispatch_09,
                "saLckDispatch in case of no pending callbacks - SA_DISPATCH_ALL");

  test_suite_add(5, "saLckFinalize Test Suite");
  test_case_add(5,
                glsv_it_finalize_01,
                "saLckFinalize closes association between Message Service and app process");
  test_case_add(5,
                glsv_it_finalize_02,
                "saLckFinalize with uninitialized lock handle");
  test_case_add(5,
                glsv_it_finalize_03,
                "saLckFinalize with finalized lock handle");
  test_case_add(5,
                glsv_it_finalize_04,
                "Selection object becomes invalid after finalizing the lock handle");
  test_case_add(5,
                glsv_it_finalize_05,
                "Resources that are opened are closed after finalizing the lock handle");
  test_case_add(5,
                glsv_it_finalize_06,
                "All locks and lock requests with the resource hdls associated with the lock hdl "
      "are dropped after finalizing lck hdl");

  test_suite_add(6, "saLckResourceOpen Test Suite");
  test_case_add(6,
                glsv_it_res_open_01,
                "saLckResourceOpen with invalid lock handle");
  test_case_add(6,
                glsv_it_res_open_02,
                "saLckResourceOpen with NULL lock resource name");
  test_case_add(6,
                glsv_it_res_open_03,
                "saLckResourceOpen with NULL lock resource handle");
  test_case_add(6,
                glsv_it_res_open_04,
                "saLckResourceOpen with invalid resource flags");
  test_case_add(6,
                glsv_it_res_open_05,
                "saLckResourceOpen with small timeout value");
  test_case_add(6,
                glsv_it_res_open_06,
                "Open a resource that does not exist without SA_LCK_RESOURCE_CREATE flag");
  test_case_add(6,
                glsv_it_res_open_07,
                "Create a lock resource");
  test_case_add(6,
                glsv_it_res_open_08,
                "Open a resource that already exists and open");
  test_case_add(6,
                glsv_it_res_open_09,
                "Open a resource that already exists and open with SA_LCK_RESOURCE_CREATE flag");
  test_case_add(6, glsv_it_res_open_10, "Open a closed resource");

  test_suite_add(7, "saLckResourceOpenAsync Test Suite");
  test_case_add(7, glsv_it_res_open_async_01, "saLckResourceOpenAsync with invalid lock handle");
  test_case_add(7, glsv_it_res_open_async_02, "saLckResourceOpenAsync with NULL lock resource name");
  test_case_add(7, glsv_it_res_open_async_03, "saLckResourceOpenAsync with invalid resource flags");
  test_case_add(7, glsv_it_res_open_async_04, "Open a resource that does not exist without SA_LCK_RESOURCE_CREATE flag");
  test_case_add(7, glsv_it_res_open_async_05, "Create a lock resource");
  test_case_add(7, glsv_it_res_open_async_06, "Invocation in open callback is same as that supplied in saLckResourceOpenAsync");
  test_case_add(7, glsv_it_res_open_async_07, "saLckResourceOpenAsync without registering with resource open callback");
  test_case_add(7, glsv_it_res_open_async_08, "Open a resource that already exists and that is open");
  test_case_add(7, glsv_it_res_open_async_09, "Open a resource that already exists and that is open with create flag");
  test_case_add(7, glsv_it_res_open_async_10, "Open a closed resource");

  test_suite_add(8, "saLckResourceClose Test Suite");
  test_case_add(8,
                glsv_it_res_close_01,
                "Close a lock resource");
  test_case_add(8,
                glsv_it_res_close_02,
                "saLckResourceClose with invalid resource handle");
  test_case_add(8,
                glsv_it_res_close_03,
                "saLckResourceClose with a resource handle associated with finalized lock handle");
  test_case_add(8,
                glsv_it_res_close_04,
                "saLckResourceClose closes the reference to that resource");
  test_case_add(8,
                glsv_it_res_close_05,
                "saLckResourceClose drops all the locks held on that resource handle");
  test_case_add(8,
                glsv_it_res_close_06,
                "saLckResourceClose drops all the lock requests made on that resource handle");
  test_case_add(8,
                glsv_it_res_close_07,
                "saLckResourceClose does not effect the locks held on the resource by other appls");
  test_case_add(8,
                glsv_it_res_close_08,
                "saLckResourceClose with a resource handle that is already closed");
  test_case_add(8,
                glsv_it_res_close_09,
                "The resource no longer exists once all references to it are closed");
  test_case_add(8,
                glsv_it_res_close_10,
                "saLckResourceClose cancels all the pending locks that "
                "refer to the resource handle");
  test_case_add(8,
                glsv_it_res_close_11,
                "saLckResourceClose cancels all the pending callbacks that "
                "refer to the resource handle");

  test_suite_add(9, "saLckResourceLock Test Suite");
  test_case_add(9,
                glsv_it_res_lck_01,
                "saLckResourceLock with invalid resource handle");
  test_case_add(9,
                glsv_it_res_lck_02,
                "saLckResourceLock after finalizing the lock handle");
  test_case_add(9,
                glsv_it_res_lck_03,
                "saLckResourceLock after closing the resource handle");
  test_case_add(9,
                glsv_it_res_lck_04,
                "saLckResourceLock with NULL lock id parameter");
  test_case_add(9,
                glsv_it_res_lck_05,
                "saLckResourceLock with NULL lock status parameter");
  test_case_add(9,
                glsv_it_res_lck_06,
                "saLckResourceLock with invalid lock mode");
  test_case_add(9,
                glsv_it_res_lck_07,
                "saLckResourceLock with invalid lock flag value");
  test_case_add(9,
                glsv_it_res_lck_08,
                "saLckResourceLock with small timeout value");
  test_case_add(9,
                glsv_it_res_lck_09,
                "Request a PR lock on the resource");
  test_case_add(9,
                glsv_it_res_lck_10,
                "Request a PR lock on the resource with SA_LCK_LOCK_NO_QUEUE flag");
  test_case_add(9,
                glsv_it_res_lck_11,
                "Request a PR lock on the resource with SA_LCK_LOCK_ORPHAN flag");
  test_case_add(9,
                glsv_it_res_lck_12,
                "Request an EX lock on the resource");
  test_case_add(9,
                glsv_it_res_lck_13,
                "Request an EX lock on the resource with SA_LCK_LOCK_NO_QUEUE flag");
  test_case_add(9,
                glsv_it_res_lck_14,
                "Request an EX lock on the resource with SA_LCK_LOCK_ORPHAN flag");
  test_case_add(9,
                glsv_it_res_lck_15,
                "Request a PR lock on a resource on which an EX lock is held by another appl");
  test_case_add(9,
                glsv_it_res_lck_16,
                "Request a PR lock on a resource on which an EX lock is held using same res hdl");
  test_case_add(9,
                glsv_it_res_lck_17,
                "Request an EX lock on a resource on which an EX lock is held using same res hdl");
  test_case_add(9,
                glsv_it_res_lck_18,
                "Request a PR lock with SA_LCK_LOCK_NO_QUEUE on a resource on "
                "which an EX lock is held");

  test_suite_add(10, "saLckResourceLockAsync Test Suite");
  test_case_add(10,
                glsv_it_res_lck_async_01,
                "saLckResourceLockAsync with invalid resource handle");
  test_case_add(10,
                glsv_it_res_lck_async_02,
                "saLckResourceLockAsync with resource handle associated with finalized lock handle");
  test_case_add(10,
                glsv_it_res_lck_async_03,
                "saLckResourceLockAsync with closed resource handle");
  test_case_add(10,
                glsv_it_res_lck_async_04,
                "saLckResourceLockAsync with NULL lock id parameter");
  test_case_add(10,
                glsv_it_res_lck_async_05,
                "saLckResourceLockAsync with invalid lock mode parameter");
  test_case_add(10,
                glsv_it_res_lck_async_06,
                "saLckResourceLockAsync with invalid lock flag parameter");
  test_case_add(10,
                glsv_it_res_lck_async_07,
                "saLckResourceLockAsync with lock hdl initialized with NULL grant callback");
  test_case_add(10,
                glsv_it_res_lck_async_08,
                "Request a PR lock on the resource");
  test_case_add(10,
                glsv_it_res_lck_async_09,
                "Invocation value in grant callback is same as that in the api call");
  test_case_add(10,
                glsv_it_res_lck_async_10,
                "Request a PR lock on the resource with SA_LCK_LOCK_NO_QUEUE flag");
  test_case_add(10,
                glsv_it_res_lck_async_11,
                "Request a PR lock on the resource with SA_LCK_LOCK_ORPHAN flag");
  test_case_add(10,
                glsv_it_res_lck_async_12,
                "Request an EX lock on the resource");
  test_case_add(10,
                glsv_it_res_lck_async_13,
                "Request an EX lock on the resource with SA_LCK_LOCK_NO_QUEUE flag");
  test_case_add(10,
                glsv_it_res_lck_async_14,
                "Request an EX lock on the resource with SA_LCK_LOCK_ORPHAN flag");
  test_case_add(10,
                glsv_it_res_lck_async_15,
                "Request a PR lock on a resource on which an EX lock is held by another appl");
  test_case_add(10,
                glsv_it_res_lck_async_16,
                "Request a PR lock on a resource on which an EX lock is held using same res hdl");
  test_case_add(10,
                glsv_it_res_lck_async_17,
                "Request an EX lock on a resource on which an EX lock is held using same res hdl");
  test_case_add(10,
                glsv_it_res_lck_async_18,
                "Request a PR lock with SA_LCK_LOCK_NO_QUEUE on a resource on "
                "which an EX lock is held");
  test_case_add(10,
                glsv_it_res_lck_async_19,
                "Lock id value obtained is valid before the invocation of grant clbk");
  test_case_add(10,
                glsv_it_res_lck_async_20,
                "Lock id value obtained is valid before the invocation of grant clbk");

  test_suite_add(11, "saLckResourceUnlock Test Suite");
  test_case_add(11,
                glsv_it_res_unlck_01,
                "saLckResourceUnlock with invalid lock id parameter");
  test_case_add(11,
                glsv_it_res_unlck_02,
                "saLckResourceUnlock with lock id associated with resource hdl that is closed");
  test_case_add(11,
                glsv_it_res_unlck_03,
                "saLckResourceUnlock after finalizing the lock handle");
  test_case_add(11,
                glsv_it_res_unlck_04,
                "saLckResourceUnlock with small timeout value");
  test_case_add(11,
                glsv_it_res_unlck_05,
                "Unlock a lock");
  test_case_add(11,
                glsv_it_res_unlck_06,
                "Unlock a pending lock request");
  test_case_add(11,
                glsv_it_res_unlck_07,
                "saLckResourceUnlock with a lock id that is already unlocked");
  test_case_add(11,
                glsv_it_res_unlck_08,
                "Unlock the lock id before the invocation of grant clbk (sync case)");

  test_suite_add(12, "saLckResourceUnlockAsync Test Suite");
  test_case_add(12,
                glsv_it_res_unlck_async_01,
                "saLckResourceUnlockAsync with invalid lock id");
  test_case_add(12,
                glsv_it_res_unlck_async_02,
                "saLckResourceUnlockAsync with lock id associated with rsc hdl that is closed");
  test_case_add(12,
                glsv_it_res_unlck_async_03,
                "saLckResourceUnlockAsync after finalizing the lock handle");
  test_case_add(12,
                glsv_it_res_unlck_async_04,
                "saLckResourceUnlockAsync without registering unlock callback");
  test_case_add(12,
                glsv_it_res_unlck_async_05,
                "Unlock a lock");
  test_case_add(12,
                glsv_it_res_unlck_async_06,
                "Unlock a pending lock request");
  test_case_add(12,
                glsv_it_res_unlck_async_07,
                "saLckResourceUnlockAsync with a lock id that is already unlocked");
  test_case_add(12,
                glsv_it_res_unlck_async_08,
                "Unlock the lock id before the invocation of grant clbk (async case)");
  test_case_add(12,
                glsv_it_res_unlck_async_09,
                "Unlocking the lock id before the invocation of unlock callback");
  test_case_add(12,
                glsv_it_res_unlck_async_10,
                "Closing the lock resource before the invocation of unlock callback");

  test_suite_add(13, "saLckLockPurge Test Suite");
  test_case_add(13,
                glsv_it_lck_purge_01,
                "saLckLockPurge with invalid lock resource handle");
  test_case_add(13,
                glsv_it_lck_purge_02,
                "saLckLockPurge with closed resource handle");
  test_case_add(13,
                glsv_it_lck_purge_03,
                "saLckLockPurge with rsc hdl associated with lock handle that is finalized");
  test_case_add(13,
                glsv_it_lck_purge_04,
                "saLckLockPurge when there are no orphan locks on the resource");
  test_case_add(13,
                glsv_it_lck_purge_05,
                "Purge the orphan locks on the resource");

  test_suite_add(14, "Creation and Deletion of Resources Test Suite");
  test_case_add(14,
                glsv_it_res_cr_del_01,
                "Creation of multiple resources by same application");
  test_case_add(14,
                glsv_it_res_cr_del_02,
                "Creation of multiple resources by same application");
  test_case_add(14,
                glsv_it_res_cr_del_03,
                "saLckResourceClose will close all the resource handles given by Lock Service");
  test_case_add(14,
                glsv_it_res_cr_del_04,
                "Resource hdl obtained from the open clbk is valid only when error = SA_AIS_OK");
  test_case_add(14,
                glsv_it_res_cr_del_05,
                "Resource open clbk is inovoked when saLckResourceOpenAsync returns SA_AIS_OK");
  test_case_add(14,
                glsv_it_res_cr_del_06,
                "Closing a res hdl does not effect locks on other res hdl of "
                "the same resource in same appl");
  test_case_add(14,
                glsv_it_res_cr_del_07,
                "Closing a res hdl does not effect locks on other res hdl of "
                "the same resource in same appl (async)");

  test_suite_add(15, "Lock Modes and Lock Waiter Callback Test Suite");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_01,
                "Acquire multiple PR locks on a resource (async)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_02,
                "Acquire multiple PR locks on a resource (sync)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_03,
                "Request two EX locks on a resource from two different applications (async)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_04,
                "Request two EX locks on a resource from two different applications (sync)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_05,
                "Request two EX locks on a resource from same app but different res hdl (async)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_06,
                "Request two EX locks on a resource from same app but different res hdl (sync)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_07,
                "Waiter callback is invoked when a lock request is blocked by "
                "a lock held on that resource (async)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_08,
                "Waiter callback is invoked when a lock request is blocked by "
                "a lock held on that resource (sync)");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_09,
                "Waiter signal in the waiter clbk is same as that in the blocked lock request");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_10,
                "No of waiter callback invoked = No of locks blocking the lock request");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_11,
                "No of waiter callback invoked = No of locks requests blocked");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_12,
                "Lock Grant clbk is inovoked when saLckResourceLockAsync returns SA_AIS_OK");
  test_case_add(15,
                glsv_it_lck_modes_wt_clbk_13,
                "Request an EX lock on a resource on which an EX lock is held "
                "using diff res hdl and same application");

  test_suite_add(16, "Lock Types, Deadlocks and Orphan Locks");
  test_case_add(16,
                glsv_it_ddlcks_orplks_01,
                "Finalizing the app does not delete the resource if there are orphan locks on it");
  test_case_add(16,
                glsv_it_ddlcks_orplks_02,
                "SA_LCK_LOCK_ORPHANED status is returned when a lock request "
                "is blocked by an orphan lock (async)");
  test_case_add(16,
                glsv_it_ddlcks_orplks_03,
                "SA_LCK_LOCK_ORPHANED status is returned when a lock request "
                "is blocked by an orphan lock (sync)");
  test_case_add(16,
                glsv_it_ddlcks_orplks_04,
                "SA_LCK_LOCK_ORPHANED status is not returned when a lock request "
                "requested without SA_LCK_LOCK_ORPHAN lock flag is blocked by an orphan lock (async)");
  test_case_add(16,
                glsv_it_ddlcks_orplks_05,
                "SA_LCK_LOCK_ORPHANED status is not returned when a lock request "
                "requested without SA_LCK_LOCK_ORPHAN lock flag is blocked by an orphan lock (sync)");
  test_case_add(16,
                glsv_it_ddlcks_orplks_06,
                "Deadlock scenario with two resourcess and single process (async)");
  test_case_add(16,
                glsv_it_ddlcks_orplks_07,
                "Deadlock scenario with two resourcess and single process (sync)");
  test_case_add(16,
                glsv_it_ddlcks_orplks_08,
                "SA_LCK_LOCK_DEADLOCK is returned if a PR lock is requested on a "
                "resource on which EX lock is held with different res hdl");
  test_case_add(16, glsv_it_ddlcks_orplks_09, "Pending orphan lock scenario");
  test_case_add(16,
                glsv_it_ddlcks_orplks_10,
                "SA_LCK_LOCK_NOT_QUEUED status is returned when a lock request "
                "with SA_LCK_LOCK_NO_QUEUE flag is blocked by an orphan lock (async)");
  test_case_add(16,
                glsv_it_ddlcks_orplks_11,
                "SA_LCK_LOCK_NOT_QUEUED status is returned when a lock request "
                "with SA_LCK_LOCK_NO_QUEUE flag is blocked by an orphan lock (sync)");

  test_suite_add(17, "Lock Stripping and Purging");
  test_case_add(17,
                glsv_it_lck_strip_purge_01,
                "saLckLockPurge will purge all the orphan locks on a resource");
  test_case_add(17,
                glsv_it_lck_strip_purge_02,
                "Orphan locks are not stripped by saLckResourceClose");
  test_case_add(17,
                glsv_it_lck_strip_purge_03,
                "saLckLockPurge does not effect the other shared locks on the same resource");
}
