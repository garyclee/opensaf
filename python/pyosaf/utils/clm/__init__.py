############################################################################
#
# (C) Copyright 2015 The OpenSAF Foundation
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
# Author(s): Ericsson
#
############################################################################
# pylint: disable=unused-argument
""" CLM common utilities """
from copy import deepcopy

from pyosaf.saAis import saAis, SaVersionT, eSaAisErrorT, eSaDispatchFlagsT
from pyosaf import saClm
from pyosaf.utils import decorate, initialize_decorate, deprecate, \
    bad_handle_retry, log_err, SafException

_clm_agent = None

# Decorate pure saClm* API's with error-handling retry and exception raising
saClmInitialize = initialize_decorate(saClm.saClmInitialize)
saClmInitialize_3 = initialize_decorate(saClm.saClmInitialize_3)
saClmInitialize_4 = initialize_decorate(saClm.saClmInitialize_4)
saClmSelectionObjectGet = decorate(saClm.saClmSelectionObjectGet)
saClmDispatch = decorate(saClm.saClmDispatch)
saClmFinalize = decorate(saClm.saClmFinalize)
saClmClusterTrack = decorate(saClm.saClmClusterTrack)
saClmClusterTrack_4 = decorate(saClm.saClmClusterTrack_4)
saClmClusterTrackStop = decorate(saClm.saClmClusterTrackStop)
saClmClusterNotificationFree = decorate(saClm.saClmClusterNotificationFree)
saClmClusterNotificationFree_4 = decorate(saClm.saClmClusterNotificationFree_4)
saClmClusterNodeGet = decorate(saClm.saClmClusterNodeGet)
saClmClusterNodeGet_4 = decorate(saClm.saClmClusterNodeGet_4)
saClmClusterNodeGetAsync = decorate(saClm.saClmClusterNodeGetAsync)
saClmResponse_4 = decorate(saClm.saClmResponse_4)


class ClusterNode(object):
    """ Class representing a CLM cluster node """

    def __init__(self, node_id, node_address, node_name, execution_environment,
                 member, boot_timestamp, initial_view_number):
        """ Constructor for ClusterNode class

        Args:
            node_id (SaClmNodeIdT): Node identifier
            node_address (SaClmNodeAddressT): Node network communication
                address
            node_name (SaNameT): Node name
            execution_environment (SaNameT): DN of the PLM execution
                environment that hosts the node
            member (SaBoolT): Indicate whether this is a CLM member node (True)
                or not (False)
            boot_timestamp (SaTimeT): Timestamp at which the node was last
                booted
            initial_view_number (SaUint64T): View number of the latest
                membership transition when this configured node joined the
                cluster membership
        """
        self.node_id = node_id
        self.node_address_value = str(node_address)
        self.node_address_family = node_address.family
        self.node_name = str(node_name)
        self.execution_environment = execution_environment
        self.member = member
        self.boot_timestamp = boot_timestamp
        self.initial_view_number = initial_view_number


