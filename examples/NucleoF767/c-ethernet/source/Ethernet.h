/*******************************************************************************
 * Copyright (c) 2010-2020 Robert Bosch GmbH
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
 ******************************************************************************/

#ifndef ETHERNET_H_
#define ETHERNET_H_

#include "Kiso_Retcode.h"

/**
 * @brief Marks the Ethernet application entry point to be called after OS-boot.
 *
 * Function is designed to be enqueued into the system-default #CmdProcessor_T
 * instance.
 *
 * @param[in,out] cmdProcessor
 * Initialized instance of a #CmdProcessor_T to be used as apps' primary
 * CmdProcessor.
 * @param param2
 * Unused and ignored.
 */
void Ethernet_Startup(void *cmdProcessor, uint32_t param2);

#endif /* ETHERNET_H_ */
