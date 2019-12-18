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
 * @file
 * @defgroup    KISO_HAL_BSP_Memory_W25X40CL Flash memory W25X40CL
 * @ingroup     KISO_HAL_BSP_IF
 * @{
 *
 * @brief       BSP API for W25X40CL NOR flash memory from Winbond.
 * <A HREF="https://www.winbond.com/resource-files/w25x40cl_e01.pdf"><B>Datasheet</B></A>
 * @details
 * After reset, the microcontroller GPIOs are mostly tristated. Once the device is requested a call to
 * BSP_Mem_W25X40CL_Connect() function is required to map its GPIOs to their desired function (input, output, SPI, etc).
 *
 * A call to BSP_Mem_W25X40CL_Enable() will execute the required sequence to enable the device and its dedicated
 * MCU resources (e.g. SPI). The Device is then **controllable** through the microcontroller.
 *
 * BSP_Mem_W25X40CL_Disable() will revert back what the BSP_Mem_W25X40CL_Enable() has done and set the device and its related
 * MCU resources into disabled state.
 *
 * BSP_Mem_W25X40CL_Disconnect() will put the GPIO pins to the lowest power consumption and risk free configuration.
 * e.g. high impedance. 
 *
 * BSP_Mem_W25X40CL_GetHandle() will return a pointer to the MCU resource structure created and initialized by the BSP
 * implementation. This structure is MCU specific and its proper initialization is mandatory for the proper function of
 * the device. The Handle is the only contract between MCU layer and BSP layer.
 *
 * The following diagram describes the states and transitions for the memory device
 * @dot
 * digraph state_diagram {
 *      INIT[color="blue", shape=record]
 *      INIT -> CONNECTED[style="dashed", label="BSP_Mem_W25X40CL_Connect()"]
 *      CONNECTED -> DISCONNECTED [label="BSP_Mem_W25X40CL_Disconnect()"]
 *      DISCONNECTED -> CONNECTED [label="BSP_Mem_W25X40CL_Connect()"]
 *      CONNECTED -> ENABLED [label="BSP_Mem_W25X40CL_Enable()"]
 *      ENABLED -> DISABLED [label="BSP_Mem_W25X40CL_Disable()"]
 *      DISABLED -> ENABLED [label="BSP_Mem_W25X40CL_Enable()"]
 *      DISABLED -> DISCONNECTED [label="BSP_Mem_W25X40CL_Disconnect()"]
 *  }
 * @enddot
 */

#ifndef KISO_BSP_MEMORY_W25X40CL_H
#define KISO_BSP_MEMORY_W25X40CL_H

#include "Kiso_HAL.h"

/* Code is only effective if feature is enabled in configuration */
#if KISO_FEATURE_BSP_MEMORY_W25X40CL

/**
 * @brief       Maps the GPIO pins to their desired function.
 *
 * @details     This function once called will map the GPIO pins to their desired functions to drive the device.
 *              Basic IO operations are allowed after the successful execution of this function.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.,
 */
Retcode_T BSP_Mem_W25X40CL_Connect(void);

/**
 * @brief       Enables the control over the peripheral.
 *
 * @details     This function will execute the required sequence to enable the memory and allows it to receive,
 *              execute commands, and transmit data.
 *
 * @pre         BSP_Mem_W25X40CL_Connect() successfully executed.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_Enable(void);

/**
 * @brief       Disables the control over the peripheral.
 *
 * @details     This function once called will disable control over the flash memory and if possible power it off.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_Disable(void);

/**
 * @brief       Disconnects the flash memory.
 *
 * @details     This function will tristate the GPIOs of the memory, except if a functional or security or safety
 *              risk opposes that, then it will put them into lowest power consumption safest configuration.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_Disconnect(void);

/**
 * @brief       Returns the handle of the communication interface used by the flash memory.
 *
 * @details     The application calls this function in order to get the handle needed by the MCU resource drivers.
 *              The handle is a pointer to the MCU resource control structure created and initialized in the BSP
 *              implementation. The control structure is MCU specific and only known to BSP and MCU layers.
 *
 * @return      HWHandle_T: Handle for the used MCU resource.
 */
HWHandle_T BSP_Mem_W25X40CL_GetSPIHandle(void);

/**
 * @brief       Sets the memory SC pin to logic level high.
 * 
 * @details     This function sets the chip select line to high voltage level as seen by the device in order to 
 *              deselect from the SPI bus.
 *
 * @pre         BSP_Mem_W25X40CL_Connect() successfully executed.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_SetCSHigh(void);

/**
 * @brief       Sets the memory SC pin to logic level low.
 * 
 * @details     This function sets the chip select line to low voltage level as seen by the device in order to 
 *              select it for an SPI transaction.
 *
 * @pre         BSP_Mem_W25X40CL_Connect() successfully executed.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_SetCSLow(void);

/**
 * @brief       Sets the memory HOLD_N pin to logic level high.
 *
 * @details     This function sets hold line to high voltage level as seen by the device.
 *
 * @pre         BSP_Mem_W25X40CL_Connect() successfully executed.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_SetHoldHigh(void);

/**
 * @brief       Sets the memory HOLD_N pin to logic level low.
 *
 * @details     This function sets the hold line to low voltage level as seen by the device.
 *
 * @pre         BSP_Mem_W25X40CL_Connect() successfully executed.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_SetHoldLow(void);

/**
 * @brief       Sets the memory WP pin to logic level high.
 *
 * @details     This function sets the write protect line to high voltage level as seen by the device.
 *
 * @pre         BSP_Mem_W25X40CL_Connect() successfully executed.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_SetWPHigh(void);

/**
 * @brief       Sets the memory WP pin to logic level low.
 *
 * @details     This function sets the write protect line to low voltage level as seen by the device.
 *
 * @pre         BSP_Mem_W25X40CL_Connect() successfully executed.
 *
 * @return      RETCODE_OK in the case of success or an error code otherwise.
 */
Retcode_T BSP_Mem_W25X40CL_SetWPLow(void);

#endif /* KISO_FEATURE_BSP_MEMORY_W25X40CL */

#endif /* KISO_BSP_MEMORY_W25X40CL_H */

/**  @} */