class ClmAgentManager(object):
    """ This class manages the life cycle of a CLM agent, and also acts as
    a proxy handler for CLM callbacks """

    def __init__(self, version=None):
        """ Constructor for ClmAgentManager class

        Args:
            version (SaVersionT): CLM API version
        """
        self.init_version = version
        self.version = None
        self.handle = None
        self.callbacks = None
        self.sel_obj = saClm.SaSelectionObjectT()
        self.track_function = None
        self.node_get_function = None

    def _track_callback(self, c_notification_buffer, c_number_of_members,
                        c_invocation_id, c_root_cause_entity,
                        c_correlation_ids, c_step, c_time_supervision,
                        c_error):
        """ This callback is invoked by CLM to notify the subscribed cluster
        membership tracker about changes in the cluster membership, along with
        detailed information of the changes.

        Args:
            c_notification_buffer (SaClmClusterNotificationBufferT_4):
                A pointer to a structure containing pointer to an array of
                information structures about current member nodes and their
                membership changes if any
            c_number_of_members (SaUint32T): The current number of member nodes
            c_invocation_id (SaInvocationT): Invocation id identifying this
                particular callback invocation, and which shall be used in
                saClmResponse_4() to respond to CLM in certain cases
            c_root_cause_entity (SaNameT): A pointer to the DN of the CLM node
                directly targeted by the action or event that caused the
                membership change
            c_correlation_ids (SaNtfCorrelationIdsT): A pointer to the
                correlation identifiers associated with the root cause
            c_step (SaClmChangeStepT): The tracking step in which this callback
                is invoked
            c_time_supervision (SaTimeT): The time specifying how long CLM will
                wait for the process to provide the response for the callback
                by invoking the saClmResponse_4() function
            c_error (SaAisErrorT): Return code to indicate whether CLM was able
                to perform the requested operation
        """
        if self.track_function is not None:
            invocation = c_invocation_id
            step = c_step
            num_of_members = c_number_of_members
            error = c_error
            # List of tuples (ClusterNode, clusterChange)
            node_list = []

            notification_buffer = c_notification_buffer.contents
            for i in range(notification_buffer.numberOfItems):
                notification = notification_buffer.notification[i]
                clm_cluster_node = notification.clusterNode
                cluster_node = \
                    self.create_cluster_node_instance(clm_cluster_node)
                node_state = (cluster_node, notification.clusterChange)

                node_list.append(node_state)

            # Send the node list to user's callback function
            self.track_function(node_list, invocation, step, num_of_members,
                                error)

    def _node_get_callback(self, c_invocation_id, c_cluster_node, c_error):
        """ This callback is invoked by CLM to return information about the
        requested member node to a registered client that had previously called
        saClmClusterNodeGetAsync().

        Args:
            c_invocation_id(SaInvocationT): Invocation id associating this
                callback invocation with the corresponding previous invocation
                of saClmClusterNodeGetAsync()
            c_cluster_node(SaClmClusterNodeT_4): A pointer to the structure
                that contains information about the requested member node
            c_error(SaAisErrorT): Return code to indicate whether the
                saClmClusterNodeGetAsync() function was successful
        """
        if self.node_get_function is not None:
            invocation = c_invocation_id
            error = c_error

            cluster_node = \
                self.create_cluster_node_instance(c_cluster_node.contents)

            # Send the node info to user's callback function
            self.node_get_function(invocation, cluster_node, error)

    def initialize(self, track_func=None, node_get_func=None):
        """ Initialize the CLM agent library

        Args:
            track_func (callback): Cluster track callback function
            node_get_func (callback): Cluster node get function

        Returns:
            SaAisErrorT: Return code of the saClmInitialize_4() API call
        """
        self.track_function = track_func
        self.node_get_function = node_get_func

        self.callbacks = saClm.SaClmCallbacksT_4()
        self.callbacks.saClmClusterTrackCallback = \
            saClm.SaClmClusterTrackCallbackT_4(self._track_callback)
        self.callbacks.saClmClusterNodeGetCallback = \
            saClm.SaClmClusterNodeGetCallbackT_4(
                self._node_get_callback)

        self.handle = saClm.SaClmHandleT()
        self.version = deepcopy(self.init_version)
        rc = saClmInitialize_4(self.handle, self.callbacks, self.version)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmInitialize_4 FAILED - %s" % eSaAisErrorT.whatis(rc))

        return rc

    def get_handle(self):
        """ Return the CLM agent handle

        Returns:
            SaClmHandleT: CLM agent handle
        """
        return self.handle

    def _fetch_sel_obj(self):
        """ Obtain a selection object (OS file descriptor)

        Returns:
            SaAisErrorT: Return code of the saClmSelectionObjectGet() API call
        """
        rc = saClmSelectionObjectGet(self.handle, self.sel_obj)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmSelectionObjectGet FAILED - %s" %
                    eSaAisErrorT.whatis(rc))

        return rc

    def get_selection_object(self):
        """ Return the selection object associating with the CLM handle

        Returns:
            SaSelectionObjectT: Selection object associated with the CLM handle
        """
        return self.sel_obj

    def dispatch(self, flags):
        """ Invoke CLM callbacks for queued events

        Args:
            flags (eSaDispatchFlagsT): Flags specifying dispatch mode

        Returns:
            SaAisErrorT: Return code of the saClmDispatch() API call
        """
        rc = saClmDispatch(self.handle, flags)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmDispatch FAILED - %s" % eSaAisErrorT.whatis(rc))

        return rc

    def finalize(self):
        """ Finalize the CLM agent handle

        Returns:
            SaAisErrorT: Return code of the saClmFinalize() API call
        """
        rc = eSaAisErrorT.SA_AIS_OK
        if self.handle is not None:
            rc = saClmFinalize(self.handle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saClmFinalize FAILED - %s" % eSaAisErrorT.whatis(rc))
            if rc == eSaAisErrorT.SA_AIS_OK \
                    or rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                # If the Finalize() call returned BAD_HANDLE, the handle should
                # already become stale and invalid, so we reset it anyway.
                self.handle = None

        return rc

    @staticmethod
    def create_cluster_node_instance(clm_cluster_node):
        """ Create ClusterNode object from cluster node information

        Args:
            clm_cluster_node (SaClmClusterNodeT): Cluster node information

        Returns:
            ClusterNode: An object containing cluster node information
        """
        return ClusterNode(
            node_id=clm_cluster_node.nodeId,
            node_address=clm_cluster_node.nodeAddress,
            node_name=clm_cluster_node.nodeName,
            execution_environment=clm_cluster_node.executionEnvironment,
            member=clm_cluster_node.member,
            boot_timestamp=clm_cluster_node.bootTimestamp,
            initial_view_number=clm_cluster_node.initialViewNumber)


class ClmAgent(ClmAgentManager):
    """ This class acts as a high-level CLM agent, providing CLM functions to
    the users at a higher level, and relieving the users of the need to manage
    the life cycle of the CLM agent """

    def __init__(self, version=None):
        """ Constructor for ClmAgent class

        Args:
            version (SaVersionT): CLM API version
        """
        self.init_version = version if version is not None \
            else SaVersionT('B', 4, 1)
        super(ClmAgent, self).__init__(self.init_version)

    def __enter__(self):
        """ Enter method for ClmAgent class

        Finalize the CLM agent handle
        """
        return self

    def __exit__(self, exit_type, exit_value, traceback):
        """ Exit method for ClmAgent class

        Finalize the CLM agent handle
        """
        if self.handle is not None:
            saClmFinalize(self.handle)
            self.handle = None

    def __del__(self):
        """ Destructor for ClmAgent class

        Finalize the CLM agent handle
        """
        if self.handle is not None:
            saClmFinalize(self.handle)
            self.handle = None

    @bad_handle_retry
    def _re_init(self):
        """ Internal function to re-initialize the CLM agent in case of getting
        BAD_HANDLE during an operation

        Returns:
            SaAisErrorT: Return code of the corresponding CLM API call(s)
        """
        self.finalize()
        self.handle = saClm.SaClmHandleT()
        self.version = deepcopy(self.init_version)
        rc = saClmInitialize_4(self.handle, self.callbacks, self.version)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmInitialize_4 FAILED - %s" % eSaAisErrorT.whatis(rc))
        else:
            rc = self._fetch_sel_obj()

        return rc

    def init(self, track_func=None, node_get_func=None):
        """ Initialize the CLM agent and fetch the selection object

        Args:
            track_func (callback): Cluster track callback function
            node_get_func (callback): Cluster node get callback function

        Returns:
            SaAisErrorT: Return code of the corresponding CLM API call(s)
        """
        rc = self.initialize(track_func, node_get_func)
        if rc == eSaAisErrorT.SA_AIS_OK:
            rc = self._fetch_sel_obj()
            if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                rc = self._re_init()

        return rc

    @bad_handle_retry
    def get_members(self):
        """ Obtain information of each CLM cluster member node

        Returns:
            SaAisErrorT: Return code of the corresponding CLM API call(s)
            list(ClusterNode): The list of ClusterNode structures containing
                information of each CLM member node
        """
        cluster_nodes = []

        notification_buffer = saClm.SaClmClusterNotificationBufferT_4()

        rc = saClmClusterTrack_4(self.handle, saAis.SA_TRACK_CURRENT,
                                 notification_buffer)
        if rc == eSaAisErrorT.SA_AIS_OK:
            for i in range(notification_buffer.numberOfItems):
                notification = notification_buffer.notification[i]
                clm_cluster_node = notification.clusterNode

                cluster_node = \
                    self.create_cluster_node_instance(clm_cluster_node)

                cluster_nodes.append(cluster_node)
        else:
            log_err("saClmClusterTrack_4 FAILED - %s" %
                    eSaAisErrorT.whatis(rc))

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self._re_init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc, cluster_nodes

    @bad_handle_retry
    def track_start(self, flags):
        """ Start cluster membership tracking with the specified track flags

        Args:
            flags (SaUint8T): Type of cluster membership tracking

        Returns:
            SaAisErrorT: Return code of the corresponding CLM API call(s)
        """
        rc = saClmClusterTrack_4(self.handle, flags, None)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmClusterTrack_4 FAILED - %s" %
                    eSaAisErrorT.whatis(rc))

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self._re_init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    def track_stop(self):
        """ Stop cluster membership tracking

        Returns:
            SaAisErrorT: Return code of the saClmClusterTrackStop() API call
        """
        rc = saClmClusterTrackStop(self.handle)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmClusterTrackStop FAILED - %s" %
                    eSaAisErrorT.whatis(rc))

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self._re_init()
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc
        # No need to retry in case of BAD_HANDLE since the tracking should
        # have already been stopped when the agent disconnected
        return rc

    def response(self, invocation, result):
        """ Respond to CLM the result of execution of the requested callback

        Args:
            invocation (SaInvocationT): Invocation id associated with the
                callback
            result (SaAisErrorT): Result of callback execution

        Returns:
            SaAisErrorT: Return code of the saClmResponse_4() API call
        """
        rc = saClmResponse_4(self.handle, invocation, result)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmResponse_4 FAILED - %s" % eSaAisErrorT.whatis(rc))

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self._re_init()
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc
            # No need to retry in case of BAD_HANDLE since the corresponding
            # callback response will no longer be valid with the new
            # re-initialized handle

        return rc

    def get_node_info(self, node_id, time_out):
        """ Get information of a specific cluster member node.

        Args:
            node_id (SaClmNodeIdT): Node id
            time_out (SaTimeT): Time-out (in nanosecond) for
                saClmClusterNodeGet_4 operation

        Returns:
            SaAisErrorT: Return code of the saClmClusterNodeGet_4() API call.
            ClusterNode: Node info or None if node with `node id` is not found.
        """
        clm_cluster_node = saClm.SaClmClusterNodeT_4()

        rc = saClmClusterNodeGet_4(self.handle, node_id, time_out,
                                   clm_cluster_node)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saClmClusterNodeGet() FAILED - %s" %
                    eSaAisErrorT.whatis(rc))
            return rc, None

        cluster_node = self.create_cluster_node_instance(clm_cluster_node)
        return rc, cluster_node


