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

#include "imm_dumper.hh"
#include <errno.h>
#include <signal.h>
//#include <fcntl.h>
#include <assert.h>
#include <saImmOi.h>
#include "immutil.h"
#include <poll.h>
#include <stdio.h>
#include <cstdlib>


#define FD_IMM_PBE_OI 0
#define FD_IMM_PBE_OM 1

const unsigned int sleep_delay_ms = 500;
const unsigned int max_waiting_time_ms = 5 * 1000;	/* 5 secs */
/*static const SaImmClassNameT immServiceClassName = "SaImmMngt";*/

static SaImmOiHandleT pbeOiHandle;
static SaSelectionObjectT immOiSelectionObject;
static SaSelectionObjectT immOmSelectionObject;
static int category_mask;

static struct pollfd fds[2];
static nfds_t nfds = 2;

static void* sDbHandle=NULL;
static ClassMap* sClassIdMap=NULL;
static unsigned int sObjCount=0;

static void saImmOiAdminOperationCallback(SaImmOiHandleT immOiHandle,
					  SaInvocationT invocation,
					  const SaNameT *objectName,
					  SaImmAdminOperationIdT opId, const SaImmAdminOperationParamsT_2 **params)
{
	const SaImmAdminOperationParamsT_2 *param = params[0];
	if(param) {TRACE("bogus trace"); goto done;}

	TRACE_ENTER();

	if (opId == 4711) {
		/*
		(void)immutil_update_one_rattr(immOiHandle, (char *)objectName->value,
					       "saLogStreamSeverityFilter", SA_IMM_ATTR_SAUINT32T,
					       &stream->severityFilter);
		*/

		(void)immutil_saImmOiAdminOperationResult(immOiHandle, invocation, SA_AIS_OK);
	} else {
		LOG_ER("Invalid operation ID, should be %d ", 4711);
		(void)immutil_saImmOiAdminOperationResult(immOiHandle, invocation, SA_AIS_ERR_INVALID_PARAM);
	}
 done:
	TRACE_LEAVE();
}

static SaAisErrorT saImmOiCcbObjectModifyCallback(SaImmOiHandleT immOiHandle,
						  SaImmOiCcbIdT ccbId,
						  const SaNameT *objectName, const SaImmAttrModificationT_2 **attrMods)
{
	SaAisErrorT rc = SA_AIS_OK;
	struct CcbUtilCcbData *ccbUtilCcbData;

	TRACE_ENTER2("Modify callback for CCB:%llu object:%s", ccbId, objectName->value);
	if ((ccbUtilCcbData = ccbutil_findCcbData(ccbId)) == NULL) {
		if ((ccbUtilCcbData = ccbutil_getCcbData(ccbId)) == NULL) {
			LOG_ER("Failed to get CCB objectfor %llu", ccbId);
			rc = SA_AIS_ERR_NO_MEMORY;
			goto done;
		}
	}

	if(strncmp((char *) objectName->value, (char *) OPENSAF_IMM_OBJECT_DN, objectName->length) ==0) {
		LOG_WA("Pbe will not allow modifications to object %s", (char *) OPENSAF_IMM_OBJECT_DN);
		rc = SA_AIS_ERR_BAD_OPERATION;
		/* We will actually get invoked twice on this object, both as normal implementer and as PBE
		   the response on the modify upcall from PBE is discarded, but not for the regular implemener.
		 */
	} else {
		/* "memorize the modification request" */
		ccbutil_ccbAddModifyOperation(ccbUtilCcbData, objectName, attrMods);
	}
 done:
	TRACE_LEAVE();
	return rc;
}

