#include "plm/apitest/plmtest.h"
#include <saAis.h>
#include <saNtf.h>
#include <saPlm.h>

void saPlmEntityGroupAdd_01(void)
{
	safassert(plmInitialize(&plmHandle, NULL, &PlmVersion), SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber, SA_PLM_GROUP_SINGLE_ENTITY);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_02(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber, SA_PLM_GROUP_SUBTREE);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_03(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber,
				 SA_PLM_GROUP_SUBTREE_HES_ONLY);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_04(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber,
				 SA_PLM_GROUP_SUBTREE_EES_ONLY);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_05(void)
{
	rc = plmEntityGroupAdd(entityGroupHandle,
				 &f120_slot_1_dn, entityNamesNumber,
				 SA_PLM_GROUP_SINGLE_ENTITY);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}

void saPlmEntityGroupAdd_06(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, 0, entityNamesNumber,
				 SA_PLM_GROUP_SUBTREE_EES_ONLY);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_07(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, 0, entityNamesNumber,
				 SA_PLM_GROUP_SUBTREE_HES_ONLY);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_08(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn, 0,
				 SA_PLM_GROUP_SUBTREE_EES_ONLY);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_09(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn, 0,
				 SA_PLM_GROUP_SUBTREE_HES_ONLY);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_10(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber, -1);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_11(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(-1, &f120_slot_1_dn, entityNamesNumber,
				 SA_PLM_GROUP_SINGLE_ENTITY);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_12(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	safassert(plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				      entityNamesNumber,
				      SA_PLM_GROUP_SINGLE_ENTITY),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber, SA_PLM_GROUP_SINGLE_ENTITY);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_13(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	/* Nonexistent DN*/
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_nonexistent,
				 entityNamesNumber, SA_PLM_GROUP_SINGLE_ENTITY);
	test_validate(rc, SA_AIS_ERR_NOT_EXIST);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_14(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_eedn,
				 entityNamesNumber,
				 SA_PLM_GROUP_SUBTREE_HES_ONLY);
	test_validate(rc, SA_AIS_OK);

	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_15(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber,
				 SA_PLM_GROUP_SUBTREE_EES_ONLY);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupAdd_16(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	safassert(plmEntityGroupCreate(plmHandle, &entityGroupHandle),
		  SA_AIS_OK);
	rc = plmEntityGroupAdd(entityGroupHandle, &f120_slot_1_dn,
				 entityNamesNumber, SA_PLM_GROUP_SUBTREE);
	rc = plmEntityGroupAdd(entityGroupHandle, &amc_slot_1_dn,
				 entityNamesNumber, SA_PLM_GROUP_SUBTREE);
	test_validate(rc, SA_AIS_ERR_EXIST);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}
