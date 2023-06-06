/*
 * ErrorLog.h
 *
 *  Created on: Jun 6, 2023
 *      Author: henla464
 */

#ifndef SRC_ERRORLOG_H_
#define SRC_ERRORLOG_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "stm32g0xx_hal.h"

void ErrorLog_log(char* functionName, char* message);
uint16_t ErrorLog_getErrorCount();
char* ErrorLog_getMessage();
void ErrorLog_SetUARTInitialized(UART_HandleTypeDef *huart, bool initialized);

#endif /* SRC_ERRORLOG_H_ */
