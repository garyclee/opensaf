/*      -*- OpenSAF  -*-
 *
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
 * Author(s): Ericsson AB
 *
 */

#include "smf/smfd/SmfUtils_ObjExist.h"

#include <string.h>
#include <iostream>

#include "ais/include/saAis.h"
#include "ais/include/saImmOm.h"
#include "base/saf_error.h"
#include "base/osaf_extended_name.h"
#include "base/time.h"

#include "smf/smfd/imm_modify_config/immccb.h"

  // Check if the object exist. If fail use recovery possibilities
  // The Object DN is created from class and parent name
  CheckObjectExist::ReturnCode CheckObjectExist::IsExisting(
                          modelmodify::CreateDescriptor& create_descriptor) {
    // Allow for two max time retries
    base::Timer check_timer(120000);  // 2 minutes
    ReturnCode rc = ReturnCode::kOk;

    while (check_timer.is_timeout() != true) {
      if (is_initialized_ != true) {
        // Only initialize if not already initialized
        if (OmInitialize() != ReturnCode::kOk) {
          LOG_NO("%s: OmInitialize(), Fail", __FUNCTION__);
          rc = ReturnCode::kFail;
          break;
        }

        ReturnCode acci_rc = AccesorInitialize();
        if (acci_rc == ReturnCode::kRestartOm) {
          is_initialized_ = false;
          // No delay needed here
          continue;
        } else if (acci_rc == kFail) {
          LOG_NO("%s: AccesorInitialize(), Fail", __FUNCTION__);
          rc = ReturnCode::kFail;
          break;
        }
      }

      ReturnCode getrdn_rc = GetObjectRdn(create_descriptor);
      if (getrdn_rc == ReturnCode::kRestartOm) {
        is_initialized_ = false;
        // No delay needed here
        continue;
      } else if (getrdn_rc == kFail) {
        LOG_NO("%s: GetObjectRdn(), Fail", __FUNCTION__);
        is_initialized_ = false;
        rc = ReturnCode::kFail;
        break;
      }

      std::string object_dn = object_rdn_;
      if (create_descriptor.parent_name.empty() == false) {
        object_dn += "," + create_descriptor.parent_name;
      }

      ReturnCode aget_rc = AccessorGet(object_dn);
      if (aget_rc == ReturnCode::kRestartOm) {
        is_initialized_ = false;
        // No delay needed here
        continue;
      }
      if (aget_rc == ReturnCode::kFail) {
        LOG_NO("%s: AccessorGet(), Fail", __FUNCTION__);
        is_initialized_ = false;
        rc = ReturnCode::kFail;
        break;
      }

      // If we get here we did not fail and have a valid result
      rc = aget_rc;
      break;
    }  // check_timer.is_timeout()
    if ((check_timer.is_timeout() == true)
        && (rc != ReturnCode::kOk)
        && (rc != ReturnCode::kNotExist)) {
      LOG_NO("%s: IsExisting Timeout", __FUNCTION__);
      is_initialized_ = false;
      rc = ReturnCode::kFail;
    }

    if ((rc == ReturnCode::kOk) || (rc == ReturnCode::kNotExist)) {
      is_initialized_ = true;
    }

    return rc;
  }


  // Update om_handle_
  CheckObjectExist::ReturnCode CheckObjectExist::OmInitialize() {
    // Long timeout. We may have to wait for IMM sync
    base::Timer init_timer(60000);  // 1 minute
    SaAisErrorT ais_rc = SA_AIS_OK;
    ReturnCode rc = ReturnCode::kOk;

    if (om_handle_ != 0) {
      saImmOmFinalize(om_handle_);
      om_handle_ = 0;
    }

    while (init_timer.is_timeout() != true) {
      SaVersionT tmp_version = kImmVersion;
      ais_rc = saImmOmInitialize(&om_handle_, NULL, &tmp_version);
      if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
        base::Sleep(base::kOneHundredMilliseconds);
        continue;
      } else if (ais_rc != SA_AIS_OK) {
        rc = ReturnCode::kFail;
        LOG_NO("%s: saImmOmInitialize Fail, %s", __FUNCTION__,
               saf_error(ais_rc));
        break;
      }
      rc = ReturnCode::kOk;
      break;
    }
    if (init_timer.is_timeout() && ais_rc != SA_AIS_OK) {
      LOG_NO("%s: saImmOmInitialize Timeout, %s", __FUNCTION__,
             saf_error(ais_rc));
      rc = ReturnCode::kFail;
    }

    return rc;
  }

  // Update accessor_handle_
  CheckObjectExist::ReturnCode CheckObjectExist::AccesorInitialize() {
    base::Timer init_timer(10000);  // 10 seconds
    SaAisErrorT ais_rc = SA_AIS_OK;
    ReturnCode rc = ReturnCode::kOk;

    while (init_timer.is_timeout() != true) {
      ais_rc = saImmOmAccessorInitialize(om_handle_, &accessor_handle_);
      if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
        base::Sleep(base::kOneHundredMilliseconds);
        continue;
      } else if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
        rc = ReturnCode::kRestartOm;
        break;
      } else if (ais_rc != SA_AIS_OK) {
        LOG_NO("%s: saImmOmAccessorInitialize Fail, %s", __FUNCTION__,
               saf_error(ais_rc));
        rc = ReturnCode::kFail;
        break;
      }
      rc = ReturnCode::kOk;
      break;
    }
    if (init_timer.is_timeout() && ais_rc != SA_AIS_OK) {
      LOG_NO("%s: saImmOmAccessorInitialize Timeout, %s", __FUNCTION__,
             saf_error(ais_rc));
      rc = ReturnCode::kFail;
    }

    return rc;
  }

  // Check object existence
  CheckObjectExist::ReturnCode CheckObjectExist::AccessorGet(std::string
                                                             object_dn) {
    ReturnCode rc = ReturnCode::kOk;
    SaAisErrorT ais_rc = SA_AIS_OK;
    SaImmAttrValuesT_2 **fetched_attributes;  // Dummy, not used
    SaNameT object_dn_as_sanamet;
    osaf_extended_name_lend(object_dn.c_str(), &object_dn_as_sanamet);

    base::Timer get_timer(10000);  // 10 seconds
    while (get_timer.is_timeout() == false) {
      ais_rc = saImmOmAccessorGet_2(accessor_handle_,
                                    &object_dn_as_sanamet,
                                    NULL,
                                    &fetched_attributes);
      if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
        base::Sleep(base::kOneHundredMilliseconds);
        continue;
      }
      if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
        rc = ReturnCode::kRestartOm;
        break;
      }
      if (ais_rc == SA_AIS_ERR_NOT_EXIST) {
        rc = ReturnCode::kNotExist;
        break;
      }
      if (ais_rc == SA_AIS_OK) {
        rc = ReturnCode::kOk;
        break;
      }

      // All other ais error codes is a fail
      LOG_NO("%s: saImmOmAccessorGet_2 Fail, %s", __FUNCTION__,
             saf_error(ais_rc));
      rc = ReturnCode::kFail;
      break;
    }
    if (get_timer.is_timeout() &&
        (ais_rc != SA_AIS_OK) &&
        (ais_rc != SA_AIS_ERR_NOT_EXIST)) {
      // Timeout fail only if we failed to get a result
      LOG_NO("%s: saImmOmAccessorGet_2 Timeout, %s", __FUNCTION__,
             saf_error(ais_rc));
      rc = ReturnCode::kFail;
    }

    return rc;
  }

  // Update object_rdn_
  // Fail if IMM error or no RDN could be found
  CheckObjectExist::ReturnCode CheckObjectExist::GetObjectRdn(
                          modelmodify::CreateDescriptor& create_descriptor ) {
    SaAisErrorT ais_rc = SA_AIS_OK;
    ReturnCode rc = ReturnCode::kOk;

    // Get the class descriptor
    SaImmAttrDefinitionT_2** attr_definitions;
    SaImmClassCategoryT class_category = SA_IMM_CLASS_CONFIG;
    SaImmClassNameT class_name = const_cast<char *>
                                 (create_descriptor.class_name.c_str());

    base::Timer get_timer(10000);  // 10 seconds
    while (get_timer.is_timeout() == false) {
      ais_rc = saImmOmClassDescriptionGet_2(om_handle_,
                                            class_name,
                                            &class_category,
                                            &attr_definitions);
      if (ais_rc == SA_AIS_ERR_TRY_AGAIN) {
        base::Sleep(base::kOneHundredMilliseconds);
        continue;
      } else if (ais_rc == SA_AIS_ERR_BAD_HANDLE) {
        rc = ReturnCode::kRestartOm;
        break;
      } else if (ais_rc != SA_AIS_OK) {
        LOG_NO("%s: saImmOmClassDescriptionGet_2 Fail, %s", __FUNCTION__,
               saf_error(ais_rc));
        rc = ReturnCode::kFail;
        break;
      }
      rc = ReturnCode::kOk;
      break;
    }
    if (get_timer.is_timeout() && ais_rc != SA_AIS_OK) {
      LOG_NO("%s: saImmOmClassDescriptionGet_2 Timeout, %s", __FUNCTION__,
             saf_error(ais_rc));
      rc = ReturnCode::kFail;
    }

    if (rc == ReturnCode::kOk) {
      // Find name of RDN attribute
      std::string rdn_attr;
      for (int i = 0; attr_definitions[i] != nullptr; i++) {
        SaImmAttrFlagsT flags = attr_definitions[i]->attrFlags;
        if ((flags & SA_IMM_ATTR_RDN) == SA_IMM_ATTR_RDN) {
          rdn_attr = reinterpret_cast<char *>(attr_definitions[i]->attrName);
          break;
        }
      }

      // Find attribute value of RDN attribute (object RDN)
      for (auto& attribute : create_descriptor.attributes) {
        if (attribute.attribute_name == rdn_attr) {
          if (attribute.values_as_strings.empty()) {
            object_rdn_.clear();
            rc = ReturnCode::kFail;
          } else {
            object_rdn_ = attribute.values_as_strings[0];
          }
          break;
        }
      }
    } else {
      // Failed. No valid object RDN is found
      object_rdn_.clear();
    }

    return rc;
  }
