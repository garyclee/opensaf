#include "plm/apitest/plmtest.h"
#include <saAis.h>
#include <saNtf.h>
#include <saPlm.h>

void saPlmDispatch_01(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = saPlmDispatch(plmHandle, SA_DISPATCH_ALL);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmDispatch_02(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = saPlmDispatch(plmHandle, SA_DISPATCH_ONE);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmDispatch_03(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = saPlmDispatch(plmHandle, SA_DISPATCH_BLOCKING);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmDispatch_04(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = saPlmDispatch(-1, SA_DISPATCH_ONE);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmDispatch_05(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = saPlmDispatch(plmHandle, -1);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}