static SaAisErrorT saImmOiCcbCompletedCallback(SaImmOiHandleT immOiHandle, SaImmOiCcbIdT ccbId)
{
	/* The completed callback is actually PREPARE AND COMMIT of the transaction.
	   The PBE receives the completed callback only AFTER all other (regular) 
	   implementers have replied on completed AND ONLY IF all of them replied
	   with SA_AIS_OK.

	   In this upcall the PBE has taken over sole responsibility of deciding on
	   commit/abort for this ccb/transaction. If we successfully commit towards
	   disk, that is pbeCommitTrans returns SA_AIS_OK, then this callback returns OK 
	   and the rest of the immsv is obligated to commit. Returning Ok from this
	   upcall is then very special and precious.

	   If we unabiguously fail to commit, that is if pbeCommitTrans returns 
	   not SA_AIS_OK, or we fail before that, then this upcall will return with failure
	   and the rest of the imsmv is obliged NOT to commit. 
	   The failure case is the same as for any implementer, returning not OK means 
	   this implemener vetoes the transaction.

	   There is a third case that is non standard. 
	   If this upcall does not reply (i.e. the PBE crashes or gets stuck in an endless 
	   loop or gets blocked on disk i/o, or...) then the rest of the immsv is obliged
	   to wait until the outcome is resolved. The outcome of a "lost" ccb/transaction
	   can be resolved in two ways. 

	   The first way is for the IMMSv to restart the PBE with argument -restart.
	   The restarted PBE will then try to reconnect to the existing pbe file.
	   If successfull, a recovery protocol can be executed to determine the 
	   outcome of any ccb-id.

	   The second way is for the IMMSv to overrule the PBE by discardign the entire
	   pbe file rep. This is done by just restarting the pbe in a normal way which
	   will cause it to dump the current contents of the immsv, ignoring any previous
	   disk rep. Discarding ccbs that could have been commited to disk may seem wrong,
	   but as long as no client of the imsmv has seen the result of the ccb as commited,
	   there is no problem. But care should be taken to discard the old disk rep so
	   that it is not re-used later to see an alternative history.
	 */

	SaAisErrorT rc = SA_AIS_OK;
	struct CcbUtilCcbData *ccbUtilCcbData;
	struct CcbUtilOperationData *ccbUtilOperationData;
	std::string objName;
	TRACE_ENTER2("Completed callback for CCB:%llu", ccbId);

	if ((ccbUtilCcbData = ccbutil_findCcbData(ccbId)) == NULL) {
		LOG_ER("Failed to find CCB object for %llu", ccbId);
		rc = SA_AIS_ERR_BAD_OPERATION;
		goto done;
	}

	if((rc =  pbeBeginTrans(sDbHandle)) != SA_AIS_OK)
	{ goto done;}

	TRACE("Begin PBE transaction for CCB %llu OK", ccbId);

	ccbUtilOperationData = ccbUtilCcbData->operationListHead;
	while (ccbUtilOperationData != NULL && rc == SA_AIS_OK) {
		const SaImmAttrModificationT_2 **attrMods = NULL;
		const SaImmAttrModificationT_2 *attMod = NULL;
		int ix=0;
						
		switch (ccbUtilOperationData->operationType) {
			case CCBUTIL_CREATE:
				do {
					TRACE("Create of object with DN: %s",
						ccbUtilOperationData->objectName.value);
					objectToPBE(std::string((const char *) ccbUtilOperationData->objectName.value), 
						ccbUtilOperationData->param.create.attrValues,
						sClassIdMap, sDbHandle, ++sObjCount,
						ccbUtilOperationData->param.create.className);
				} while (0);
				break;

			case CCBUTIL_DELETE:
				TRACE("Delete of object with DN: %s",
					ccbUtilOperationData->param.deleteOp.objectName->value);
				objectDeleteToPBE(std::string((const char *) 
					ccbUtilOperationData->param.deleteOp.objectName->value),
					sDbHandle);
					
				break;

			
			case CCBUTIL_MODIFY:
				/* Note: it is impprtant that the code in this MODIFY case follow
				   the same logic as performed by ImmModel::ccbObjectModify()
				   We DO NOT want the PBE repository to diverge from the main memory
				   represenation of the immsv data. 
				   This is not the only way to solve this. In fact the current solution is
				   very unoptimal since it generates possibly several sql commands for what
				   could be one. The advantag with the current solution is that it follows
				   the logic of the ImmModel and can do so using the data provided by the
				   unmodified ccbObjectModify upcall.
				 */
				TRACE("Modify of object with DN: %s",
					ccbUtilOperationData->param.modify.objectName->value);
				attrMods = ccbUtilOperationData->param.modify.attrMods;
				while((attMod = attrMods[ix++]) != NULL) {
					objName.append((const char *) ccbUtilOperationData->
						param.modify.objectName->value);
					switch(attMod->modType) {
						case SA_IMM_ATTR_VALUES_REPLACE:
							objectModifyDiscardAllValuesOfAttrToPBE(sDbHandle, objName,
								&(attMod->modAttr));

							if(attMod->modAttr.attrValuesNumber == 0) {
								continue; //Ok to replace with nothing
							}
							//else intentional fall through

						case SA_IMM_ATTR_VALUES_ADD:
							if(attMod->modAttr.attrValuesNumber == 0) {
								LOG_ER("Empty value used for adding to attribute %s",
									attMod->modAttr.attrName);
								rc = SA_AIS_ERR_BAD_OPERATION;
								goto done;
							}
							
							objectModifyAddValuesOfAttrToPBE(sDbHandle, objName,
								&(attMod->modAttr));
							
							break;

						case SA_IMM_ATTR_VALUES_DELETE:
							if(attMod->modAttr.attrValuesNumber == 0) {
								LOG_ER("Empty value used for deleting from attribute %s",
									attMod->modAttr.attrName);
								rc = SA_AIS_ERR_BAD_OPERATION;
								goto done;
							}
							objectModifyDiscardMatchingValuesOfAttrToPBE(sDbHandle, objName,
								&(attMod->modAttr));

							break;
						default: assert(0);
					}
				}
				break;

			default: assert(0);

		}
		ccbUtilOperationData = ccbUtilOperationData->next;
	}

	rc =  pbeCommitTrans(sDbHandle);
	if(rc == SA_AIS_OK) {
		TRACE("COMMIT PBE TRANSACTION for ccb %llu OK", ccbId);
		/* Use ccbUtilCcbData->userData to record ccb outcome, verify in ccbApply/ccbAbort */
	} else {
		TRACE("COMMIT TRANSACTION %llu FAILED rc:%u", ccbId, rc);
	}

	/* Fault injection.
	if(ccbId == 3) { exit(1);} 
	*/
       
 done:
	TRACE_LEAVE();
	return rc;
}

