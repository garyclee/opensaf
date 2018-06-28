/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2017 - All Rights Reserved.
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
 * Author(s): Emerson Network Power
 *
 */

/*****************************************************************************
..............................................................................
  DESCRIPTION:  MDS LOG APIs

  ******************************************************************************
  */

#include "mds/mds_log.h"
#include "base/logtrace_client.h"
#include "mds/mds_dt2c.h"
#include "mds/mds_papi.h"

int gl_mds_log_level = 3;
LogTraceClient* gl_mds_log = nullptr;

/*******************************************************************************
 * Funtion Name   :    mds_log_init
 *
 * Purpose        :
 *
 * Return Value   :    None
 *
 *******************************************************************************/
uint32_t mds_log_init(const char *) {
  if (!gl_mds_log) {
    gl_mds_log = new LogTraceClient("mds.log",
        LogTraceClient::kRemoteNonblocking);
  }
  tzset();
  log_mds_notify("BEGIN MDS LOGGING| ARCHW=%x|64bit=%zu\n", MDS_SELF_ARCHWORD,
                 MDS_WORD_SIZE_TYPE);
  return NCSCC_RC_SUCCESS;
}

/*******************************************************************************
 * Funtion Name   :    log_mds_critical
 *
 * Purpose        :
 *
 * Return Value   :    None
 *
 *******************************************************************************/
void log_mds_critical(const char *fmt, ...) {
  if (gl_mds_log_level < NCSMDS_LC_CRITICAL) return;
  va_list ap;
  va_start(ap, fmt);
  LogTraceClient::Log(gl_mds_log, base::LogMessage::Severity::kCrit, fmt, ap);
  va_end(ap);
}

/*******************************************************************************
 * Funtion Name   :    log_mds_err
 *
 * Purpose        :
 *
 * Return Value   :    None
 *
 *******************************************************************************/
void log_mds_err(const char *fmt, ...) {
  if (gl_mds_log_level < NCSMDS_LC_ERR) return;
  va_list ap;
  va_start(ap, fmt);
  LogTraceClient::Log(gl_mds_log, base::LogMessage::Severity::kErr, fmt, ap);
  va_end(ap);
}

/*******************************************************************************
 * Funtion Name   :    log_mds_notify
 *
 * Purpose        :
 *
 * Return Value   :    None
 *
 *******************************************************************************/
void log_mds_notify(const char *fmt, ...) {
  if (gl_mds_log_level < NCSMDS_LC_NOTIFY) return;
  va_list ap;
  va_start(ap, fmt);
  LogTraceClient::Log(gl_mds_log, base::LogMessage::Severity::kNotice, fmt, ap);
  va_end(ap);
}

/*******************************************************************************
 * Funtion Name   :    log_mds_info
 *
 * Purpose        :
 *
 * Return Value   :    None
 *
 *******************************************************************************/
void log_mds_info(const char *fmt, ...) {
  if (gl_mds_log_level < NCSMDS_LC_INFO) return;
  va_list ap;
  va_start(ap, fmt);
  LogTraceClient::Log(gl_mds_log, base::LogMessage::Severity::kInfo, fmt, ap);
  va_end(ap);
}

/*******************************************************************************
 * Funtion Name   :    log_mds_dbg
 *
 * Purpose        :
 *
 * Return Value   :    NCSCC_RC_SUCCESS
 *                     NCSCC_RC_FAILURE
 *
 *******************************************************************************/
void log_mds_dbg(const char *fmt, ...) {
  if (gl_mds_log_level < NCSMDS_LC_DBG) return;
  va_list ap;
  va_start(ap, fmt);
  LogTraceClient::Log(gl_mds_log, base::LogMessage::Severity::kDebug, fmt, ap);
  va_end(ap);
}
