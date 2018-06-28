
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
 * Author(s): Ericsson AB
 *
 */

/**
 * This file defines APIs for logging and tracing.
 *
 * Logging is enabled by default and level based. Current backend for logging is
 * syslog, in the future SAF log could be used for some services (but not all -
 * boot strapping problem). Filtering is done with the mechanisms of the back
 * end e.g. the syslog settings.
 *
 * Tracing is disabled by default and category based. Categories are or-ed into
 * the current mask setting and and-ed with the mask during filtering. Current
 * backend for tracing is file, in the future syslog could be used.
 * Filtering is done by the file back end.
 */

#ifndef BASE_LOGTRACE_H_
#define BASE_LOGTRACE_H_

#include <syslog.h>
#include <stdarg.h>
#include "base/ncsgl_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Categories */
enum logtrace_categories {
  CAT_LOG = 0,
  CAT_TRACE,
  CAT_TRACE1,
  CAT_TRACE2,
  CAT_TRACE3,
  CAT_TRACE4,
  CAT_TRACE5,
  CAT_TRACE6,
  CAT_TRACE7,
  CAT_TRACE8,
  CAT_TRACE_ENTER,
  CAT_TRACE_LEAVE,
  CAT_MAX
};

#define CATEGORY_ALL 0xffffffff

/**
 * logtrace_init - Initialize the logtrace system.
 *
 * @param ident An identity string to be prepended to every message. Typically
 * set to the program name.
 * @param pathname The pathname parameter should contain a valid
 * path name for a file if tracing is to be enabled. The user must have write
 * access to that file. If the file already exist, it is appended. If the file
 * name is not valid, no tracing is performed.
 * @param mask The initial trace mask. Should be set set to zero by
 *             default (trace disabled)
 *
 * @return int - 0 if OK, -1 otherwise
 */
extern int logtrace_init(const char *ident, const char *pathname,
                         unsigned mask);

/**
 * logtrace_init_daemon - Initialize the logtrace system for daemons
 *
 * @param ident An identity string to be prepended to every message. Typically
 * set to the program name.
 * @param pathname The pathname parameter should contain a valid
 * path name for a file if tracing is to be enabled. The user must have write
 * access to that file. If the file already exist, it is appended. If the file
 * name is not valid, no tracing is performed.
 * @param tracemask The initial trace mask. Should be set set to zero by
 *             default (trace disabled)
 * @param logmask The initial log level to be set for log filtering.
 *
 * @return int - 0 if OK, -1 otherwise
 */
extern int logtrace_init_daemon(const char *ident, const char *pathname,
                                unsigned tracemask, int logmask);

/*
 * logtrace_exit_daemon
 * This should be called when a daemon exit
 */
extern int logtrace_exit_daemon();

/**
 * trace_category_set - Set the mask used for trace filtering.
 *
 * In libraries the category mask is typically set (to all ones) after a
 * library specific environment variable has been examined for the name of a
 * output file. The variable will not exist in a production system hence no
 * tracing will be done.
 *
 * In daemons there is no need to call this function during initialization,
 * tracing is disabled by default. In runtime, using e.g. a signal handler the
 * daemon could call this function to enable or change traced categories.
 *
 * @param category_mask The mask to set, 0 indicates no tracing.
 *
 * @return int - 0 if OK, -1 otherwise
 */
extern int trace_category_set(unsigned category_mask);

/**
 * trace_category_get - Get the current mask used for trace filtering.
 *
 * @return int - The filtering mask value
 */
extern unsigned trace_category_get(void);

/* internal functions, do not use directly */
extern void logtrace_log(const char *file, unsigned line, int priority,
                         const char *format, ...)
    __attribute__((format(printf, 4, 5)));
extern void logtrace_trace(const char *file, unsigned line, unsigned category,
                           const char *format, ...)
    __attribute__((format(printf, 4, 5)));

extern bool is_logtrace_enabled(unsigned category);
extern void trace_output(const char *file, unsigned line, unsigned priority,
                            unsigned category, const char *format, va_list ap);
extern void log_output(const char *file, unsigned line, unsigned priority,
                            unsigned category, const char *format, va_list ap);

