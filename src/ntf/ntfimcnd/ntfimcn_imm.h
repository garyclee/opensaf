/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2013 The OpenSAF Foundation
 * Copyright Ericsson AB 2020 - All Rights Reserved.
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

#ifndef NTF_NTFIMCND_NTFIMCN_IMM_H_
#define NTF_NTFIMCND_NTFIMCN_IMM_H_

#include "ntfimcn_main.h"
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the OI interface, get a selection object and become applier
 *
 * @global_param max_waiting_time_ms: Wait max time for each operation.
 * @global_param applier_name: The name of the "configuration change" applier
 * @param *cb[out]
 *
 * @return (-1) if init fail
 */
int ntfimcn_imm_init(ntfimcn_cb_t *cb);

/**
 * For a given array of IMM Attribute Modifications, fetch the
 * corresponding current IMM Attribute Values. The return data
 * curAttr is managed by IMM unless a copy is requested.
 * Deep clone method is used to copy the returned IMM Attribute Values
 *
 * @param *objectName[in]
 * @param **attrMods[in]
 * @param ***curAttr[out]
 * @param copy[in]
 * @return SaAisErrorT
 */
SaAisErrorT get_current_attrs(const SaNameT *objectName,
   const SaImmAttrModificationT_2 **attrMods, SaImmAttrValuesT_2 ***curAttr);

/**
 * Deep clone IMM Attribute Values
 *
 * @param **src[in]
 */
SaImmAttrValuesT_2** dupSaImmAttrValuesT_array(const SaImmAttrValuesT_2 **src);

/**
 * Deallocate memory used for deep cloning IMM Attribute Values
 *
 * @param **attrs[in]
 */
void free_imm_attrs(SaImmAttrValuesT_2 **attrs);

/**
 * Find attribute values from the given attribute name
 * Reuturn NULL if no values found for the given name
 *
 * @param **attrs[in]
 * @param name[in]
 * @return SaImmAttrValuesT_2*
 */

const SaImmAttrValuesT_2* find_attr_from_name(
   const SaImmAttrValuesT_2** attrs, SaImmAttrNameT name);


#ifdef __cplusplus
}
#endif

#endif  // NTF_NTFIMCND_NTFIMCN_IMM_H_