@deprecate
def initialize(track_func=None, node_get_func=None, version=None):
    """ Initialize the CLM agent library

    Args:
        track_func (callback): Cluster track callback function
        node_get_func (callback): Cluster node get callback function
        version (SaVersionT): Clm version being initialized

    Raises:
        SafException: If the return code of the corresponding CLM API call(s)
            is not SA_AIS_OK
    """
    global _clm_agent
    _clm_agent = ClmAgent(version)

    rc = _clm_agent.initialize(track_func, node_get_func)
    if rc != eSaAisErrorT.SA_AIS_OK:
        raise SafException(rc)


def get_handle():
    """ Get this CLM agent handle

    Returns:
        SaClmHandleT: CLM agent handle if one was successfully initialized.
            Otherwise, 'None' is returned.
    """
    if _clm_agent is not None:
        return _clm_agent.get_handle()

    return None


@deprecate
def track(flags=saAis.SA_TRACK_CHANGES_ONLY):
    """ Start cluster membership tracking with specified flags

    Args:
        flags (SaUint8T): Type of cluster membership tracking

    Raises:
        SafException: If the return code of the corresponding CLM API call(s)
            is not SA_AIS_OK
    """
    if _clm_agent is None:
        # Return SA_AIS_ERR_INIT if user calls this function without first
        # calling initialize()
        return eSaAisErrorT.SA_AIS_ERR_INIT

    rc = _clm_agent.track_start(flags)
    if rc != eSaAisErrorT.SA_AIS_OK:
        raise SafException(rc)


