/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2008, 2017 - All Rights Reserved.
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

#include "log/agent/lga_agent.h"

/***************************************************************************
 * 8.4.1
 *
 * saLogInitialize()
 *
 * This function initializes the Log Service.
 *
 * Parameters
 *
 * logHandle - [out] A pointer to the handle for this initialization of the
 *                   Log Service.
 * callbacks - [in] A pointer to the callbacks structure that contains the
 *                  callback functions of the invoking process that the
 *                  Log Service may invoke.
 * version - [in] A pointer to the version of the Log Service that the
 *                invoking process is using.
 *
 ***************************************************************************/
SaAisErrorT saLogInitialize(SaLogHandleT* logHandle,
                            const SaLogCallbacksT* callbacks,
                            SaVersionT* version) {
  return LogAgent::instance()->saLogInitialize(logHandle, callbacks, version);
}

/***************************************************************************
 * 8.4.2
 *
 * saLogSelectionObjectGet()
 *
 * The saLogSelectionObjectGet() function returns the operating system handle
 * selectionObject, associated with the handle logHandle, allowing the
 * invoking process to ascertain when callbacks are pending. This function
 * allows a process to avoid repeated invoking saLogDispatch() to see if
 * there is a new event, thus, needlessly consuming CPU time. In a POSIX
 * environment the system handle could be a file descriptor that is used with
 * the poll() system call to detect incoming callbacks.
 *
 *
 * Parameters
 *
 * logHandle - [in]
 *   A pointer to the handle, obtained through the saLogInitialize() function,
 *   designating this particular initialization of the Log Service.
 *
 * selectionObject - [out]
 *   A pointer to the operating system handle that the process may use to
 *   detect pending callbacks.
 *
 ***************************************************************************/
SaAisErrorT saLogSelectionObjectGet(SaLogHandleT logHandle,
                                    SaSelectionObjectT* selectionObject) {
  return LogAgent::instance()->saLogSelectionObjectGet(logHandle,
                                                      selectionObject);
}

/***************************************************************************
 * 8.4.3
 *
 * saLogDispatch()
 *
 * The saLogDispatch() function invokes, in the context of the calling thread,
 * one or all of the pending callbacks for the handle logHandle.
 *
 *
 * Parameters
 *
 * logHandle - [in]
 *   A pointer to the handle, obtained through the saLogInitialize() function,
 *   designating this particular initialization of the Log Service.
 *
 * dispatchFlags - [in]
 *   Flags that specify the callback execution behavior of the the
 *   saLogDispatch() function, which have the values SA_DISPATCH_ONE,
 *   SA_DISPATCH_ALL or SA_DISPATCH_BLOCKING, as defined in Section 3.3.8.
 *
 ***************************************************************************/
SaAisErrorT saLogDispatch(SaLogHandleT logHandle,
                          SaDispatchFlagsT dispatchFlags) {
  return LogAgent::instance()->saLogDispatch(logHandle, dispatchFlags);
}

/***************************************************************************
 * 8.4.4
 *
 * saLogFinalize()
 *
 * The saLogFinalize() function closes the association, represented by the
 * logHandle parameter, between the process and the Log Service.
 * It may free up resources.
 *
 * This function cannot be invoked before the process has invoked the
 * corresponding saLogInitialize() function for the Log Service.
 * After this function is invoked, the selection object is no longer valid.
 * Moreover, the Log Service is unavailable for further use unless it is
 * reinitialized using the saLogInitialize() function.
 *
 * Parameters
 *
 * logHandle - [in]
 *   A pointer to the handle, obtained through the saLogInitialize() function,
 *   designating this particular initialization of the Log Service.
 *
 ***************************************************************************/
SaAisErrorT saLogFinalize(SaLogHandleT logHandle) {
  return LogAgent::instance()->saLogFinalize(logHandle);
}

/**
 * API function for opening a stream
 *
 * @param logHandle
 * @param logStreamName
 * @param logFileCreateAttributes
 * @param logStreamOpenFlags
 * @param timeOut
 * @param logStreamHandle
 *
 * @return SaAisErrorT
 */
SaAisErrorT saLogStreamOpen_2(
    SaLogHandleT logHandle, const SaNameT* logStreamName,
    const SaLogFileCreateAttributesT_2* logFileCreateAttributes,
    SaLogStreamOpenFlagsT logStreamOpenFlags, SaTimeT timeOut,
    SaLogStreamHandleT* logStreamHandle) {
  return LogAgent::instance()->saLogStreamOpen_2(
      logHandle, logStreamName, logFileCreateAttributes, logStreamOpenFlags,
      timeOut, logStreamHandle);
}

/**
 *
 * @param logHandle
 * @param logStreamName
 * @param logFileCreateAttributes
 * @param logstreamOpenFlags
 * @param invocation
 *
 * @return SaAisErrorT
 */
SaAisErrorT saLogStreamOpenAsync_2(
    SaLogHandleT logHandle, const SaNameT* logStreamName,
    const SaLogFileCreateAttributesT_2* logFileCreateAttributes,
    SaLogStreamOpenFlagsT logstreamOpenFlags, SaInvocationT invocation) {
  return LogAgent::instance()->saLogStreamOpenAsync_2(
      logHandle, logStreamName, logFileCreateAttributes, logstreamOpenFlags,
      invocation);
}

/**
 *
 * @param logStreamHandle
 * @param timeOut
 * @param logRecord
 *
 * @return SaAisErrorT
 */
SaAisErrorT saLogWriteLog(SaLogStreamHandleT logStreamHandle, SaTimeT timeOut,
                          const SaLogRecordT* logRecord) {
  return LogAgent::instance()->saLogWriteLog(logStreamHandle, timeOut,
                                            logRecord);
}

/**
 *
 * @param logStreamHandle
 * @param invocation
 * @param ackFlags
 * @param logRecord
 *
 * @return SaAisErrorT
 */
SaAisErrorT saLogWriteLogAsync(SaLogStreamHandleT logStreamHandle,
                               SaInvocationT invocation,
                               SaLogAckFlagsT ackFlags,
                               const SaLogRecordT* logRecord) {
  return LogAgent::instance()->saLogWriteLogAsync(logStreamHandle, invocation,
                                                 ackFlags, logRecord);
}
/**
 * API function for closing stream
 *
 * @param logStreamHandle
 *
 * @return SaAisErrorT
 */
SaAisErrorT saLogStreamClose(SaLogStreamHandleT logStreamHandle) {
  return LogAgent::instance()->saLogStreamClose(logStreamHandle);
}

/**
 *
 * @param logHandle
 * @param limitId
 * @param limitValue
 *
 * @return SaAisErrorT
 */
SaAisErrorT saLogLimitGet(SaLogHandleT logHandle, SaLogLimitIdT limitId,
                          SaLimitValueT* limitValue) {
  return LogAgent::instance()->saLogLimitGet(logHandle, limitId, limitValue);
}
