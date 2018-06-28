/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2008 The OpenSAF Foundation
 * Copyright Ericsson AB 2018 - All Rights Reserved.
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

#ifndef BASE_NCSSYSF_TMR_H_
#define BASE_NCSSYSF_TMR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/ncsgl_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// A timer handle, that can be allocated using ncs_tmr_alloc() and freed using
// ncs_tmr_free().
typedef void* tmr_t;

// A null (unallocated) timer handle, analogous to a null pointer.
#define TMR_T_NULL ((tmr_t*)0)

// Timer callback function, which is called when a timer has expired.
typedef void (*TMR_CALLBACK)(void*);

// Initialize the timer functionality. This function must be called before any
// other timer function can be used. Returns true if successful, and false
// otherwise.
bool sysfTmrCreate(void);

// Free resources allocated by a previous call to sysfTmrCreate(), and free all
// timers that may have been allocated using ncs_tmr_alloc(). No other timer
// function except sysfTmrCreate() can be used after this function has been
// called. Returns true if successful, and false otherwise.
bool sysfTmrDestroy(void);

// Returns a newly allocated timer handle. The function functionality must have
// been initialized using the function sysfTmrCreate() before ncs_tmr_alloc()
// can be called. The two function parameters are ignored but should be set to
// nullptr and 0, respectively.
tmr_t ncs_tmr_alloc(const char*, uint32_t);

// Free @a timer, which has previously been allocated using ncs_tmr_alloc(). If
// the timer is currently running it will first be stopped.
void ncs_tmr_free(tmr_t);

// Start @a timer, which has previously been allocated using
// ncs_tmr_alloc(). The timer will expire once when @a timeout centiseconds (10
// ** -2 seconds), at which point the @a callback will be called with the
// specified @a argument. If the timer is already running it will be updated
// with the new @a timeout, @a callback and @a argument. The last two function
// parameters of ncs_tmr_start() are ignored but should be set to nullptr and 0,
// respectively.
tmr_t ncs_tmr_start(tmr_t timer, int64_t timeout, TMR_CALLBACK callback,
                    void* argument, const char*, uint32_t);

// Stop @a timer, which has previously been started using ncs_tmr_start(). It is
// harmless to stop a timer which is not running, in which case nothing will
// happen.
void ncs_tmr_stop(tmr_t);

// Stores remaining duration of @a timer in the unit of myriadth seconds (10 **
// -4 seconds) in the optput parameter @a time_left. Returns NCSCC_RC_SUCCESS if
// successful, and NCSCC_RC_FAILURE otherwise. One reason why this function may
// fail is that the specified @a timer is currently not running.
uint32_t ncs_tmr_remaining(tmr_t timer, int64_t* time_left);

// Old name of ncs_tmr_stop. Deprecated, use the new name instead.
static inline void m_NCS_TMR_STOP(tmr_t tid) { ncs_tmr_stop(tid); }
// Old name of ncs_tmr_free. Deprecated, use the new name instead.
static inline void m_NCS_TMR_DESTROY(tmr_t tid) { ncs_tmr_free(tid); }
// Old name of ncs_tmr_remaining. Deprecated, use the new name instead.
static inline uint32_t m_NCS_TMR_MSEC_REMAINING(tmr_t tid, int64_t* p_tleft) {
  return ncs_tmr_remaining(tid, p_tleft);
}
// Old name of ncs_tmr_alloc. Deprecated, use the new name instead.
#define m_NCS_TMR_CREATE(tid, prd, cb, arg) (tid = ncs_tmr_alloc(NULL, 0))
// Old name of ncs_tmr_start. Deprecated, use the new name instead.
#define m_NCS_TMR_START(tid, prd, cb, arg) \
  (tid = ncs_tmr_start(tid, prd, cb, arg, NULL, 0))

#ifdef __cplusplus
}
#endif

#endif  // BASE_NCSSYSF_TMR_H_