static void saImmOiCcbApplyCallback(SaImmOiHandleT immOiHandle, SaImmOiCcbIdT ccbId)
{
	struct CcbUtilCcbData *ccbUtilCcbData;
	struct CcbUtilOperationData *ccbUtilOperationData;
	//const SaImmAttrModificationT_2 *attrMod;

	TRACE_ENTER2("PBE APPLY CALLBACK cleanup CCB:%llu", ccbId);

	if ((ccbUtilCcbData = ccbutil_findCcbData(ccbId)) == NULL) {
		LOG_ER("Failed to find CCB object for %llu", ccbId);
		goto done;
	}

	ccbUtilOperationData = ccbUtilCcbData->operationListHead;

	/* Verify ok outcome with ccbUtilCcbData->userData */

	ccbutil_deleteCcbData(ccbUtilCcbData);
 done:
	TRACE_LEAVE();
}

static void saImmOiCcbAbortCallback(SaImmOiHandleT immOiHandle, SaImmOiCcbIdT ccbId)
{
	struct CcbUtilCcbData *ccbUtilCcbData;

	TRACE_ENTER2("ABORT callback. Cleanup CCB %llu", ccbId);

	if ((ccbUtilCcbData = ccbutil_findCcbData(ccbId)) != NULL)
		/* Verify nok outcome with ccbUtilCcbData->userData */
		ccbutil_deleteCcbData(ccbUtilCcbData);
	else
		LOG_ER("Failed to find CCB object for %llu", ccbId);

	TRACE_LEAVE();
}

