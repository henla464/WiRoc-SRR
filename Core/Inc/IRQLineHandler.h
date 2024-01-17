/*
 * IRQLineHandler.h
 *
 *  Created on: Jan 17, 2024
 *      Author: henla464
 */

#ifndef INC_IRQLINEHANDLER_H_
#define INC_IRQLINEHANDLER_H_
#include <stdio.h>
#include <stdbool.h>
#include "stm32g0xx_hal.h"

extern void IRQLineHandler_SetErrorMessagesExist(void);
extern void IRQLineHandler_SetPunchesExist(void);
extern void IRQLineHandler_ClearErrorMessagesExist(void);
extern void IRQLineHandler_ClearPunchesExist(void);

#endif /* INC_IRQLINEHANDLER_H_ */
