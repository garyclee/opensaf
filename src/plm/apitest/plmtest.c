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

#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "plm/apitest/plmtest.h"

SaVersionT PlmVersion = {'A', 1, 3};
SaAisErrorT rc;
SaPlmHandleT plmHandle;
SaSelectionObjectT selectionObject;
SaPlmEntityGroupHandleT entityGroupHandle;

const SaNameT f120_slot_1_dn = {sizeof("safHE=f120_slot_1,safDomain=domain_1") -
				    1,
				"safHE=f120_slot_1,safDomain=domain_1"};
const SaNameT f120_slot_16_dn = {
    sizeof("safHE=f120_slot_16,safDomain=domain_1") - 1,
    "safHE=f120_slot_16,safDomain=domain_1"};
const SaNameT amc_slot_1_dn = {
    sizeof("safHE=pramc_slot_1,safHE=f120_slot_1,safDomain=domain_1") - 1,
    "safHE=pramc_slot_1,safHE=f120_slot_1,safDomain=domain_1"};
const SaNameT amc_slot_16_dn = {
    sizeof("safHE=pramc_slot_16,safHE=f120_slot_16,safDomain=domain_1") - 1,
    "safHE=pramc_slot_16,safHE=f120_slot_16,safDomain=domain_1"};
const SaNameT f120_slot_1_eedn = {
    sizeof(
	"safEE=Linux_os_hosting_clm_node,safHE=f120_slot_1,safDomain=domain_1") -
	1,
    "safEE=Linux_os_hosting_clm_node,safHE=f120_slot_1,safDomain=domain_1"};
const SaNameT f120_slot_16_eedn = {
    sizeof(
	"safEE=Linux_os_hosting_clm_node,safHE=f120_slot_16,safDomain=domain_1") -
	1,
    "safEE=Linux_os_hosting_clm_node,safHE=f120_slot_16,safDomain=domain_1"};
const SaNameT amc_slot_1_eedn = {
    sizeof(
	"safEE=Linux_os_hosting_clm_node,safHE=pramc_slot_1,safHE=f120_slot_1,safDomain=domain_1") -
	1,
    "safEE=Linux_os_hosting_clm_node,safHE=pramc_slot_1,safHE=f120_slot_1,safDomain=domain_1"};
const SaNameT amc_slot_16_eedn = {
    sizeof(
	"safEE=Linux_os_hosting_clm_node,safHE=pramc_slot_16,safHE=f120_slot_16,safDomain=domain_1") -
	1,
    "safEE=Linux_os_hosting_clm_node,safHE=pramc_slot_16,safHE=f120_slot_16,safDomain=domain_1"};
const SaNameT f120_nonexistent = {sizeof("safHE=f121_slot_1") - 1,
				  "safHE=f121_slot_1"};
int entityNamesNumber = 1;

SaAisErrorT plmInitialize(SaPlmHandleT *plmHandle,
                          const SaPlmCallbacksT *plmCallbacks,
                          SaVersionT *version)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmInitialize(plmHandle, plmCallbacks, version);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmInitialize returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmFinalize(SaPlmHandleT plmHandle)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmFinalize(plmHandle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmFinalize returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmEntityGroupCreate(SaPlmHandleT plmHandle,
                                 SaPlmEntityGroupHandleT *entityGroupHandle)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmEntityGroupCreate(plmHandle, entityGroupHandle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmEntityGroupCreate returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmEntityGroupAdd(SaPlmEntityGroupHandleT entityGroupHandle,
                              const SaNameT *entityNames,
                              SaUint32T entityNamesNumber,
                              SaPlmGroupOptionsT options)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmEntityGroupAdd(entityGroupHandle,
                             entityNames,
                             entityNamesNumber,
                             options);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmEntityGroupAdd returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmEntityGroupRemove(SaPlmEntityGroupHandleT entityGroupHandle,
                                 const SaNameT *entityNames,
                                 SaUint32T entityNamesNumber)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmEntityGroupRemove(entityGroupHandle,
                                entityNames,
                                entityNamesNumber);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmEntityGroupRemove returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmEntityGroupDelete(SaPlmEntityGroupHandleT entityGroupHandle)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmEntityGroupDelete(entityGroupHandle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmEntityGroupDelete returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmReadinessTrack(SaPlmEntityGroupHandleT entityGroupHandle,
                              SaUint8T trackFlags,
                              SaUint64T trackCookie,
                              SaPlmReadinessTrackedEntitiesT *trackedEntities)

{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmReadinessTrack(entityGroupHandle,
                             trackFlags,
                             trackCookie,
                             trackedEntities);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmReadinessTrack returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmReadinessTrackStop(SaPlmEntityGroupHandleT entityGroupHandle)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmReadinessTrackStop(entityGroupHandle);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmReadinessTrackStop returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmReadinessTrackResponse(SaPlmEntityGroupHandleT entityGroupHandle,
                                      SaInvocationT invocation,
                                      SaPlmReadinessTrackResponseT response)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmReadinessTrackResponse(entityGroupHandle, invocation, response);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmReadinessTrackResponse returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}

SaAisErrorT plmEntityReadinessImpact(SaPlmHandleT plmHandle,
                                     const SaNameT *impactedEntity,
                                     SaPlmReadinessImpactT impact,
                                     SaNtfCorrelationIdsT *correlationIds)
{
  SaAisErrorT rc = SA_AIS_OK;

  while (true) {
    rc = saPlmEntityReadinessImpact(plmHandle,
                                    impactedEntity,
                                    impact,
                                    correlationIds);

    if (rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_TIMEOUT) {
      printf("saPlmEntityReadinessImpact returned %i\n", rc);
      sleep(1);
    } else
      break;
  }

  return rc;
}


int main(int argc, char **argv)
{
	int suite = ALL_SUITES, tcase = ALL_TESTS;

	srandom(getpid());

	if (argc > 1) {
		suite = atoi(argv[1]);
	}

	if (argc > 2) {
		tcase = atoi(argv[2]);
	}

	if (suite == 0) {
		test_list();
		return 0;
	}

	return test_run(suite, tcase);
}
