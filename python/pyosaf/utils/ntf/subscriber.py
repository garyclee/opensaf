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
""" NTF subscriber utilities """
from __future__ import print_function
import ctypes
from copy import deepcopy

from pyosaf import saNtf
from pyosaf.saAis import BYREF, eSaAisErrorT, SaVoidPtr, SaNameT, SaUint16T, \
    SaStringT, PY3
from pyosaf.utils import bad_handle_retry, log_err, log_warn
from pyosaf.utils.ntf import agent as ntf

STRING_TYPES = [saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_STRING,
                saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_LDAP_NAME,
                saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_IPADDRESS]
INT_TYPES = [saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT8,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT8,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT16,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT16,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT32,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT32,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_FLOAT,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT64,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT64,
             saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_DOUBLE]


class NtfConsumer(ntf.NtfAgent):
    """ This class provides functions of the NTF Consumer interface """
    def __init__(self, version=None):
        """ Constructor for NtfConsumer class """
        super(NtfConsumer, self).__init__(version)
        self.filter_info = ntf.NotificationFilterInfo()
        self.filter_handles = None

    def __enter__(self):
        """ Enter method for NtfConsumer class """
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        """ Exit method for NtfConsumer class

        Finalize the NTF agent handle
        """
        if self.handle is not None:
            ntf.saNtfFinalize(self.handle)
            self.handle = None

    def __del__(self):
        """ Destructor for NtfConsumer class

        Finalize the NTF agent handle
        """
        if self.handle is not None:
            saNtf.saNtfFinalize(self.handle)
            self.handle = None

    def set_filter_event_types(self, event_type_list):
        """ Set data for the eventTypes field in the notification filter header
        of all notification types

        Args:
            event_type_list (list(SaNtfEventTypeT)): List of notification event
                types
        """
        for event_type in event_type_list:
            if event_type <= saNtf.eSaNtfEventTypeT.SA_NTF_OBJECT_DELETION:
                self.filter_info.obj_create_del_evt_list.append(event_type)
            elif event_type <= saNtf.eSaNtfEventTypeT.SA_NTF_ATTRIBUTE_RESET:
                self.filter_info.attr_change_evt_list.append(event_type)
            elif event_type <= \
                    saNtf.eSaNtfEventTypeT.SA_NTF_OBJECT_STATE_CHANGE:
                self.filter_info.state_change_evt_list.append(event_type)
            elif event_type <= saNtf.eSaNtfEventTypeT.SA_NTF_ALARM_ENVIRONMENT:
                self.filter_info.alarm_evt_list.append(event_type)
            elif event_type <= saNtf.eSaNtfEventTypeT.SA_NTF_TIME_VIOLATION:
                self.filter_info.sec_alarm_evt_list.append(event_type)

    def set_filter_notification_objects(self, notification_object_list):
        """ Set data for the notificationObjects field in the notification
        filter header of all notification types

        Args:
            notification_object_list (list(str)): List of notification
                objects' dns
        """
        self.filter_info.notification_object_list = notification_object_list

    def set_filter_notifying_objects(self, notifying_objects_list):
        """ Set data for the notifyingObjects field in the notification filter
        header of all notification types

        Args:
            notifying_objects_list (list(str)): List of notifying objects' dns
        """
        self.filter_info.notifying_objects_list = notifying_objects_list

    def set_filter_ntf_class_ids(self, ntf_class_id_list):
        """ Set data for the notificationClassIds field in the notification
        filter header of all notification types

        Args:
            ntf_class_id_list (list(SaNtfClassIdT)): List of notification
                class ids
        """
        self.filter_info.ntf_class_id_list = ntf_class_id_list

    def set_filter_source_indicators(self, source_indicators):
        """ Set data for the sourceIndicators field in the notification filter
        header of either one of object-create/delete, attribute-change,
        state-change notification types

        Args:
            source_indicators (list(SaNtfSourceIndicatorT)): List of
                notification source indicators
        """
        self.filter_info.source_indicator_list = source_indicators

    def set_filter_changed_states(self, changed_states):
        """ Set data for the changedStates field in the notification filter
        header of state-change notification type

        Args:
            changed_states (list(StateChange)): List of StateChange structures
        """
        self.filter_info.changed_state_list = changed_states

    def set_filter_probable_causes(self, probable_causes):
        """ Set data for the probableCauses field in the notification filter
        header of either of alarm or security alarm notification types

        Args:
            probable_causes (list(SaNtfProbableCauseT)): List of alarm probable
                causes
        """
        self.filter_info.probable_cause_list = probable_causes

    def set_filter_perceived_severities(self, perceived_severities):
        """ Set data for the perceivedSeverities field in the notification
        filter header of alarm notification type

        Args:
            perceived_severities (list(SaNtfSeverityT)): List of alarm
                severities
        """
        self.filter_info.perceived_severity_list = perceived_severities

    def set_filter_trends(self, trends):
        """ Set data for the trends field in the notification filter header of
        alarm notification type

        Args:
            trends (list(SaNtfSeverityTrendT)): List of alarm severity trends
        """
        self.filter_info.trend_list = trends

    def set_filter_severities(self, severities):
        """ Set data for the severities field in the notification filter header
        of security alarm notification type

        Args:
            severities (list(SaNtfSeverityT)): List of security alarm
                severities
        """
        self.filter_info.severity_list = severities

    def clear_filter_info(self):
        """ Reset the NotificationInfo filter field values """
        self.filter_info = ntf.NotificationFilterInfo()

    def _fill_in_filter_header(self, filter_header):
        """ Fill in the given notification filter header with user-provided
        data """
        for i, info in enumerate(self.filter_info.notification_object_list):
            filter_header.notificationObjects[i] = SaNameT(info)
        for i, info in enumerate(self.filter_info.notifying_objects_list):
            filter_header.notifyingObjects[i] = SaNameT(info)
        for i, info in enumerate(self.filter_info.ntf_class_id_list):
            filter_header.notificationClassIds[i] = info

    def _generate_object_create_delete_filter(self):
        """ Allocate memory for the object-create/delete notification filter
        and fill in the corresponding user-provided data

        Returns:
            SaAisErrorT: Return code of the
                saNtfObjectCreateDeleteNotificationFilterAllocate() API call
        """
        notification_filter = \
            saNtf.SaNtfObjectCreateDeleteNotificationFilterT()

        rc = ntf.saNtfObjectCreateDeleteNotificationFilterAllocate(
            self.handle, notification_filter,
            len(self.filter_info.obj_create_del_evt_list),
            len(self.filter_info.notification_object_list),
            len(self.filter_info.notifying_objects_list),
            len(self.filter_info.ntf_class_id_list),
            len(self.filter_info.source_indicator_list))
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfObjectCreateDeleteNotificationFilterAllocate"
                    " FAILED, rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            self.filter_handles.objectCreateDeleteFilterHandle = \
                notification_filter.notificationFilterHandle

            self._fill_in_filter_header(
                notification_filter.notificationFilterHeader)

            # Fill in the eventTypes array with user-provided data
            for i, evt in enumerate(self.filter_info.obj_create_del_evt_list):
                notification_filter.notificationFilterHeader.eventTypes[i] = \
                    evt

            # Fill in the sourceIndicators array with user-provided data
            for i, source_indicator in \
                    enumerate(self.filter_info.source_indicator_list):
                notification_filter.sourceIndicators[i] = source_indicator

        return rc

    def _generate_attribute_change_filter(self):
        """ Allocate memory for the attribute change notification filter and
        fill in the corresponding user-provided data

        Returns:
            SaAisErrorT: Return code of the
                saNtfAttributeChangeNotificationFilterAllocate() API call
        """
        notification_filter = saNtf.SaNtfAttributeChangeNotificationFilterT()

        rc = ntf.saNtfAttributeChangeNotificationFilterAllocate(
            self.handle, notification_filter,
            len(self.filter_info.attr_change_evt_list),
            len(self.filter_info.notification_object_list),
            len(self.filter_info.notifying_objects_list),
            len(self.filter_info.ntf_class_id_list),
            len(self.filter_info.source_indicator_list))
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfAttributeChangeNotificationFilterAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            self.filter_handles.attributeChangeFilterHandle = \
                notification_filter.notificationFilterHandle

            self._fill_in_filter_header(
                notification_filter.notificationFilterHeader)

            # Fill in the eventTypes array with user-provided data
            for i, evt in enumerate(self.filter_info.attr_change_evt_list):
                notification_filter.notificationFilterHeader.eventTypes[i] = \
                    evt

            # Fill in the sourceIndicators array with user-provided data
            for i, source_indicator in \
                    enumerate(self.filter_info.source_indicator_list):
                notification_filter.sourceIndicators[i] = source_indicator

        return rc

    def _generate_state_change_filter(self):
        """ Allocate memory for the state change notification filter and fill
        in the corresponding user-provided data

        Returns:
            SaAisErrorT: Return code of the
                saNtfStateChangeNotificationFilterAllocate() API call
        """
        notification_filter = saNtf.SaNtfStateChangeNotificationFilterT()

        rc = ntf.saNtfStateChangeNotificationFilterAllocate(
            self.handle, notification_filter,
            len(self.filter_info.state_change_evt_list),
            len(self.filter_info.notification_object_list),
            len(self.filter_info.notifying_objects_list),
            len(self.filter_info.ntf_class_id_list),
            len(self.filter_info.source_indicator_list),
            len(self.filter_info.changed_state_list))
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfStateChangeNotificationFilterAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            self.filter_handles.stateChangeFilterHandle = \
                notification_filter.notificationFilterHandle

            self._fill_in_filter_header(
                notification_filter.notificationFilterHeader)

            # Fill in the eventTypes array with user-provided data
            for i, evt in enumerate(self.filter_info.state_change_evt_list):
                notification_filter.notificationFilterHeader.eventTypes[i] = \
                    evt

            # Fill in the sourceIndicators array with user-provided data
            for i, source_indicator in \
                    enumerate(self.filter_info.source_indicator_list):
                notification_filter.sourceIndicators[i] = source_indicator

            # Fill in the changedStates array with user-provided data
            for i, state_change in \
                    enumerate(self.filter_info.changed_state_list):
                notification_filter.changedStates[i].stateId = \
                    state_change.state_id
                notification_filter.changedStates[i].newState = \
                    state_change.new_state
                notification_filter.changedStates[i].oldStatePresent = \
                    state_change.old_state_present
                if notification_filter.changedStates[i].oldStatePresent:
                    notification_filter.changedStates[i].oldState = \
                        state_change.old_state

        return rc

    def _generate_alarm_filter(self):
        """ Allocate memory for the alarm notification filter and fill in the
        corresponding user-provided data

        Returns:
            SaAisErrorT: Return code of the
                saNtfAlarmNotificationFilterAllocate() API call
        """
        notification_filter = saNtf.SaNtfAlarmNotificationFilterT()

        rc = ntf.saNtfAlarmNotificationFilterAllocate(
            self.handle, notification_filter,
            len(self.filter_info.alarm_evt_list),
            len(self.filter_info.notification_object_list),
            len(self.filter_info.notifying_objects_list),
            len(self.filter_info.ntf_class_id_list),
            len(self.filter_info.probable_cause_list),
            len(self.filter_info.perceived_severity_list),
            len(self.filter_info.trend_list))
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfAlarmNotificationFilterAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            self.filter_handles.alarmFilterHandle = \
                notification_filter.notificationFilterHandle

            self._fill_in_filter_header(
                notification_filter.notificationFilterHeader)

            # Fill in the eventTypes array with user-provided data
            for i, evt in enumerate(self.filter_info.alarm_evt_list):
                notification_filter.notificationFilterHeader.eventTypes[i] = \
                    evt

            # Fill in the probableCauses array with user-provided data
            for i, probable_cause in \
                    enumerate(self.filter_info.probable_cause_list):
                notification_filter.probableCauses[i] = probable_cause

            # Fill in the perceivedSeverities array with user-provided data
            for i, perceived_severity in \
                    enumerate(self.filter_info.perceived_severity_list):
                notification_filter.perceivedSeverities[i] = perceived_severity

            # Fill in the trends array with user-provided data
            for i, trend in enumerate(self.filter_info.trend_list):
                notification_filter.trends[i] = trend

        return rc

    def _generate_security_alarm_filter(self):
        """ Allocate memory for the security alarm notification filter and fill
        in the corresponding user-provided data

        Returns:
            SaAisErrorT: Return code of the
                saNtfSecurityAlarmNotificationFilterAllocate() API call
        """
        notification_filter = saNtf.SaNtfSecurityAlarmNotificationFilterT()

        rc = ntf.saNtfSecurityAlarmNotificationFilterAllocate(
            self.handle, notification_filter,
            len(self.filter_info.sec_alarm_evt_list),
            len(self.filter_info.notification_object_list),
            len(self.filter_info.notifying_objects_list),
            len(self.filter_info.ntf_class_id_list),
            len(self.filter_info.probable_cause_list),
            len(self.filter_info.severity_list), 0, 0, 0)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfSecurityAlarmNotificationFilterAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            self.filter_handles.securityAlarmFilterHandle = \
                notification_filter.notificationFilterHandle

            self._fill_in_filter_header(
                notification_filter.notificationFilterHeader)

            # Fill in the eventTypes array with user-provided data
            for i, evt in enumerate(self.filter_info.sec_alarm_evt_list):
                notification_filter.notificationFilterHeader.eventTypes[i] = \
                    evt

            # Fill in the probableCauses array with user-provided data
            for i, probable_cause in \
                    enumerate(self.filter_info.probable_cause_list):
                notification_filter.probableCauses[i] = probable_cause

            # Fill in the severities array with user-provided data
            for i, severity in enumerate(self.filter_info.severity_list):
                notification_filter.severities[i] = severity

        return rc

    def _free_notification_filters(self):
        """ Free the memory previously allocated for a notification filter """
        if self.filter_handles.objectCreateDeleteFilterHandle:
            ntf.saNtfNotificationFilterFree(
                self.filter_handles.objectCreateDeleteFilterHandle)
        if self.filter_handles.attributeChangeFilterHandle:
            ntf.saNtfNotificationFilterFree(
                self.filter_handles.attributeChangeFilterHandle)
        if self.filter_handles.stateChangeFilterHandle:
            ntf.saNtfNotificationFilterFree(
                self.filter_handles.stateChangeFilterHandle)
        if self.filter_handles.alarmFilterHandle:
            ntf.saNtfNotificationFilterFree(
                self.filter_handles.alarmFilterHandle)
        if self.filter_handles.securityAlarmFilterHandle:
            ntf.saNtfNotificationFilterFree(
                self.filter_handles.securityAlarmFilterHandle)

    @staticmethod
    def _get_ptr_value(ntf_handle, value):
        """ Get the correct string value from the given 'value' field in the
        received notification

        Args:
            ntf_handle (SaNtfNotificationHandleT): A handle to the internal
                notification structure
            value (SaNtfValueT): A 'value' field in the notification structure

        Returns:
            SaStringT: The value in string format
        """
        data_ptr = SaVoidPtr()
        data_size = SaUint16T()
        rc = ntf.saNtfPtrValGet(ntf_handle, value, data_ptr, data_size)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_warn("saNtfPtrValGet FAILED - %s" % (eSaAisErrorT.whatis(rc)))
        if PY3:
            return str(ctypes.cast(data_ptr, SaStringT))
        return ctypes.cast(data_ptr, SaStringT).value

    def _get_ntf_value(self, ntf_handle, value, value_type):
        """ Get the correct typed value from a specific 'value' field in the
        received notification

        Args:
            ntf_handle (SaNtfNotificationHandleT): A handle to the internal
                notification structure
            value (SaNtfValueT): A 'value' field in the notification structure
            value_type (SaNtfValueTypeT): Value type

        Returns:
            typed_value: Value of a specific type
        """
        if value_type in STRING_TYPES:
            return self._get_ptr_value(ntf_handle, value)
        elif value_type in INT_TYPES:
            return saNtf.unmarshalSaNtfValue(BYREF(value), value_type)

        return "This value type is currently not supported"

    def _parse_notification_header(self, ntf_handle, ntf_header):
        """ Parse the notification header in the received notification to
        retrieve information

        Args:
            ntf_handle (SaNtfNotificationHandleT): A handle to the internal
                notification structure
            ntf_header (SaNtfNotificationHeaderT): Notification header
                structure in the received notification

        Returns:
            NotificationInfo: A NotificationInfo structure containing
                information from the notification header
        """
        ntf_info = ntf.NotificationInfo()
        ntf_info.event_type = ntf_header.eventType.contents.value
        ntf_info.notification_object = \
            str(ntf_header.notificationObject.contents)
        ntf_info.notifying_object = str(ntf_header.notifyingObject.contents)
        ntf_info.ntf_class_id = \
            ntf_header.notificationClassId.contents
        ntf_info.event_time = ntf_header.eventTime.contents.value
        ntf_info.notification_id = \
            ntf_header.notificationId.contents.value

        additional_text_str = \
            ctypes.create_string_buffer(ntf_header.lengthAdditionalText)
        ctypes.memmove(additional_text_str, ntf_header.additionalText,
                       ntf_header.lengthAdditionalText)
        ntf_info.additional_text = additional_text_str.value.decode(
            'utf-8') if PY3 else additional_text_str.value

        for i in range(ntf_header.numAdditionalInfo):
            c_add_info = ntf_header.additionalInfo[i]
            add_info = ntf.AdditionalInfo()
            add_info.info_id = c_add_info.infoId
            add_info.info_type = c_add_info.infoType
            add_info.info_value = self._get_ntf_value(ntf_handle,
                                                      c_add_info.infoValue,
                                                      c_add_info.infoType)
            ntf_info.additional_info.append(add_info)

        return ntf_info

    def _parse_object_create_delete_ntf(self, c_ntf, ntf_info):
        """ Parse the received object-create/delete notification to retrieve
        its information

        Args:
            c_ntf (SaNtfObjectCreateDeleteNotificationT): Object-create/delete
                notification structure
            ntf_info (NotificationInfo): NotificationInfo structure with
                information from the notification
        """
        ntf_handle = c_ntf.notificationHandle
        ntf_info.source_indicator = \
            c_ntf.sourceIndicator.contents.value
        for i in range(c_ntf.numAttributes):
            c_attr = c_ntf.objectAttributes[i]
            attr = ntf.Attribute()
            attr.attribute_id = c_attr.attributeId
            attr.attribute_type = c_attr.attributeType
            attr.attribute_value = \
                self._get_ntf_value(ntf_handle, c_attr.attributeValue,
                                    c_attr.attributeType)
            ntf_info.object_attributes.append(attr)

    def _parse_attribute_change_ntf(self, c_ntf, ntf_info):
        """ Parse the received attribute-change notification to retrieve its
        information

        Args:
            c_ntf (SaNtfAttributeChangeNotificationT): Attribute-change
                notification structure
            ntf_info (NotificationInfo): NotificationInfo structure with
                information from the notification
        """
        ntf_handle = c_ntf.notificationHandle
        ntf_info.source_indicator = \
            c_ntf.sourceIndicator.contents.value
        for i in range(c_ntf.numAttributes):
            c_attr = c_ntf.changedAttributes[i]
            attr = ntf.AttributeChange()
            attr.attribute_id = c_attr.attributeId
            attr.attribute_type = c_attr.attributeType
            attr.new_attribute_value = \
                self._get_ntf_value(ntf_handle, c_attr.newAttributeValue,
                                    c_attr.attributeType)
            attr.old_attribute_present = c_attr.oldAttributePresent
            if c_attr.oldAttributePresent:
                attr.old_attribute_value = \
                    self._get_ntf_value(ntf_handle, c_attr.oldAttributeValue,
                                        c_attr.attributeType)
            ntf_info.changed_attributes.append(attr)

    @staticmethod
    def _parse_state_change_ntf(c_ntf, ntf_info):
        """ Parse the received state-change notification to retrieve its
        information

        Args:
            c_ntf (SaNtfStateChangeNotificationT): State-change notification
                structure
            ntf_info (NotificationInfo): NotificationInfo structure with
                information from the notification
        """
        ntf_info.source_indicator = \
            c_ntf.sourceIndicator.contents.value
        for i in range(c_ntf.numStateChanges):
            c_attr = c_ntf.changedStates[i]
            attr = ntf.StateChange()
            attr.state_id = c_attr.stateId
            attr.new_state = c_attr.newState
            attr.old_state_present = c_attr.oldStatePresent
            if c_attr.oldStatePresent:
                attr.old_state = c_attr.oldState
            ntf_info.state_changes.append(attr)

    def _parse_alarm_ntf(self, c_ntf, ntf_info):
        """ Parse the received alarm notification to retrieve its information

        Args:
            c_ntf (SaNtfAlarmNotificationT): Alarm notification structure
            ntf_info (NotificationInfo): NotificationInfo structure with
                information from the notification
        """
        ntf_handle = c_ntf.notificationHandle
        ntf_info.probable_cause = c_ntf.probableCause.contents.value
        ntf_info.perceived_severity = c_ntf.perceivedSeverity.contents.value
        ntf_info.trend = c_ntf.trend.contents.value

        c_threshold_info = c_ntf.thresholdInformation.contents
        if c_threshold_info:
            threshold_info = ntf.ThresholdInformation()
            c_threshold_info = c_ntf.thresholdInformation.contents

            threshold_info.threshold_id = c_threshold_info.thresholdId
            threshold_info.threshold_value_type = \
                c_threshold_info.thresholdValueType
            threshold_info.threshold_value = \
                self._get_ntf_value(ntf_handle,
                                    c_threshold_info.thresholdValue,
                                    c_threshold_info.thresholdValueType)
            threshold_info.threshold_hysteresis = \
                self._get_ntf_value(ntf_handle,
                                    c_threshold_info.thresholdHysteresis,
                                    c_threshold_info.thresholdValueType)
            threshold_info.observed_value = \
                self._get_ntf_value(ntf_handle,
                                    c_threshold_info.observedValue,
                                    c_threshold_info.thresholdValueType)
            threshold_info.arm_time = c_threshold_info.armTime

            ntf_info.threshold_information = threshold_info

        for i in range(c_ntf.numSpecificProblems):
            c_specific_problem = c_ntf.specificProblems[i]
            spec_problem = ntf.SpecificProblem()
            spec_problem.problem_id = c_specific_problem.problemId
            spec_problem.problem_class_id = c_specific_problem.problemClassId
            spec_problem.problem_type = c_specific_problem.problemType
            spec_problem.problem_value = \
                self._get_ntf_value(ntf_handle,
                                    c_specific_problem.problemValue,
                                    c_specific_problem.problemType)
            ntf_info.specific_problems.append(spec_problem)

        for i in range(c_ntf.numMonitoredAttributes):
            c_attr = c_ntf.monitoredAttributes[i]
            attr = ntf.Attribute()
            attr.attribute_id = c_attr.attributeId
            attr.attribute_type = c_attr.attributeType
            attr.attribute_value = \
                self._get_ntf_value(ntf_handle, c_attr.attributeValue,
                                    c_attr.attributeType)
            ntf_info.monitored_attrs.append(attr)

        for i in range(c_ntf.numProposedRepairActions):
            c_action = c_ntf.proposedRepairActions[i]
            action = ntf.ProposedRepairAction()
            action.action_id = c_action.actionId
            action.action_value_type = c_action.actionValueType
            action.action_value = \
                self._get_ntf_value(ntf_handle, c_action.actionValue,
                                    c_action.actionValueType)
            ntf_info.proposed_repair_actions.append(action)

    def _parse_security_alarm_ntf(self, c_ntf, ntf_info):
        """ Parse the received security alarm notification to retrieve its
        information

        Args:
            c_ntf (SaNtfSecurityAlarmNotificationT): Security alarm
                notification structure
            ntf_info (NotificationInfo): NotificationInfo structure with
                information from the notification
        """
        ntf_handle = c_ntf.notificationHandle
        ntf_info.probable_cause = c_ntf.probableCause.contents.value
        ntf_info.severity = c_ntf.severity.contents.value
        if c_ntf.securityAlarmDetector.contents:
            ntf_info.security_alarm_detector = ntf.SecurityAlarmDetector()
            ntf_info.security_alarm_detector.value_type = \
                c_ntf.securityAlarmDetector.contents.valueType
            ntf_info.security_alarm_detector.value = self._get_ntf_value(
                ntf_handle,
                c_ntf.securityAlarmDetector.contents.value,
                c_ntf.securityAlarmDetector.contents.valueType)
        if c_ntf.serviceUser.contents:
            ntf_info.service_user = ntf.SecurityAlarmDetector()
            ntf_info.service_user.value_type = \
                c_ntf.serviceUser.contents.valueType
            ntf_info.service_user.value = self._get_ntf_value(
                ntf_handle,
                c_ntf.serviceUser.contents.value,
                c_ntf.serviceUser.contents.valueType)
        if c_ntf.serviceProvider.contents:
            ntf_info.service_provider = ntf.SecurityAlarmDetector()
            ntf_info.service_provider.value_type = \
                c_ntf.serviceProvider.contents.valueType
            ntf_info.service_provider.value = self._get_ntf_value(
                ntf_handle,
                c_ntf.serviceProvider.contents.value,
                c_ntf.serviceProvider.contents.valueType)


