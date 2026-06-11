/*
 * Radio.h
 *
 *  Created on: Jun 11, 2026
 *      Author: henla464
 */

#ifndef INC_RADIO_H_
#define INC_RADIO_H_

#include "stm32g0xx_hal.h"
#include "CC2500.h"
#include "PunchQueue.h"

// CC2500 chip select / interrupt port+pin definitions
extern struct PortAndPin RedChannelChipSelectPortPin;
extern struct PortAndPin BlueChannelChipSelectPortPin;

// Sync word interrupt state (read by main loop for stuck-interrupt detection)
extern bool volatile RedChannelSyncWordInterrupt;
extern bool volatile RedChannelSyncWordDetected;
extern bool volatile BlueChannelSyncWordInterrupt;
extern bool volatile BlueChannelSyncWordDetected;

// TX guard flag (cleared by ReconfigureCC2500 from main loop)
extern bool volatile txInProgress;

// Public functions — called from main()
void InitializeBothCC2500(void);
void ReconfigureCC2500(void);
void ProcessOutgoingPunches(void);

// HAL EXTI callbacks (override weak defaults)
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin);
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin);

#endif /* INC_RADIO_H_ */