static SaAisErrorT saImmOiCcbObjectCreateCallback(SaImmOiHandleT immOiHandle, SaImmOiCcbIdT ccbId,
	const SaImmClassNameT className, const SaNameT *parentName, const SaImmAttrValuesT_2 **attr)
{
	SaAisErrorT rc = SA_AIS_OK;
	struct CcbUtilCcbData *ccbUtilCcbData;
	bool rdnFound=false;
	const SaImmAttrValuesT_2 *attrValue;
	int i = 0;
	std::string classNameString(className);
	struct CcbUtilOperationData  *operation = NULL;
	ClassInfo* classInfo = (*sClassIdMap)[classNameString];
	if(parentName && parentName->length) {
		TRACE_ENTER2("CREATE CALLBACK CCB:%llu class:%s parent:%s", ccbId, className, parentName->value);
	} else {
		TRACE_ENTER2("CREATE CALLBACK CCB:%llu class:%s ROOT OBJECT (no parent)", ccbId, className);
	}
	if(!classInfo) {
		LOG_ER("Class '%s' not found in classIdMap", className);
		rc = SA_AIS_ERR_BAD_OPERATION;
		goto done;
	}

	if ((ccbUtilCcbData = ccbutil_findCcbData(ccbId)) == NULL) {
		if ((ccbUtilCcbData = ccbutil_getCcbData(ccbId)) == NULL) {
			LOG_ER("Failed to get CCB objectfor %llu", ccbId);
			rc = SA_AIS_ERR_NO_MEMORY;
			goto done;
		}
	}

	if(strncmp((char *) className, (char *) OPENSAF_IMM_CLASS_NAME, strlen(className)) == 0) {
		LOG_WA("Pbe will not allow creates of instances of class %s", (char *) OPENSAF_IMM_CLASS_NAME);
		rc = SA_AIS_ERR_BAD_OPERATION;
		/* We will actually get invoked twice on this create, both as normal implementer and as PBE
		   the response on the create upcall from PBE is discarded, but not for the regular implementer UC.
		 */
		goto done;
	} 

	/* "memorize the creation request" */

	operation = ccbutil_ccbAddCreateOperation(ccbUtilCcbData, className, parentName, attr);
	TRACE("ABT after create add operation:%p", operation);
	assert(operation);
	/* Find the RDN attribute, construct the object DN and store it with the object. */
	while((attrValue = attr[i++]) != NULL) {
		std::string attName(attrValue->attrName);
		TRACE("ABT2 attrName:%s", attName.c_str());
		SaImmAttrFlagsT attrFlags = classInfo->mAttrMap[attName];
		TRACE("ABT3 rdn?:%llu", (attrFlags & SA_IMM_ATTR_RDN));
		if(attrFlags & SA_IMM_ATTR_RDN) {
			rdnFound = true;
			if(attrValue->attrValueType == SA_IMM_ATTR_SASTRINGT) {
				TRACE("ABT4 RDN is SaStringT");
				SaStringT rdnVal = *((SaStringT *) attrValue->attrValues[0]);
				TRACE("ABT5 RDN is %s", rdnVal);
				if((parentName==NULL) || (parentName->length == 0)) {
					operation->objectName.length =
						sprintf((char *) operation->objectName.value,
							"%s", rdnVal);
					TRACE("ABT6 EMPTY PARENT %s", operation->objectName.value);

				} else {
				TRACE("ABT66 NOT EMPTY PARENT !!!!!!");
					operation->objectName.length = 
						sprintf((char *) operation->objectName.value,
							"%s,%s", rdnVal, parentName->value);
				}
			} else if(attrValue->attrValueType == SA_IMM_ATTR_SANAMET) {
				TRACE("ABT7 RDN is SaNameT");
				SaNameT *rdnVal = ((SaNameT *) attrValue->attrValues[0]);
				if((parentName==NULL) || (parentName->length == 0)) {
					operation->objectName.length =
						sprintf((char *) operation->objectName.value,
							"%s", rdnVal->value);
				} else {
					operation->objectName.length = 
						sprintf((char *) operation->objectName.value,
							"%s,%s", rdnVal->value, parentName->value);
				}
			} else {
				LOG_ER("Rdn attribute %s for class '%s' is neither SaStringT nor SaNameT!", 
					attrValue->attrName, className);
				rc = SA_AIS_ERR_BAD_OPERATION;
				goto done;
			}
			TRACE("Extracted DN: %s(%u)", operation->objectName.value,  operation->objectName.length);
		}
	}

	if(!rdnFound) {
		LOG_ER("Could not find Rdn attribute for class '%s'!", 	className);
		rc = SA_AIS_ERR_BAD_OPERATION;
		goto done;
	}

	assert(operation->objectName.length > 0);

 done:
	TRACE_LEAVE();
	return rc;
}