class NtfSubscriber(NtfConsumer):
    """ This class provides functions of the NTF Subscriber interface """

    def _ntf_notif_callback(self, c_subscription_id, c_notif):
        """ This callback is invoked by NTF to deliver a notification to a
        subscriber of that notification type.

        Args:
            c_subscription_id (SaNtfSubscriptionIdT): The subscription id
                previously provided by the subscriber when subscribing for this
                type of notification
            c_notif (SaNtfNotificationsT): The notification delivered by this
                callback
        """
        if not self.ntf_notif_function:
            return

        subscription_id = c_subscription_id
        notification_type = c_notif.contents.notificationType
        if notification_type == \
                saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_OBJECT_CREATE_DELETE:
            notification = \
                c_notif.contents.notification.objectCreateDeleteNotification
            ntf_handle = notification.notificationHandle
            ntf_header = notification.notificationHeader
            ntf_info = self._parse_notification_header(ntf_handle, ntf_header)
            self._parse_object_create_delete_ntf(notification, ntf_info)
        elif notification_type == \
                saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_ATTRIBUTE_CHANGE:
            notification = \
                c_notif.contents.notification.attributeChangeNotification
            ntf_handle = notification.notificationHandle
            ntf_header = notification.notificationHeader
            ntf_info = self._parse_notification_header(ntf_handle, ntf_header)
            self._parse_attribute_change_ntf(notification, ntf_info)

        elif notification_type == \
                saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_STATE_CHANGE:
            notification = \
                c_notif.contents.notification.stateChangeNotification
            ntf_handle = notification.notificationHandle
            ntf_header = notification.notificationHeader
            ntf_info = self._parse_notification_header(ntf_handle, ntf_header)
            self._parse_state_change_ntf(notification, ntf_info)

        elif notification_type == \
                saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_ALARM:
            notification = c_notif.contents.notification.alarmNotification
            ntf_handle = notification.notificationHandle
            ntf_header = notification.notificationHeader
            ntf_info = self._parse_notification_header(ntf_handle, ntf_header)
            self._parse_alarm_ntf(notification, ntf_info)

        elif notification_type == \
                saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_SECURITY_ALARM:
            notification = \
                c_notif.contents.notification.securityAlarmNotification
            ntf_handle = notification.notificationHandle
            ntf_header = notification.notificationHeader
            ntf_info = self._parse_notification_header(ntf_handle, ntf_header)
            self._parse_security_alarm_ntf(notification, ntf_info)

        else:
            return

        # Make a deep copy of the parsed notification for use after freeing it
        ntf_info_copy = deepcopy(ntf_info)

        # Send the ntf info to user's callback function
        self.ntf_notif_function(subscription_id, notification_type,
                                ntf_info_copy)

        # Free the notification after parsing for needed information
        ntf.saNtfNotificationFree(notification.notificationHandle)

    def _ntf_notif_discarded_callback(self, c_subscription_id,
                                      c_notification_type, c_number_discarded,
                                      c_discarded_notification_identifiers):
        """ This callback is invoked by NTF to notify a subscriber of a
        particular notification type that one or more notifications of that
        type have been discarded.

        Args:
            c_subscription_id (SaNtfSubscriptionIdT): The subscription id
                previously provided by the subscriber when subscribing for
                discarded notifications
            c_notification_type (SaNtfNotificationTypeT): The notification type
                of the discarded notifications
            c_number_discarded (SaUint32T): The number of discarded
                notifications
            c_discarded_notification_identifiers (SaNtfIdentifierT): The list
                of notification identifiers of the discarded notifications
        """
        if not self.ntf_notif_discarded_function:
            return

        # Send all info to user callback
        self.ntf_notif_discarded_function(c_subscription_id,
                                          c_notification_type,
                                          c_number_discarded,
                                          c_discarded_notification_identifiers)

    @bad_handle_retry
    def _re_init(self):
        """ Internal function to re-initialize the NTF agent and fetch a new
        selection object in case of getting BAD_HANDLE during an operation

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        self.finalize()
        self.version = deepcopy(self.init_version)
        self.handle = saNtf.SaNtfHandleT()
        rc = ntf.saNtfInitialize(self.handle, self.callbacks, self.version)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfInitialize FAILED - %s" % (eSaAisErrorT.whatis(rc)))
        else:
            rc = self._fetch_sel_obj()

        return rc

    def init(self, ntf_notif_func=None, ntf_notif_discarded_func=None):
        """ Initialize the NTF agent and fetch the selection object

        Args:
            ntf_notif_func (callback): Callback function for subscribed
                notifications
            ntf_notif_discarded_func (callback): Callback function for
                discarded notifications

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        rc = self.initialize(ntf_notif_func, ntf_notif_discarded_func)
        if rc == eSaAisErrorT.SA_AIS_OK:
            rc = self._fetch_sel_obj()
            if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
                rc = self._re_init()
        return rc

    @bad_handle_retry
    def subscribe(self, subscription_id, notification_types=None):
        """ Subscribe for notifications from NTF with the types specified in
        the notification_types list. If the list is not provided, all types of
        notification are subscribed to by default.

        NOTE: Users have to set the wanted filters criteria via the set of
        set_filter_*() methods before calling this method. Otherwise, the
        filters criteria will be set with default values.

        Args:
            subscription_id (SaNtfSubscriptionIdT): Subscription id
            notification_types (list(SaNtfNotificationTypeT)): List of
                notification types

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        self.filter_handles = \
            saNtf.SaNtfNotificationTypeFilterHandlesT(0, 0, 0, 0, 0)

        create_delete = \
            saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_OBJECT_CREATE_DELETE
        attr_change = \
            saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_ATTRIBUTE_CHANGE
        state_change = \
            saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_STATE_CHANGE
        alarm = saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_ALARM
        security_alarm = \
            saNtf.eSaNtfNotificationTypeT.SA_NTF_TYPE_SECURITY_ALARM

        rc = eSaAisErrorT.SA_AIS_OK
        # Create and allocate the object create delete filter
        if notification_types is None or create_delete in notification_types:
            rc = self._generate_object_create_delete_filter()

        if rc == eSaAisErrorT.SA_AIS_OK:
            # Create and allocate the attribute change filter
            if notification_types is None or attr_change in notification_types:
                rc = self._generate_attribute_change_filter()

        if rc == eSaAisErrorT.SA_AIS_OK:
            # Create and allocate the state change filter
            if notification_types is None \
                    or state_change in notification_types:
                rc = self._generate_state_change_filter()

        if rc == eSaAisErrorT.SA_AIS_OK:
            # Create and allocate the alarm filter
            if notification_types is None or alarm in notification_types:
                rc = self._generate_alarm_filter()

        if rc == eSaAisErrorT.SA_AIS_OK:
            # Create and allocate the security alarm filter
            if notification_types is None \
                    or security_alarm in notification_types:
                rc = self._generate_security_alarm_filter()

        if rc == eSaAisErrorT.SA_AIS_OK:
            # Start subscription
            rc = ntf.saNtfNotificationSubscribe(self.filter_handles,
                                                subscription_id)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saNtfNotificationSubscribe FAILED, rc = %s" %
                        eSaAisErrorT.whatis(rc))
            else:
                self.clear_filter_info()

            # Free the notification filters
            self._free_notification_filters()

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self._re_init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    @staticmethod
    def unsubscribe(subscription_id):
        """ Unsubscribe the previous notification subscription

        Args:
            subscription_id (SaNtfSubscriptionIdT): Subscription id provided in
                the previous notification subscription

        Returns:
            SaAisErrorT: Return code of the saNtfNotificationUnsubscribe()
                API call
        """
        return ntf.saNtfNotificationUnsubscribe(subscription_id)
