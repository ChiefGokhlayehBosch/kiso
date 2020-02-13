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
 *
 * @defgroup ENGINE Engine
 * @{
 *
 * @brief The engine of the Cellular driver.
 *
 * @details This module manages the Idle-URC-Listener-task and maintains an
 * #AtTransceiver_S instance for use within the driver. It also performs state
 * notification to application-code.
 *
 * The Idle-URC-Listener-task is only activated, if the driver is idle (i.e.
 * transceiver unlocked) and data arrived on the BSP interface. This ensures,
 * that the URC-handling doesn't steal AT responses meant for a
 * command-sender.
 *
 * @file
 */

#ifndef ENGINE_H_
#define ENGINE_H_

#include "Kiso_Cellular.h"
#include "Kiso_CellularConfig.h"

#include "AtTransceiver.h"

/**
 * @brief Initializes the Engine. Allocates necessary RTOS resources and starts
 * the Idle-URC-Listener-task. It also initializes the AT transceiver.
 *
 * @param[in] onStateChanged
 * Callback to be called in case of any state change.
 *
 * @return A #Retcode_T indicating the result of the procedure.
 */
Retcode_T Engine_Initialize(Cellular_StateChanged_T onStateChanged);

/**
 * @brief Deinitializes the Engine. Deallocates necessary RTOS resources and
 * stops the Idle-URC-Listener-task. It also deinitializes the AT transceiver.
 */
void Engine_Deinitialize(void);

/**
 * @brief Transition the engine into a new state and notify the user.
 *
 * @param[in] newState
 * New state to transition to.
 *
 * @param[in] param
 * Arbitrary parameter directly passed to the user-callback. May be NULL.
 *
 * @param[in] len
 * Length of arbitrary parameter. May be zero.
 */
void Engine_NotifyNewState(Cellular_State_T newState, void *param, uint32_t len);

/**
 * @brief Set the driver-internal state on wheather or not to expect echo
 * responses from the modem.
 *
 * This function will @b not interact with the modem to enable/disable
 * echo-mode. Telling the modem to enable/disable echo-mode must be done
 * through other means.
 *
 * Changing the internal echo-mode state while a session is active may not have
 * an effect on that session. The session must be closed and re-opened for the
 * settings to apply.
 *
 * @param[in] echoMode
 * `true` if the transceivers should be configured with echo-mode
 * "ON", `false` otherwise.
 */
void Engine_SetEchoMode(bool echoMode);

/**
 * @brief Returns the current driver-internal echo-mode state.
 *
 * @return true
 * Driver-side echo-mode is enabled.
 *
 * @return false
 * Driver-side echo-mode is disabled.
 */
bool Engine_GetEchoMode(void);

/**
 * @brief Open an AT transceiver session on the phyisical communcations
 * channel.
 *
 * Upon successful return the function guarantees, that the returned
 * #AtTransceiver_S instance is initialized with a fresh write-sequence. The
 * transceiver can then be used for sending AT commands and/or handling received
 * AT responses.
 *
 * The driver only supports a single phyisical communcations channel. Hence the
 * need to synchronize access. Any party interested in accessing the physical
 * channel must obtain a shared transceiver session through this function. The
 * session is then implicitly locked and associated with the calling thread. If
 * the channel is currently in use by a different thread, the calling thread
 * will halt and try to obtain the lock once it becomes available again.
 *
 * A caller must close the session via call to #Engine_CloseTransceiver() after
 * use. Thereby allowing other threads to take control over the channel.
 *
 * @param[out] atTransceiver
 * On success, will be pointing to a fully set-up transceiver instance for
 * sending and receiving on the requested channel.
 *
 * @return A Retcode_T indicating the result of the action.
 *
 * @see Engine_CloseTransceiver
 */
Retcode_T Engine_OpenTransceiver(struct AtTransceiver_S **atTransceiver);

/**
 * @brief Close down the active transceiver session.
 *
 * This includes releasing locks held on the channel and transceiver.
 *
 * The calling thread must be owner of the session.
 *
 * If the calling thread is not currently owner of the session behaviour is
 * undefined.
 *
 * @return A Retcode_T indicating the result of the action.
 *
 * @see Engine_OpenTransceiver
 */
Retcode_T Engine_CloseTransceiver(void);

#endif /* ENGINE_H_ */
/** @} */
