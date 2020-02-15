#include "plm/apitest/plmtest.h"
#include <saAis.h>
#include <saNtf.h>
#include <saPlm.h>

void saPlmSelectionObjectGet_01(void)
{
	safassert(plmInitialize(&plmHandle, NULL, &PlmVersion), SA_AIS_OK);
	rc = saPlmSelectionObjectGet(plmHandle, &selectionObject);
	test_validate(rc, SA_AIS_OK);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmSelectionObjectGet_02(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = saPlmSelectionObjectGet(-1, &selectionObject);
	test_validate(rc, SA_AIS_ERR_BAD_HANDLE);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}

void saPlmSelectionObjectGet_03(void)
{
	SaPlmCallbacksT plms_cbks;
	plms_cbks.saPlmReadinessTrackCallback = &TrackCallbackT;
	safassert(plmInitialize(&plmHandle, &plms_cbks, &PlmVersion),
		  SA_AIS_OK);
	rc = saPlmSelectionObjectGet(plmHandle, NULL);
	test_validate(rc, SA_AIS_ERR_INVALID_PARAM);
	safassert(plmFinalize(plmHandle), SA_AIS_OK);
}