static SaAisErrorT saImmOiCcbObjectDeleteCallback(SaImmOiHandleT immOiHandle, SaImmOiCcbIdT ccbId,
	const SaNameT *objectName)
{
	SaAisErrorT rc = SA_AIS_OK;
	struct CcbUtilCcbData *ccbUtilCcbData;
	TRACE_ENTER2("DELETE CALLBACK CCB:%llu object:%s", ccbId, objectName->value);

	if ((ccbUtilCcbData = ccbutil_findCcbData(ccbId)) == NULL) {
		if ((ccbUtilCcbData = ccbutil_getCcbData(ccbId)) == NULL) {
			LOG_ER("Failed to get CCB objectfor %llu", ccbId);
			rc = SA_AIS_ERR_NO_MEMORY;
			goto done;
		}
	}

	if(strncmp((char *) objectName->value, (char *) OPENSAF_IMM_OBJECT_DN, objectName->length) ==0) {
		LOG_WA("Pbe will not allow delete of object %s", (char *) OPENSAF_IMM_OBJECT_DN);
		rc = SA_AIS_ERR_BAD_OPERATION;
		/* We will actually get invoked twice on this object, both as normal implementer and as PBE
		   the response on the delete upcall from PBE is discarded, but not for the regular implemener.
		 */
	} else {
		/* "memorize the deletion request" */
		ccbutil_ccbAddDeleteOperation(ccbUtilCcbData, objectName);
	}

 done:
	TRACE_LEAVE();
	return rc;
}
/**
 * IMM requests us to update a non cached runtime attribute.
 * @param immOiHandle
 * @param objectName
 * @param attributeNames
 * 
 * @return SaAisErrorT
 */
static SaAisErrorT saImmOiRtAttrUpdateCallback(SaImmOiHandleT immOiHandle,
					       const SaNameT *objectName, const SaImmAttrNameT *attributeNames)
{
	SaAisErrorT rc = SA_AIS_OK;
	//SaImmAttrNameT attributeName;

	TRACE_ENTER2("RT ATTR UPDATE CALLBACK");

	TRACE_LEAVE();
	return rc;
}

static const SaImmOiCallbacksT_2 callbacks = {
	saImmOiAdminOperationCallback,
	saImmOiCcbAbortCallback,
	saImmOiCcbApplyCallback,
	saImmOiCcbCompletedCallback,
	saImmOiCcbObjectCreateCallback,
	saImmOiCcbObjectDeleteCallback,
	saImmOiCcbObjectModifyCallback,
	saImmOiRtAttrUpdateCallback
};

/**
 * Ssignal handler to dump information from all data structures
 * @param sig
 */
static void dump_sig_handler(int sig)
{
	/*
	int old_category_mask = category_mask;

	if (trace_category_set(CATEGORY_ALL) == -1)
		printf("trace_category_set failed");

	if (trace_category_set(old_category_mask) == -1)
		printf("trace_category_set failed");
	*/
}

/**
 * USR2 signal handler to enable/disable trace (toggle)
 * @param sig
 */
static void sigusr2_handler(int sig)
{
	if (category_mask == 0) {
		category_mask = CATEGORY_ALL;
		printf("Enabling traces");
		dump_sig_handler(sig); /* Do a dump each time we toggle on.*/
	} else {
		category_mask = 0;
		printf("Disabling traces");
	}

	if (trace_category_set(category_mask) == -1)
		printf("trace_category_set failed");
}

