#include "plm/apitest/plmtest.h"
#include <saAis.h>
#include <saNtf.h>
#include <saPlm.h>

void saPlmFinalize_01(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = plmFinalize(plmHandle);
	test_validate(rc, SA_AIS_OK);
}

void saPlmFinalize_02(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = plmFinalize(-1);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}
