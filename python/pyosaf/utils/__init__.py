############################################################################
#
# (C) Copyright 2014 The OpenSAF Foundation
# (C) Copyright 2017 Ericsson AB. All rights reserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
# under the GNU Lesser General Public License Version 2.1, February 1999.
# The complete license can be accessed from the following location:
# http://opensource.org/licenses/lgpl-license.php
# See the Copying file included with the OpenSAF distribution for full
# licensing terms.
#
# Author(s): Oracle, Ericsson
#
############################################################################
""" pyosaf common utils """
import os
import syslog
import time
import warnings
from copy import deepcopy
from ctypes import POINTER

from pyosaf import saImmOm
from pyosaf.saAis import eSaAisErrorT, SaStringT, unmarshalNullArray

# Version for pyosaf utils
__version__ = "1.0.1"

# The 'MAX_RETRY_TIME' and 'RETRY_INTERVAL' environment variables shall be set
# with user-defined values BEFORE importing the pyosaf 'utils' module;
# Otherwise, the default values for MAX_RETRY_TIME(60s) and RETRY_INTERVAL(1s)
# will be used throughout.
MAX_RETRY_TIME = int(os.environ.get("MAX_RETRY_TIME")) \
    if "MAX_RETRY_TIME" in os.environ else 60
RETRY_INTERVAL = int(os.environ.get("RETRY_INTERVAL")) \
    if "RETRY_INTERVAL" in os.environ else 1


class SafException(Exception):
    """ SAF Exception for error during executing SAF functions """
    def __init__(self, value, msg=None):
        Exception.__init__(self)
        self.value = value
        self.msg = msg

    def __str__(self):
        return eSaAisErrorT.whatis(self.value)


def raise_saf_exception(func, error):
    """ Raise an exception for a given SAF function, based on the given
    error code
    """
    error_string = "%s: %s" % (func.__name__, eSaAisErrorT.whatis(error))
    raise SafException(error, error_string)


def check_resource_abort(ccb_handle):
    """ Get error strings from IMM and check if it is a resource abort case

    Args:
        ccb_handle (SaImmCcbHandleT): CCB handle

    Returns:
        bool: 'True' if it is a resource abort case. 'False', otherwise.
    """
    c_error_strings = POINTER(SaStringT)()
    saImmOmCcbGetErrorStrings = decorate(saImmOm.saImmOmCcbGetErrorStrings)

    # Get error strings
    # As soon as the ccb_handle is finalized, the strings are freed
    rc = saImmOmCcbGetErrorStrings(ccb_handle, c_error_strings)
    if rc == eSaAisErrorT.SA_AIS_OK:
        if c_error_strings:
            list_err_strings = unmarshalNullArray(c_error_strings)
            for c_error_string in list_err_strings:
                if c_error_string.startswith("IMM: Resource abort: "):
                    if "CCB is in an error state" not in c_error_string:
                        return True

    return False


def decorate(func):
    """ Decorate the given SAF function so that it retries a fixed number of
    times for certain returned error codes during execution

    Args:
        func (function): The decorated SAF function

    Returns:
        SaAisErrorT: Return code of the decorated SAF function
    """

    def inner(*args):
        """ Call the decorated function in the lexical scope in a retry loop

        Args:
            args (tuple): Arguments of the decorated SAF function
        """
        count_sec_sleeps = 0

        rc = func(*args)

        while rc != eSaAisErrorT.SA_AIS_OK:

            if count_sec_sleeps >= MAX_RETRY_TIME:
                break

            if rc == eSaAisErrorT.SA_AIS_ERR_TRY_AGAIN:
                sleep_time_interval = RETRY_INTERVAL
            elif rc == eSaAisErrorT.SA_AIS_ERR_NO_RESOURCES:
                sleep_time_interval = RETRY_INTERVAL
            elif rc == eSaAisErrorT.SA_AIS_ERR_BUSY:
                sleep_time_interval = 3 * RETRY_INTERVAL
            elif rc == eSaAisErrorT.SA_AIS_ERR_FAILED_OPERATION:
                # Retry on getting FAILED_OPERATION only applies to IMM
                # CCB-related operations in case of a resource abort
                # and CCB not in error state.
                ccb_handle = args[0]
                resource_abort = check_resource_abort(ccb_handle)
                if not resource_abort:
                    break
                sleep_time_interval = RETRY_INTERVAL
            else:
                break  # Break out of the retry loop

            # Check sleep_time_interval to sleep and retry the function
            time.sleep(sleep_time_interval)
            count_sec_sleeps += sleep_time_interval
            rc = func(*args)

        return rc

    return inner


