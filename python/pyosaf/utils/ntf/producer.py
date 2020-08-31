############################################################################
#
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
# pylint: disable=too-many-public-methods
""" NTF producer utilities """
import ctypes

from pyosaf import saNtf
from pyosaf.saNtf import eSaNtfValueTypeT
from pyosaf.saAis import eSaBoolT, eSaAisErrorT, SaVoidPtr, SaNameT, PY3
from pyosaf.utils import log_warn, log_err, bad_handle_retry
from pyosaf.utils.ntf import agent as ntf


class NtfProducer(ntf.NtfAgent):
    """ This class provides functions of the NTF Producer interface """

    def __init__(self, version=None):
        """ Constructor for NtfProducer class """
        super(NtfProducer, self).__init__(version)
        self.ntf_info = ntf.NotificationInfo()

    def init(self):
        """ Initialize the NTF agent

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        # This finalize() is needed for re-init case
        self.finalize()
        return self.initialize()

    @staticmethod
    def _assign_ntf_value(ntf_handle, attr_value, value, value_type):
        """ Assign the correct sub-field in the given attribute

        Args:
            ntf_handle (SaNtfNotificationHandleT): Notification handle
            attr_value (SaNtfValueT): Object attribute value in an object
                creation or deletion notification
            value (Any type of eSaNtfValueTypeT): Actual value of the object
                attribute
            value_type (SaNtfValueTypeT): Type of the object attribute value
        """
        if value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT8:
            attr_value.uint8Val = int(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT8:
            attr_value.int8Val = value

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT16:
            attr_value.uint16Val = int(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT16:
            attr_value.int16Val = int(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT32:
            attr_value.uint32Val = int(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT32:
            attr_value.int32Val = int(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_FLOAT:
            attr_value.floatVal = float(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_UINT64:
            attr_value.uint64Val = int(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_INT64:
            attr_value.int64Val = int(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_DOUBLE:
            attr_value.doubleVal = float(value)

        elif value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_LDAP_NAME \
                or value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_STRING \
                or value_type == saNtf.eSaNtfValueTypeT.SA_NTF_VALUE_IPADDRESS:
            if PY3:
                value = value.encode('utf-8')
            len_value = len(value)
            dest_ptr = SaVoidPtr()
            rc = ntf.saNtfPtrValAllocate(ntf_handle, len_value + 1, dest_ptr,
                                         attr_value)
            if rc == eSaAisErrorT.SA_AIS_OK:
                ctypes.memmove(dest_ptr, value, len_value + 1)
            else:
                log_warn("saNtfPtrValAllocate FAILED, rc = %s" %
                         eSaAisErrorT.whatis(rc))

    def set_event_type(self, event_type):
        """ Fill in the eventType field in the notification header

        Args:
            event_type (SaNtfEventTypeT): Event type
        """
        self.ntf_info.event_type = event_type

    def set_notification_object(self, notification_object):
        """ Fill in the notificationObject field in the notification header

        Args:
            notification_object (str): Notification object's dn
        """
        self.ntf_info.notification_object = notification_object

    def set_notifying_object(self, notifying_object):
        """ Fill in the notifyingObject field in the notification header

        Args:
            notifying_object (str): Notifying object's dn
        """
        self.ntf_info.notifying_object = notifying_object

    def set_class_id(self, notif_class_id):
        """ Fill in the notificationClassId field in the notification header

        Args:
            notif_class_id (SaNtfClassIdT): Notification class id
        """
        self.ntf_info.ntf_class_id = notif_class_id

    def set_event_time(self, event_time):
        """ Fill in the eventTime field in the notification header

        Args:
            event_time (SaTimeT): Event time
        """
        self.ntf_info.event_time = event_time

    def set_additional_text(self, additional_text):
        """ Fill in the additionalText field in the notification header

        Args:
            additional_text (str): Additional text
        """
        self.ntf_info.additional_text = additional_text

    def set_additional_info(self, additional_info):
        """ Fill in the additionalInfo field in the notification header

        Args:
            additional_info (list(AdditionalInfo)): List of AdditionalInfo
                structures
        """
        self.ntf_info.additional_info = additional_info

    def set_source_indicator(self, source_indicator):
        """ Fill in the sourceIndicator field in either object-create/delete
        notification, attribute-change notification, or state-change
        notification

        Args:
            source_indicator (SaNtfSourceIndicatorT): Source indicator of the
                notification
        """
        self.ntf_info.source_indicator = source_indicator

    def set_object_attributes(self, attributes):
        """ Fill in the objectAttributes field in object-create/delete
        notification

        Args:
            attributes (list(Attribute)): List of Attribute structures
        """
        self.ntf_info.object_attributes = attributes

    def set_changed_attributes(self, changed_attributes):
        """ Fill in the changedAttributes field in attribute-change
        notification

        Args:
            changed_attributes(list(AttributeChange)): List of AttributeChange
                structures
        """
        self.ntf_info.changed_attributes = changed_attributes

    def set_state_changes(self, state_changes):
        """ Fill in the changedStates field in state-change notification

        Args:
            state_changes (list(StateChange)): List of StateChange structures
        """
        self.ntf_info.state_changes = state_changes

    def set_probable_cause(self, probable_cause):
        """ Fill in the probableCause field in either alarm notification or
        security-alarm notification

        Args:
            probable_cause (SaNtfProbableCauseT): Probable cause of the alarm
        """
        self.ntf_info.probable_cause = probable_cause

    def set_specific_problems(self, specific_problems):
        """ Fill in the specificProblems field in alarm notification

        Args:
            specific_problems (list(SpecificProblem)): List of SpecificProblem
                structures
        """
        self.ntf_info.specific_problems = specific_problems

    def set_perceived_severity(self, perceived_severity):
        """ Fill in the perceivedSeverity field in alarm notification

        Args:
            perceived_severity (SaNtfSeverityT): Severity of the alarm
        """
        self.ntf_info.perceived_severity = perceived_severity

    def set_trend(self, trend):
        """ Fill in the trend field in alarm notification

        Args:
            trend (SaNtfSeverityTrendT): Trend of alarm severity
        """
        self.ntf_info.trend = trend

    def set_threshold_information(self, threshold_information):
        """ Fill in the thresholdInformation field in alarm notification

        Args:
            threshold_information (ThresholdInformation):
                A ThresholdInformation structure
        """
        self.ntf_info.threshold_information = threshold_information

    def set_monitored_attributes(self, monitored_attributes):
        """ Fill in the monitoredAttributes field in alarm notification

        Args:
            monitored_attributes (list(Attribute)): List of monitored
                Attribute structures
        """
        self.ntf_info.monitored_attrs = monitored_attributes

    def set_proposed_repair_actions(self, proposed_repair_action):
        """ Fill in the proposedRepairActions in alarm notification

        Args:
            proposed_repair_action (list(ProposedRepairAction)): List of
                ProposedRepairAction structures
        """
        self.ntf_info.proposed_repair_actions = proposed_repair_action

    def set_severity(self, severity):
        """ Fill in the severity field in security alarm notification

        Args:
            severity (SaNtfSeverityT): Severity of the security alarm
        """
        self.ntf_info.severity = severity

    def set_security_alarm_detector(self, security_alarm_detector):
        """ Fill in the securityAlarmDetector field in security alarm
        notification

        Args:
            security_alarm_detector (SecurityAlarmDetector):
                A SecurityAlarmDetector structure
        """
        self.ntf_info.security_alarm_detector = security_alarm_detector

    def set_service_user(self, service_user):
        """ Fill in the serviceUser field in security alarm notification

        Args:
            service_user (ServiceUser): A ServiceUser structure
        """
        self.ntf_info.service_user = service_user

    def set_service_provider(self, service_provider):
        """ Fill in the serviceProvider field in security alarm notification

        Args:
            service_provider (ServiceProvider): A ServiceProvider structure
        """
        self.ntf_info.service_provider = service_provider

    def clear_info(self):
        """ Reset the NotificationInfo field values """
        self.ntf_info = ntf.NotificationInfo()

    def _fill_in_header(self, ntf_handle, header):
        """ Fill in the given notification header with the provided information

        Args:
            ntf_handle (SaNtfNotificationHandleT): Notification handle
            header (SaNtfNotificationHeaderT): Notification header
        """
        header.eventType.contents.value = self.ntf_info.event_type

        header.notificationObject[0] = \
            SaNameT(self.ntf_info.notification_object)

        header.notifyingObject[0] = SaNameT(self.ntf_info.notifying_object)

        header.notificationClassId.contents.vendorId = \
            self.ntf_info.ntf_class_id.vendorId
        header.notificationClassId.contents.majorId = \
            self.ntf_info.ntf_class_id.majorId
        header.notificationClassId.contents.minorId = \
            self.ntf_info.ntf_class_id.minorId

        header.eventTime.contents.value = self.ntf_info.event_time

        additional_text = self.ntf_info.additional_text
        if PY3:
            additional_text = self.ntf_info.additional_text.encode('utf-8')
        header.lengthAdditionalText = len(additional_text)
        ctypes.memmove(header.additionalText,
                       additional_text,
                       len(additional_text) + 1)

        if self.ntf_info.additional_info:
            for i, add_info in enumerate(self.ntf_info.additional_info):
                header.additionalInfo[i].infoId = add_info.info_id
                header.additionalInfo[i].infoType = add_info.info_type

                self._assign_ntf_value(ntf_handle,
                                       header.additionalInfo[i].infoValue,
                                       add_info.info_value, add_info.info_type)

    @bad_handle_retry
    def send_object_create_delete_notification(self):
        """ Send an SaNtfObjectCreateDeleteNotificationT notification

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        # Create the notification
        notification = saNtf.SaNtfObjectCreateDeleteNotificationT()
        rc = ntf.saNtfObjectCreateDeleteNotificationAllocate(
            self.handle, notification, 0,
            len(self.ntf_info.additional_text) + 1,
            len(self.ntf_info.additional_info),
            len(self.ntf_info.object_attributes),
            saNtf.saNtf.SA_NTF_ALLOC_SYSTEM_LIMIT)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfObjectCreateDeleteNotificationAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            # Fill in the header
            self._fill_in_header(notification.notificationHandle,
                                 notification.notificationHeader)

            # Fill in the notification
            notification.sourceIndicator.contents.value = \
                self.ntf_info.source_indicator

            for i, attribute in enumerate(self.ntf_info.object_attributes):
                ptr = notification.objectAttributes[i]

                ptr.attributeId = attribute.attribute_id
                ptr.attributeType = attribute.attribute_type

                self._assign_ntf_value(notification.notificationHandle,
                                       ptr.attributeValue,
                                       attribute.attribute_value,
                                       attribute.attribute_type)

            # Send the notification
            rc = ntf.saNtfNotificationSend(notification.notificationHandle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saNtfNotificationSend FAILED, rc = %s" %
                        eSaAisErrorT.whatis(rc))

            # Free the notification
            ntf.saNtfNotificationFree(notification.notificationHandle)

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    @bad_handle_retry
    def send_attribute_change_notification(self):
        """ Send an SaNtfAttributeChangeNotificationT notification

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        # Create the notification
        notification = saNtf.SaNtfAttributeChangeNotificationT()

        rc = ntf.saNtfAttributeChangeNotificationAllocate(
            self.handle, notification, 0,
            len(self.ntf_info.additional_text) + 1,
            len(self.ntf_info.additional_info),
            len(self.ntf_info.changed_attributes),
            saNtf.saNtf.SA_NTF_ALLOC_SYSTEM_LIMIT)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfAttributeChangeNotificationAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            # Fill in the header
            self._fill_in_header(notification.notificationHandle,
                                 notification.notificationHeader)

            # Fill in the notification
            notification.sourceIndicator.contents.value = \
                self.ntf_info.source_indicator

            for i, changed_attr in enumerate(self.ntf_info.changed_attributes):
                ptr = notification.changedAttributes[i]

                ptr.attributeId = changed_attr.attribute_id
                ptr.attributeType = changed_attr.attribute_type
                self._assign_ntf_value(
                    notification.notificationHandle, ptr.newAttributeValue,
                    changed_attr.new_attribute_value,
                    changed_attr.attribute_type)

                if changed_attr.old_attribute_present:
                    ptr.oldAttributePresent = eSaBoolT.SA_TRUE

                    self._assign_ntf_value(notification.notificationHandle,
                                           ptr.oldAttributeValue,
                                           changed_attr.old_attribute_value,
                                           changed_attr.attribute_type)
                else:
                    ptr.oldAttributePresent = eSaBoolT.SA_FALSE

            # Send the notification
            rc = ntf.saNtfNotificationSend(notification.notificationHandle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saNtfNotificationSend FAILED, rc = %s" %
                        eSaAisErrorT.whatis(rc))

            # Free the notification
            ntf.saNtfNotificationFree(notification.notificationHandle)

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    @bad_handle_retry
    def send_state_change_notification(self):
        """ Send an SaNtfStateChangeNotificationT notification

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        # Create the notification
        notification = saNtf.SaNtfStateChangeNotificationT()

        rc = ntf.saNtfStateChangeNotificationAllocate(
            self.handle, notification, 0,
            len(self.ntf_info.additional_text) + 1,
            len(self.ntf_info.additional_info),
            len(self.ntf_info.state_changes),
            saNtf.saNtf.SA_NTF_ALLOC_SYSTEM_LIMIT)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfStateChangeNotificationAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            # Fill in the header
            self._fill_in_header(notification.notificationHandle,
                                 notification.notificationHeader)

            # Fill in the notification
            notification.sourceIndicator.contents.value = \
                self.ntf_info.source_indicator

            for i, state_change in enumerate(self.ntf_info.state_changes):
                ptr = notification.changedStates[i]

                ptr.stateId = state_change.state_id
                ptr.newState = state_change.new_state

                if state_change.old_state_present:
                    ptr.oldStatePresent = eSaBoolT.SA_TRUE
                    ptr.oldState = state_change.old_state
                else:
                    ptr.oldStatePresent = eSaBoolT.SA_FALSE

            # Send the notification
            rc = ntf.saNtfNotificationSend(notification.notificationHandle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saNtfNotificationSend FAILED, rc = %s" %
                        eSaAisErrorT.whatis(rc))

            # Free the notification
            ntf.saNtfNotificationFree(notification.notificationHandle)

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    @bad_handle_retry
    def send_alarm_notification(self):
        """ Send an SaNtfAlarmNotificationT notification

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        # Create the notification
        notification = saNtf.SaNtfAlarmNotificationT()
        rc = ntf.saNtfAlarmNotificationAllocate(
            self.handle, notification, 0,
            len(self.ntf_info.additional_text) + 1,
            len(self.ntf_info.additional_info),
            len(self.ntf_info.specific_problems),
            len(self.ntf_info.monitored_attrs),
            len(self.ntf_info.proposed_repair_actions),
            saNtf.saNtf.SA_NTF_ALLOC_SYSTEM_LIMIT)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfAlarmNotificationAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            # Fill in the header
            self._fill_in_header(notification.notificationHandle,
                                 notification.notificationHeader)

            # Fill in the notification
            notification.probableCause.contents.value = \
                self.ntf_info.probable_cause

            notification.perceivedSeverity.contents.value = \
                self.ntf_info.perceived_severity

            if self.ntf_info.trend is not None:
                notification.trend.contents.value = self.ntf_info.trend

            for i, problem in enumerate(self.ntf_info.specific_problems):
                ptr = notification.specificProblems[i]
                ptr.problemId = problem.problem_id
                if problem.problem_class_id is not None:
                    prob_class_id = problem.problem_class_id
                    ptr.problemClassId.vendorId = prob_class_id.vendorId
                    ptr.problemClassId.majorId = prob_class_id.majorId
                    ptr.problemClassId.minorId = prob_class_id.minorId
                ptr.problemType = problem.problem_type
                self._assign_ntf_value(
                    notification.notificationHandle, ptr.problemValue,
                    problem.problem_value, problem.problem_type)

            if self.ntf_info.threshold_information is not None:
                ptr = notification.thresholdInformation.contents
                ptr.thresholdId = \
                    self.ntf_info.threshold_information.threshold_id
                ptr.thresholdValueType = \
                    self.ntf_info.threshold_information.threshold_value_type

                self._assign_ntf_value(
                    notification.notificationHandle, ptr.thresholdValue,
                    self.ntf_info.threshold_information.threshold_value,
                    ptr.thresholdValueType)
                self._assign_ntf_value(
                    notification.notificationHandle, ptr.thresholdHysteresis,
                    self.ntf_info.threshold_information.threshold_hysteresis,
                    ptr.thresholdValueType)
                self._assign_ntf_value(
                    notification.notificationHandle, ptr.observedValue,
                    self.ntf_info.threshold_information.observed_value,
                    ptr.thresholdValueType)
                ptr.armTime = self.ntf_info.threshold_information.arm_time

            for i, attribute in enumerate(self.ntf_info.monitored_attrs):
                ptr = notification.monitoredAttributes[i]

                ptr.attributeId = attribute.attribute_id
                ptr.attributeType = attribute.attribute_type

                self._assign_ntf_value(notification.notificationHandle,
                                       ptr.attributeValue,
                                       attribute.attribute_value,
                                       attribute.attribute_type)

            for i, action in enumerate(self.ntf_info.proposed_repair_actions):
                ptr = notification.proposedRepairActions[i]
                ptr.actionId = action.action_id
                ptr.actionValueType = action.action_value_type
                self._assign_ntf_value(notification.notificationHandle,
                                       ptr.actionValue,
                                       action.action_value,
                                       action.action_value_type)

            # Send the notification
            rc = ntf.saNtfNotificationSend(notification.notificationHandle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saNtfNotificationSend FAILED, rc = %s" %
                        eSaAisErrorT.whatis(rc))

            # Free the notification
            ntf.saNtfNotificationFree(notification.notificationHandle)

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc

    @bad_handle_retry
    def send_security_alarm_notification(self):
        """ Send an SaNtfSecurityAlarmNotificationT notification

        Returns:
            SaAisErrorT: Return code of the corresponding NTF API call(s)
        """
        notification = saNtf.SaNtfSecurityAlarmNotificationT()
        rc = ntf.saNtfSecurityAlarmNotificationAllocate(
            self.handle, notification, 0,
            len(self.ntf_info.additional_text) + 1,
            len(self.ntf_info.additional_info),
            saNtf.saNtf.SA_NTF_ALLOC_SYSTEM_LIMIT)
        if rc != eSaAisErrorT.SA_AIS_OK:
            log_err("saNtfSecurityAlarmNotificationAllocate FAILED, "
                    "rc = %s" % eSaAisErrorT.whatis(rc))
        else:
            # Fill in the header
            self._fill_in_header(notification.notificationHandle,
                                 notification.notificationHeader)

            # Fill in security alarm-specific fields
            notification.probableCause.contents.value = \
                self.ntf_info.probable_cause
            notification.severity.contents.value = self.ntf_info.severity

            if self.ntf_info.security_alarm_detector is not None:
                notification.securityAlarmDetector.contents.valueType = \
                    self.ntf_info.security_alarm_detector.value_type
                self._assign_ntf_value(
                    notification.notificationHandle,
                    notification.securityAlarmDetector.contents.value,
                    self.ntf_info.security_alarm_detector.value,
                    self.ntf_info.security_alarm_detector.value_type)

            if self.ntf_info.service_user is not None:
                notification.serviceUser.contents.valueType = \
                    self.ntf_info.service_user.value_type
                self._assign_ntf_value(notification.notificationHandle,
                                       notification.serviceUser.contents.value,
                                       self.ntf_info.service_user.value,
                                       self.ntf_info.service_user.value_type)

            if self.ntf_info.service_provider is not None:
                notification.serviceProvider.contents.valueType = \
                    self.ntf_info.service_provider.value_type
                self._assign_ntf_value(
                    notification.notificationHandle,
                    notification.serviceProvider.contents.value,
                    self.ntf_info.service_provider.value,
                    self.ntf_info.service_provider.value_type)

            # Send the notification
            rc = ntf.saNtfNotificationSend(notification.notificationHandle)
            if rc != eSaAisErrorT.SA_AIS_OK:
                log_err("saNtfNotificationSend FAILED, rc = %s" %
                        eSaAisErrorT.whatis(rc))

            # Free the notification
            ntf.saNtfNotificationFree(notification.notificationHandle)

        if rc == eSaAisErrorT.SA_AIS_ERR_BAD_HANDLE:
            init_rc = self.init()
            # If the re-initialization of agent handle succeeds, we still need
            # to return BAD_HANDLE to the function decorator, so that it would
            # re-try the failed operation. Otherwise, the true error code is
            # returned to the user to decide further actions.
            if init_rc != eSaAisErrorT.SA_AIS_OK:
                rc = init_rc

        return rc
