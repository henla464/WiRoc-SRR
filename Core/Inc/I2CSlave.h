/*
 * I2CSlave.h
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */

#ifndef INC_I2CSLAVE_H_
#define INC_I2CSLAVE_H_

#include "stm32g0xx_hal.h"
#include "stdint.h"
#include "PunchQueue.h"

extern __IO uint32_t I2C_Transfer_Complete;

extern volatile uint8_t I2CSlave_serialNumber[4];
extern volatile uint8_t I2CSlave_hardwareFeaturesEnableDisable;

extern bool IsRedChannelEnabled(void);
extern bool IsBlueChannelEnabled(void);
extern bool IsSendErrorsToUARTEnabled(void);
extern bool IsRedChannelListenOnlyEnabled(void);
extern bool IsBlueChannelListenOnlyEnabled(void);
extern bool IsInSendMode(void);
extern bool HasChannelConfigurationChanged(void);
extern void ClearHasChannelConfigurationChanged(void);
extern void SetChannelConfigurationChanged(void);

/* Test modes (register TESTMODEREGADDR) for EU RED / EN 300 328 RF testing.
 * Test mode is enabled/disabled by bit 6 of
 * HARDWAREFEATURESENABLEDISABLEREGADDR (0x06); this register only selects
 * which test mode to run while enabled.
 * Only compiled in when TEST_MODES_ENABLED is defined (see main.h). When
 * disabled, the test-mode bit in HARDWAREFEATURES (0x01) reports unavailable. */
#define TEST_MODE_TX_CARRIER   0x01  // continuous unmodulated carrier
#define TEST_MODE_TX_AA_LOOP   0x02  // continuous loop of 0xAA-filled packets
#define TEST_MODE_TX_NORMAL_5S 0x03  // normal punch packet every 5 seconds
#define TEST_MODE_RX_TEST      0x04  // RX test: ignore CW, discard 0xAA, ACK normal

extern uint8_t GetTestMode(void);
extern bool IsTestModeEnabled(void);

#endif /* INC_I2CSLAVE_H_ */
