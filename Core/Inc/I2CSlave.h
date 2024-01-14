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

extern uint8_t I2CSlave_serialNumber[4];

extern bool IsRedChannelEnabled(void);
extern bool IsBlueChannelEnabled(void);
extern bool IsSendErrorsToUARTEnabled(void);
extern bool IsRedChannelListenOnlyEnabled(void);
extern bool IsBlueChannelListenOnlyEnabled(void);
extern bool HasChannelConfigurationChanged(void);
extern void ClearHasChannelConfigurationChanged(void);


#endif /* INC_I2CSLAVE_H_ */
