/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
 * (C) Copyright Ericsson AB 2010, 2017 - All Rights Reserved.
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
 * Author(s): Ericsson
 *
 */
#include "osaf/saflog/saflog.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <saAis.h>
#include "base/logtrace.h"
#include "base/saf_error.h"

static bool log_client_initialized = false;
static SaLogStreamHandleT logStreamHandle;
static SaLogHandleT logHandle;

void saflog_init(void)
{
	if (log_client_initialized == false) {
		SaVersionT logVersion = {'A', 2, 3};
		SaNameT stream_name;
		saAisNameLend(SA_LOG_STREAM_SYSTEM, &stream_name);

		SaAisErrorT rc = saLogInitialize(&logHandle, NULL, &logVersion);
		if (rc != SA_AIS_OK) {
			LOG_NO("saflogInit: saLogInitialize FAILED: %s",
			       saf_error(rc));
			return;
		}

		rc = saLogStreamOpen_2(logHandle, &stream_name, NULL, 0,
					  SA_TIME_ONE_SECOND, &logStreamHandle);
		if (rc != SA_AIS_OK) {
			LOG_NO("saflogInit: saLogStreamOpen_2 FAILED: %s",
			       saf_error(rc));
			if (saLogFinalize(logHandle) != SA_AIS_OK)
				LOG_NO("saflogInit: saLogFinalize FAILED");
			return;
		}
		log_client_initialized = true;
	}
}

void saflog(int priority, const SaNameT *logSvcUsrName, const char *format, ...)
{
	SaAisErrorT rc;
	SaLogRecordT logRecord;
	SaLogBufferT logBuffer;
	va_list ap;
	char str[SA_LOG_MAX_RECORD_SIZE + 1];
	/* SA_LOG_MAX_RECORD_SIZE = 65535
	 * This will allow logging of record containing long dns */

	va_start(ap, format);
	logBuffer.logBufSize = vsnprintf(str, sizeof(str), format, ap);
	va_end(ap);

	if (logBuffer.logBufSize > SA_LOG_MAX_RECORD_SIZE) {
		LOG_NO("saflog write FAILED: log record size > %u max limit",
		       SA_LOG_MAX_RECORD_SIZE);
		return;
	}

	// Initialize log client and open system stream if they are NOT
	saflog_init();
	if (log_client_initialized == false) {
		LOG_NO("saflog write \"%s\" FAILED", str);
		return;
	}

	logRecord.logTimeStamp = SA_TIME_UNKNOWN;
	logRecord.logHdrType = SA_LOG_GENERIC_HEADER;
	logRecord.logHeader.genericHdr.notificationClassId = NULL;
	logRecord.logHeader.genericHdr.logSeverity = priority;
	logRecord.logHeader.genericHdr.logSvcUsrName = logSvcUsrName;
	logRecord.logBuffer = &logBuffer;
	logBuffer.logBuf = (SaUint8T *)str;

	rc = saLogWriteLogAsync(logStreamHandle, 0, 0, &logRecord);

	if (rc != SA_AIS_OK) {
		LOG_NO("saflog write \"%s\" FAILED: %s", str, saf_error(rc));
		if (rc == SA_AIS_ERR_BAD_HANDLE) {
			log_client_initialized = false;
			saLogFinalize(logHandle);
		}
	}
}