@deprecate
def track_stop():
    """ Stop cluster membership tracking

    Raises:
        SafException: If the return code of the corresponding CLM API call(s)
            is not SA_AIS_OK
    """
    if _clm_agent is None:
        # Return SA_AIS_ERR_INIT if user calls this function without first
        # calling initialize()
        raise SafException(eSaAisErrorT.SA_AIS_ERR_INIT)

    rc = _clm_agent.track_stop()
    if rc != eSaAisErrorT.SA_AIS_OK:
        raise SafException(rc)


@deprecate
def get_members():
    """ Get member notifications from notification buffer

    Returns:
        ClusterNode: The node information

    Raises:
        SafException: If the return code of the corresponding CLM API call(s)
            is not SA_AIS_OK
    """
    if _clm_agent is None:
        # SA_AIS_ERR_INIT is returned if user calls this function without first
        # calling initialize()
        rc = eSaAisErrorT.SA_AIS_ERR_INIT
    else:
        rc, cluster_nodes = _clm_agent.get_members()

    if rc != eSaAisErrorT.SA_AIS_OK:
        raise SafException(rc)

    return cluster_nodes


@deprecate
def get_selection_object():
    """ Get the selection object associated with this CLM agent

    Returns:
        SaSelectionObjectT: Selection object associated with the CLM handle

    Raises:
        SafException: If the return code of the corresponding CLM API call(s)
            is not SA_AIS_OK
    """
    if _clm_agent is None:
        # SA_AIS_ERR_INIT is returned if user calls this function without first
        # calling initialize()
        raise SafException(eSaAisErrorT.SA_AIS_ERR_INIT)

    return _clm_agent.get_selection_object()