def initialize_decorate(init_func):
    """ Decorate the given SAF sa<Service>Initialize() function so that it
    retries a fixed number of times with the same arguments for certain
    returned error codes during execution
    """

    def inner(*args):
        """ Call the decorated Initialize() function in the lexical scope in a
        retry loop

        Args:
            args (tuple): Arguments of the SAF Initialize() function with
                format (handle, callbacks, version)
        """
        count_sec_sleeps = 0

        # Backup the current version
        backup_version = deepcopy(args[2])

        rc = init_func(*args)
        while rc != eSaAisErrorT.SA_AIS_OK:

            if count_sec_sleeps >= MAX_RETRY_TIME:
                break

            if rc == eSaAisErrorT.SA_AIS_ERR_TRY_AGAIN:
                sleep_time_interval = RETRY_INTERVAL
            elif rc == eSaAisErrorT.SA_AIS_ERR_NO_RESOURCES:
                sleep_time_interval = RETRY_INTERVAL
            elif rc == eSaAisErrorT.SA_AIS_ERR_BUSY:
                sleep_time_interval = 3 * RETRY_INTERVAL
            else:
                break  # Break out of the retry loop

            # Check sleep_time_interval to sleep and retry the function
            time.sleep(sleep_time_interval)
            count_sec_sleeps += sleep_time_interval
            # If the SAF Initialize() function returns ERR_TRY_AGAIN, the
            # version (as output argument) will still get updated to the
            # latest supported service API version; thus we need to restore
            # the original backed-up version before the next retry of
            # initialization.
            version = deepcopy(backup_version)
            args = args[:2] + (version,)
            rc = init_func(*args)

        return rc

    return inner


def bad_handle_retry(func):
    """ Decorate the given function so that it retries a fixed number of times
    if getting the error code SA_AIS_ERR_BAD_HANDLE during execution

    Args:
        func (function): The decorated function

    Returns:
        Return code/output of the decorated function
    """
    def inner(*args, **kwargs):
        """ Call the decorated function in the lexical scope in a retry loop
        if it gets the returned error code SA_AIS_ERR_BAD_HANDLE

        Args:
            args (tuple): Arguments of the decorated function
        """
        count_sec_sleeps = 0
        sleep_time_interval = 10 * RETRY_INTERVAL

        result = func(*args, **kwargs)
        rc = result[0] if isinstance(result, tuple) else result
        while rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            if count_sec_sleeps >= MAX_RETRY_TIME:
                break

            time.sleep(sleep_time_interval)
            count_sec_sleeps += sleep_time_interval
            result = func(*args, **kwargs)
            rc = result[0] if isinstance(result, tuple) else result

        return result

    return inner


def deprecate(func):
    """ Decorate the given function as deprecated

    A warning message to notify the users about the function deprecation will
    be displayed if the users have enabled the filter for this kind of warning

    Args:
        func (function): The deprecated function

    Returns:
        Return code/output of the decorated function
    """
    def inner(*args, **kwargs):
        """ Call the deprecated function in the lexical scope """
        warnings.warn("This function will be deprecated in future release. "
                      "Please consider using its OOP counterpart.",
                      PendingDeprecationWarning)
        return func(*args, **kwargs)

    return inner


###############################
# Common system logging utils #
###############################
def log_err(message):
    """ Print a message to syslog at ERROR level

    Args:
        message (str): Message to be printed to syslog
    """
    syslog.syslog(syslog.LOG_ERR, "ER " + message)


def log_warn(message):
    """ Print a message to syslog at WARNING level

    Args:
        message (str): Message to be printed to syslog
    """
    syslog.syslog(syslog.LOG_WARNING, "WA " + message)


def log_notice(message):
    """ Print a message to syslog at NOTICE level
    Args:
        message (str): Message to be printed to syslog
    """
    syslog.syslog(syslog.LOG_NOTICE, "NO " + message)


def log_info(message):
    """ Print a message to syslog at INFO level

    Args:
        message (str): Message to be printed to syslog
    """
    syslog.syslog(syslog.LOG_INFO, "IN " + message)


def log_debug(message):
    """ Print a message to syslog at DEBUG level

    Args:
        message (str): Message to be printed to syslog
    """
    syslog.syslog(syslog.LOG_DEBUG, "DB " + message)


def log_init(ident):
    """ Initialize system logging function

    Args:
        ident(str): A string to be prepended to each message
    """
    syslog.openlog(ident, syslog.LOG_PID, syslog.LOG_USER)