SaAisErrorT pbe_daemon_imm_init(SaImmHandleT immHandle)
{
	SaAisErrorT rc;
	unsigned int msecs_waited = 0;
	SaVersionT  immVersion;
	TRACE_ENTER();

	immVersion.releaseCode = RELEASE_CODE;
	immVersion.majorVersion = MAJOR_VERSION;
	immVersion.minorVersion = MINOR_VERSION;

	rc = saImmOiInitialize_2(&pbeOiHandle, &callbacks, &immVersion);
	while ((rc == SA_AIS_ERR_TRY_AGAIN) && (msecs_waited < max_waiting_time_ms)) {
		usleep(sleep_delay_ms * 1000);
		msecs_waited += sleep_delay_ms;
		rc = saImmOiInitialize_2(&pbeOiHandle, &callbacks, &immVersion);
	}
	if (rc != SA_AIS_OK) {
		LOG_ER("saImmOiInitialize_2 failed %u", rc);
		return rc;
	}

	rc = saImmOiImplementerSet(pbeOiHandle, (char *) OPENSAF_IMM_PBE_IMPL_NAME);
	while ((rc == SA_AIS_ERR_TRY_AGAIN || rc == SA_AIS_ERR_EXIST) && 
		(msecs_waited < max_waiting_time_ms)) {
		usleep(sleep_delay_ms * 1000);
		msecs_waited += sleep_delay_ms;
		rc = saImmOiImplementerSet(pbeOiHandle, (char *) OPENSAF_IMM_PBE_IMPL_NAME);
	}
	if (rc != SA_AIS_OK) {
		LOG_ER("saImmOiImplementerSet for %s failed %u", OPENSAF_IMM_PBE_IMPL_NAME, rc);
		return rc;
	}
	/* Second implementer: OPENSAF_IMM_SERVICE_IMPL_NAME */

	rc = saImmOiClassImplementerSet(pbeOiHandle, (char *) OPENSAF_IMM_CLASS_NAME);
	while ((rc == SA_AIS_ERR_TRY_AGAIN) && (msecs_waited < max_waiting_time_ms)) {
		usleep(sleep_delay_ms * 1000);
		msecs_waited += sleep_delay_ms;	
		rc = saImmOiClassImplementerSet(pbeOiHandle, (char *) OPENSAF_IMM_CLASS_NAME);
	}

	rc = saImmOiSelectionObjectGet(pbeOiHandle, &immOiSelectionObject);
	/* SelectionObjectGet is library local, no need for retry loop */
	if (rc != SA_AIS_OK) {
		LOG_ER("saImmOiSelectionObjectGet failed %u", rc);
		return rc;
	}

	rc = saImmOmSelectionObjectGet(immHandle, &immOmSelectionObject);
	/* SelectionObjectGet is library local, no need for retry loop */
	if (rc != SA_AIS_OK) {
		LOG_ER("saImmOmSelectionObjectGet failed %u", rc);
		return rc;
	}

	TRACE_LEAVE();

	return rc;
}


void pbeDaemon(SaImmHandleT immHandle, void* dbHandle, ClassMap* classIdMap,
	unsigned int objCount)
{

	SaAisErrorT error = SA_AIS_OK;

	/* cbHandle, classIdMap and objCount are needed by the PBE OI upcalls.
	   Make them available by assigning to module local static variables. 
	 */
	sDbHandle = dbHandle;
	sClassIdMap = classIdMap;
	sObjCount = objCount;

	if (pbe_daemon_imm_init(immHandle) != SA_AIS_OK) {
		exit(1);
	}

	if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR) {
		LOG_ER("signal USR2 failed: %s", strerror(errno));
		exit(1);
	}
	/* Set up all file descriptors to listen to */
	fds[FD_IMM_PBE_OI].fd = immOiSelectionObject;
	fds[FD_IMM_PBE_OI].events = POLLIN;
	fds[FD_IMM_PBE_OM].fd = immOmSelectionObject;
	fds[FD_IMM_PBE_OM].events = POLLIN;

        while(1) {
		TRACE("PBE Daemon entering poll");
		int ret = poll(fds, nfds, -1);
		TRACE("PBE Daemon returned from poll ret: %d", ret);
		if (ret == -1) {
			if (errno == EINTR)
				continue;

			LOG_ER("poll failed - %s", strerror(errno));
			break;
		}

		if (pbeOiHandle && fds[FD_IMM_PBE_OI].revents & POLLIN) {
			error = saImmOiDispatch(pbeOiHandle, SA_DISPATCH_ALL);

			if (error == SA_AIS_ERR_BAD_HANDLE) {
				TRACE("saImmOiDispatch returned BAD_HANDLE");
				pbeOiHandle = 0;
				break;
			}
		}
		/* Attch as OI for 
		   SaImmMngt: safRdn=immManagement,safApp=safImmService
		   ?? OpensafImm: opensafImm=opensafImm,safApp=safImmService ??
		*/

		if (immHandle && fds[FD_IMM_PBE_OM].revents & POLLIN) {
			error = saImmOmDispatch(immHandle, SA_DISPATCH_ALL);
			if (error != SA_AIS_OK) {
				LOG_WA("saImmOmDispatch returned %u PBE lost contact with IMMND - exiting", error);
				pbeOiHandle = 0;
				immHandle = 0;
				break;
			}
		}

		
	}

	LOG_IN("IMM PBE process EXITING...");
	TRACE_LEAVE();
	exit(1);
}