@deprecate
def dispatch(flags=eSaDispatchFlagsT.SA_DISPATCH_ALL):
    """ Invoke CLM callbacks for queued events with the given dispatch flag.
    If no dispatch flag is specified, the default is to dispatch all available
    events.

    Args:
        flags (eSaDispatchFlagsT): Flags specifying dispatch mode

    Raises:
        SafException: If the return code of the corresponding CLM API call(s)
            is not SA_AIS_OK
    """
    if _clm_agent is None:
        # SA_AIS_ERR_INIT is returned if user calls this function without first
        # calling initialize()
        rc = eSaAisErrorT.SA_AIS_ERR_INIT
    else:
        rc = _clm_agent.dispatch(flags)
    if rc != eSaAisErrorT.SA_AIS_OK:
        raise SafException(rc)


@deprecate
def response(invocation, result):
    """ Respond to CLM the result of execution of the requested callback

    Args:
        invocation (SaInvocationT): Invocation id associated with the callback
        result (SaAisErrorT): Result of callback execution

    Raises:
        SafException: If the return code of the corresponding CLM API call(s)
            is not SA_AIS_OK
    """
    if _clm_agent is None:
        # SA_AIS_ERR_INIT is returned if user calls this function without first
        # calling initialize()
        rc = eSaAisErrorT.SA_AIS_ERR_INIT
    else:
        rc = _clm_agent.response(invocation, result)
    if rc != eSaAisErrorT.SA_AIS_OK:
        raise SafException(rc)
