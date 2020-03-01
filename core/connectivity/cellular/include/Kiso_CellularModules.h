/********************************************************************************
* Copyright (c) 2010-2019 Robert Bosch GmbH
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License 2.0 which is available at
* http://www.eclipse.org/legal/epl-2.0.
*
* SPDX-License-Identifier: EPL-2.0
*
* Contributors:
*    Robert Bosch GmbH - initial contribution
*
********************************************************************************/

/**
 * @ingroup KISO_CELLULAR
 * @file
 * @brief Small utility header that contains the module IDs used by Cellular.
 */
#ifndef KISO_CELLULARMODULES_H_
#define KISO_CELLULARMODULES_H_

/**
 * @brief Cellular Module-Ids
 */
enum Kiso_Cellular_ModuleId_E
{
    KISO_CELLULAR_MODULE_ID_AT3GPP27007 = 1,
    KISO_CELLULAR_MODULE_ID_ATPARSER,
    KISO_CELLULAR_MODULE_ID_ATQUEUE,
    KISO_CELLULAR_MODULE_ID_ATTRANSCEIVER,
    KISO_CELLULAR_MODULE_ID_UTILS,
    KISO_CELLULAR_MODULE_ID_ENGINE,
    KISO_CELLULAR_MODULE_ID_HARDWARE,
    KISO_CELLULAR_MODULE_ID_QUEUE,

    KISO_CELLULAR_MODULE_ID_STARTOFVARIANT
};

#endif /* KISO_CELLULARMODULES_H_ */
