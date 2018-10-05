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

/* ========================================================================
 *   INCLUDE FILES
 * ========================================================================
 */
#include "ntf/ntfd/NtfReader.h"
#include "ntf/ntfd/NtfLogger.h"
#include <iostream>
#include "base/logtrace.h"
#include "ntf/ntfd/NtfNotification.h"

/* ========================================================================
 *   DEFINITIONS
 * ========================================================================
 */

/* ========================================================================
 *   TYPE DEFINITIONS
 * ========================================================================
 */

/* ========================================================================
 *   DATA DECLARATIONS
 * ========================================================================
 */

/* ========================================================================
 *   FUNCTION PROTOTYPES
 * ========================================================================
 */

NtfReader::NtfReader(NtfLogger& ntfLogger, unsigned int readerId,
    ntfsv_reader_init_req_t *req)
    : coll_(ntfLogger.coll_),
      ffIter(coll_.begin()),
      readerId_(readerId),
      hasFilter_(false),
      firstRead_(true),
      read_init_req_(*req){
  searchCriteria_.eventTime = 0;
  searchCriteria_.notificationId = 0;
  searchCriteria_.searchMode = SA_NTF_SEARCH_NOTIFICATION_ID;
  TRACE_3("ntfLogger.coll_.size: %u", (unsigned int)ntfLogger.coll_.size());
}

NtfReader::NtfReader(NtfLogger& ntfLogger, unsigned int readerId,
    ntfsv_reader_init_req_2_t *req)
    : readerId_(readerId),
      hasFilter_(true),
      firstRead_(true),
      read_init_2_req_(*req){
  TRACE_3("New NtfReader with filter, ntfLogger.coll_.size: %u",
          (unsigned int)ntfLogger.coll_.size());
  searchCriteria_ = req->head.searchCriteria;
  if (req->f_rec.alarm_filter) {
    NtfFilter* filter = new NtfAlarmFilter(req->f_rec.alarm_filter);
    filterMap[filter->type()] = filter;
  }
  if (req->f_rec.sec_al_filter) {
    NtfFilter* filter = new NtfSecurityAlarmFilter(req->f_rec.sec_al_filter);
    filterMap[filter->type()] = filter;
  }
  filterCacheList(ntfLogger);
}

NtfReader::~NtfReader() {
  FilterMap::iterator posN = filterMap.begin();
  while (posN != filterMap.end()) {
    NtfFilter* filter = posN->second;
    TRACE_2("Delete filter type %#x", (int)filter->type());
    delete filter;
    filterMap.erase(posN++);
  }
}

void NtfReader::filterCacheList(NtfLogger& ntfLogger) {
  TRACE_ENTER();
  if (hasFilter_ == true) {
    readerNotificationListT::iterator rpos;
    for (rpos = ntfLogger.coll_.begin(); rpos != ntfLogger.coll_.end(); rpos++) {
      NtfSmartPtr n(*rpos);
      bool rv = false;
      FilterMap::iterator pos = filterMap.find(n->getNotificationType());
      if (pos != filterMap.end()) {
        NtfFilter* filter = pos->second;
        osafassert(filter);
        rv = filter->checkFilter(n);
      }
      if (rv) {
        if (filterSearchCriteria(n)) break;
      }
    }
  } else {
    coll_ = ntfLogger.coll_;
  }

  ffIter = coll_.begin();
  TRACE_LEAVE();
}

bool NtfReader::filterSearchCriteria(NtfSmartPtr& notification) {
  bool filter_done = false;
  switch (searchCriteria_.searchMode) {
    case SA_NTF_SEARCH_BEFORE_OR_AT_TIME:
      if (*notification->header()->eventTime <= searchCriteria_.eventTime)
        coll_.push_back(notification);
      break;

    case SA_NTF_SEARCH_AT_TIME:
      if (*notification->header()->eventTime == searchCriteria_.eventTime)
        coll_.push_back(notification);
      break;

    case SA_NTF_SEARCH_AT_OR_AFTER_TIME:
      if (*notification->header()->eventTime >= searchCriteria_.eventTime)
        coll_.push_back(notification);
      break;

    case SA_NTF_SEARCH_BEFORE_TIME:
      if (*notification->header()->eventTime < searchCriteria_.eventTime)
        coll_.push_back(notification);
      break;

    case SA_NTF_SEARCH_AFTER_TIME:
      if (*notification->header()->eventTime > searchCriteria_.eventTime)
        coll_.push_back(notification);
      break;

    case SA_NTF_SEARCH_NOTIFICATION_ID:
      if (*notification->header()->notificationId ==
                                searchCriteria_.notificationId) {
        coll_.push_back(notification);
        filter_done = true;
      }
      break;

    case SA_NTF_SEARCH_ONLY_FILTER:
    default:
      unsigned int nId = notification->getNotificationId();
      TRACE_3("nId: %u added to readerList", nId);
      coll_.push_back(notification);
      break;
  }
  return filter_done;
}

NtfSmartPtr NtfReader::next(SaNtfSearchDirectionT direction,
                            SaAisErrorT* error) {
  TRACE_ENTER();

  *error = SA_AIS_ERR_NOT_EXIST;
  if (direction == SA_NTF_SEARCH_YOUNGER ||
      searchCriteria_.searchMode == SA_NTF_SEARCH_AT_TIME ||
      searchCriteria_.searchMode == SA_NTF_SEARCH_ONLY_FILTER) {
    if (ffIter >= coll_.end()) {
      *error = SA_AIS_ERR_NOT_EXIST;
      TRACE_LEAVE();
      NtfSmartPtr notif;
      firstRead_ = false;
      return notif;
    }
    NtfSmartPtr notif(*ffIter);
    ffIter++;
    *error = SA_AIS_OK;
    TRACE_LEAVE();
    firstRead_ = false;
    return notif;
  } else {  // SA_NTF_SEARCH_OLDER
    readerNotReverseIterT rIter(ffIter);
    if (firstRead_)
      rIter = coll_.rbegin();
    else
      ++rIter;

    if (rIter >= coll_.rend() || rIter < coll_.rbegin()) {
      *error = SA_AIS_ERR_NOT_EXIST;
      ffIter = rIter.base();
      TRACE_LEAVE();
      NtfSmartPtr notif;
      firstRead_ = false;
      return notif;
    }
    NtfSmartPtr notif(*rIter);
    ffIter = rIter.base();
    *error = SA_AIS_OK;
    TRACE_LEAVE();
    firstRead_ = false;
    return notif;
  }
}

void NtfReader::syncRequest(NCS_UBAID* uba) {
  TRACE_ENTER();
  syncReaderInfo(readerId_, uint8_t(hasFilter_),
      (uint32_t)std::distance(coll_.begin(), ffIter),
      uint8_t(firstRead_), uba);
  if (hasFilter_ == true) {
    syncReaderWithFilter(&read_init_2_req_, uba);
  } else {
    syncReaderWithoutFilter(&read_init_req_, uba);
  }
  TRACE_LEAVE();
}

unsigned int NtfReader::getId() { return readerId_; }
