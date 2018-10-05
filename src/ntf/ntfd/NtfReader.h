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

#ifndef NTF_NTFD_NTFREADER_H_
#define NTF_NTFD_NTFREADER_H_

/* ========================================================================
 *   INCLUDE FILES
 * ========================================================================
 */
#include "ntf/ntfd/NtfNotification.h"
#include "ntf/ntfd/NtfFilter.h"
/* ========================================================================
 *   DEFINITIONS
 * ========================================================================
 */

/* ========================================================================
 *   TYPE DEFINITIONS
 * ========================================================================
 */
typedef std::deque<NtfSmartPtr> readerNotificationListT;
typedef std::deque<NtfSmartPtr>::iterator readerNotificationListTIter;
typedef std::deque<NtfSmartPtr>::reverse_iterator readerNotReverseIterT;

/* ========================================================================
 *   DATA DECLARATIONS
 * ========================================================================
 */
class NtfLogger;
class NtfCriteriaFilter;

class NtfReader {
  friend class NtfCriteriaFilter;

 public:
  // NtfReader object creates a copy of the current cashed
  // notifications. The private forward iterator ffIter is set
  // to first element.
  NtfReader(NtfLogger& ntfLogger, unsigned int readerId,
      ntfsv_reader_init_req_t *req);

  // NtfReader object creates a copy of the current cashed
  // notifications that matches the filter and the search
  // criteria. The private forward iterator ffIter is set
  // according to the searchCriteria. This constructor is used
  // when filter is provided.
  NtfReader(NtfLogger& ntfLogger, unsigned int readerId,
      ntfsv_reader_init_req_2_t *req);

  ~NtfReader();

  // This method is called to check which notifications that
  // matches with the referenced filter and the filtering criteria
  // and copy these notications to the read list
  void filterCacheList(NtfLogger& ntfLogger);

  // This method returns the notification at the current
  // position of the iterator @ffIter if search direction is
  // SA_NTF_SEARCH_YOUNGER.
  NtfSmartPtr next(SaNtfSearchDirectionT direction, SaAisErrorT* error);

  // Get reader-id
  unsigned int getId();

  // This method is to sync the reader information to the standby NTFD
  void syncRequest(NCS_UBAID* uba);

  // Set Reader-id
  void setReaderId(unsigned int readerId) { readerId_ = readerId; }

  // Set current pointer of reader in the reader notification list
  void setReaderIteration(unsigned int iterPos) {
    ffIter = coll_.begin() + iterPos;}

  // Set the flag if the reading is first time
  void setFirstRead(bool firstRead) {firstRead_ = firstRead; }

  // Filter the notification with the search criteria
  // Return true if it doesn't need other notification to check search criteria
  bool filterSearchCriteria(NtfSmartPtr& notification);

 private:
  readerNotificationListT coll_;
  readerNotificationListTIter ffIter;
  FilterMap filterMap;
  unsigned int readerId_;
  SaNtfSearchCriteriaT searchCriteria_;
  bool hasFilter_;
  bool firstRead_;
  ntfsv_reader_init_req_t read_init_req_;
  ntfsv_reader_init_req_2_t read_init_2_req_;
};

#endif  // NTF_NTFD_NTFREADER_H_
