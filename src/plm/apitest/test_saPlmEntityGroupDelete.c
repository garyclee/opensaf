#include "plm/apitest/plmtest.h"

void saPlmEntityGroupDelete_01(void)
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
	safassert(plmEntityGroupRemove(entityGroupHandle, &f120_slot_1_dn,
					 entityNamesNumber),
		  SA_AIS_OK);
	rc = plmEntityGroupDelete(entityGroupHandle);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupDelete_02(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = plmEntityGroupDelete(entityGroupHandle);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmEntityGroupDelete_03(void)
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
	safassert(plmEntityGroupRemove(entityGroupHandle, &f120_slot_1_dn,
					 entityNamesNumber),
		  SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
	rc = plmEntityGroupDelete(entityGroupHandle);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
}