/* LOG API. Use same levels as syslog */
#define LOG_EM(format, args...) \
  logtrace_log(__FILE__, __LINE__, LOG_EMERG, (format), ##args)
#define LOG_AL(format, args...) \
  logtrace_log(__FILE__, __LINE__, LOG_ALERT, (format), ##args)
#define LOG_CR(format, args...) \
  logtrace_log(__FILE__, __LINE__, LOG_CRIT, (format), ##args)
#define LOG_ER(format, args...) \
  logtrace_log(__FILE__, __LINE__, LOG_ERR, (format), ##args)
#define LOG_WA(format, args...) \
  logtrace_log(__FILE__, __LINE__, LOG_WARNING, (format), ##args)
#define LOG_NO(format, args...) \
  logtrace_log(__FILE__, __LINE__, LOG_NOTICE, (format), ##args)
#define LOG_IN(format, args...) \
  logtrace_log(__FILE__, __LINE__, LOG_INFO, (format), ##args)

/* TRACE API. */
#define TRACE(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE, (format), ##args)
#define TRACE_1(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE1, (format), ##args)
#define TRACE_2(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE2, (format), ##args)
#define TRACE_3(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE3, (format), ##args)
#define TRACE_4(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE4, (format), ##args)
#define TRACE_5(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE5, (format), ##args)
#define TRACE_6(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE6, (format), ##args)
#define TRACE_7(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE7, (format), ##args)
#define TRACE_8(format, args...) \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE8, (format), ##args)

#ifdef __cplusplus

class Trace {
 public:
  Trace() = delete;
  Trace(const char *file, const char *function) {
    file_ = file;
    function_ = function;
  }
  ~Trace() {
    if (!trace_leave_called && is_logtrace_enabled(CAT_TRACE_LEAVE)) {
      va_list ap{};
      trace_output(file_, 0, LOG_DEBUG, CAT_TRACE_LEAVE, function_, ap);
    }
  }
  void trace(const char *file, const char *function, unsigned line,
             unsigned category, const char *format, ...) {
    va_list ap;
    if (is_logtrace_enabled(category)) {
      file_ = file;
      function_ = function;
      va_start(ap, format);
      trace_output(file, line, LOG_DEBUG, category, format, ap);
      va_end(ap);
    }
  }

  void trace_leave(const char *file, unsigned line, unsigned category,
                   const char *format, ...) {
    va_list ap;
    if (is_logtrace_enabled(category)) {
      va_start(ap, format);
      trace_leave_called = true;
      trace_output(file, line, LOG_DEBUG, category, format, ap);
      va_end(ap);
    }
  }

 private:
  bool trace_leave_called{false};
  const char *file_{nullptr};
  const char *function_{nullptr};
};

#define TRACE_ENTER()                                                \
  Trace t_(__FILE__, __FUNCTION__); \
  t_.trace(__FILE__, __FUNCTION__, __LINE__, CAT_TRACE_ENTER, "%s ", \
           __FUNCTION__)
#define TRACE_ENTER2(format, args...)                                        \
  Trace t_(__FILE__, __FUNCTION__); \
  t_.trace(__FILE__, __FUNCTION__, __LINE__, CAT_TRACE_ENTER, "%s: " format, \
           __FUNCTION__, ##args)

#define TRACE_LEAVE() \
  t_.trace_leave(__FILE__, __LINE__, CAT_TRACE_LEAVE, "%s ", __FUNCTION__)
#define TRACE_LEAVE2(format, args...)                                \
  t_.trace_leave(__FILE__, __LINE__, CAT_TRACE_LEAVE, "%s: " format, \
                 __FUNCTION__, ##args)
#else
#define TRACE_ENTER() \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE_ENTER, "%s ", __FUNCTION__)
#define TRACE_ENTER2(format, args...)                                \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE_ENTER, "%s: " format, \
                 __FUNCTION__, ##args)
#define TRACE_LEAVE() \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE_LEAVE, "%s ", __FUNCTION__)
#define TRACE_LEAVE2(format, args...)                                \
  logtrace_trace(__FILE__, __LINE__, CAT_TRACE_LEAVE, "%s: " format, \
                 __FUNCTION__, ##args)
#endif

#ifdef __cplusplus
}
#endif

#endif  // BASE_LOGTRACE_H_
