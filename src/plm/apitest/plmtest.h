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

#ifndef PLM_APITEST_PLMTEST_H_
#define PLM_APITEST_PLMTEST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "osaf/apitest/utest.h"
#include "osaf/apitest/util.h"
#include <string.h>
#include <saNtf.h>
#include <saPlm.h>
#include <saAis.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Common globals */
extern SaVersionT PlmVersion;
extern SaAisErrorT rc;
extern SaSelectionObjectT selectionObject;

/* Management globals */
extern SaPlmHandleT plmHandle;
extern SaPlmCallbacksT plmCallbacks;
extern SaPlmEntityGroupHandleT entityGroupHandle;

extern const SaNameT f120_slot_1_dn, f120_slot_16_dn, amc_slot_1_dn,
    amc_slot_16_dn, f120_slot_1_eedn, f120_nonexistent, f120_slot_16_eedn,
    amc_slot_1_eedn, amc_slot_16_eedn;
extern int entityNamesNumber;

extern void TrackCallbackT(
    SaPlmEntityGroupHandleT entityGroupHandle, SaUint64T trackCookie,
    SaInvocationT invocation, SaPlmTrackCauseT cause,
    const SaNameT *rootCauseEntity, SaNtfIdentifierT rootCorrelationId,
    const SaPlmReadinessTrackedEntitiesT *trackedEntities,
    SaPlmChangeStepT step, SaAisErrorT error);

SaAisErrorT plmInitialize(SaPlmHandleT *, const SaPlmCallbacksT *, SaVersionT *);
SaAisErrorT plmFinalize(SaPlmHandleT);
SaAisErrorT plmEntityGroupCreate(SaPlmHandleT, SaPlmEntityGroupHandleT *);
SaAisErrorT plmEntityGroupAdd(SaPlmEntityGroupHandleT,
                              const SaNameT *entityNames,
                              SaUint32T entityNamesNumber,
                              SaPlmGroupOptionsT);
SaAisErrorT plmEntityGroupRemove(SaPlmEntityGroupHandleT,
                                 const SaNameT *entityNames,
                                 SaUint32T entityNamesNumber);
SaAisErrorT plmEntityGroupDelete(SaPlmEntityGroupHandleT);
SaAisErrorT plmReadinessTrack(SaPlmEntityGroupHandleT,
                              SaUint8T trackFlags,
                              SaUint64T trackCookie,
                              SaPlmReadinessTrackedEntitiesT *);
SaAisErrorT plmReadinessTrackStop(SaPlmEntityGroupHandleT);
SaAisErrorT plmReadinessTrackResponse(SaPlmEntityGroupHandleT,
                                      SaInvocationT,
                                      SaPlmReadinessTrackResponseT);
SaAisErrorT plmEntityReadinessImpact(SaPlmHandleT,
                                     const SaNameT *impactedEntity,
                                     SaPlmReadinessImpactT ,
                                     SaNtfCorrelationIdsT *);

#ifdef __cplusplus
}
#endif

#endif  // PLM_APITEST_PLMTEST_H_
