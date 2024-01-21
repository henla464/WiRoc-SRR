/*
 * ErrorLog.c
 *
 *  Created on: Jun 6, 2023
 *      Author: henla464
 */
#include "ErrorLog.h"
#include "I2CSlave.h"

uint16_t ErrorLog_errorCount = 0;
char ErrorLog_message[256];
bool ErrorLog_UARTInitialized = false;
UART_HandleTypeDef* ErrorLog_huart;

void ErrorLog_log(char* functionName, char* message)
{
	ErrorLog_errorCount++;
	uint8_t functionNameLength = strlen(functionName);
	uint8_t lengthToCopy = functionNameLength < 50 ? functionNameLength : 50;
	strncpy(ErrorLog_message, functionName, lengthToCopy);
	ErrorLog_message[lengthToCopy] = ':';
	ErrorLog_message[lengthToCopy+1] = ':';
	uint8_t messageLength = strlen(message);
	uint8_t remainingSpace = 255-lengthToCopy-2;
	uint8_t messageLengthToCopy = messageLength < remainingSpace ? messageLength : remainingSpace;
	strncpy(&ErrorLog_message[lengthToCopy+2], message, messageLengthToCopy);
	ErrorLog_message[lengthToCopy+2+messageLengthToCopy] = '\0';

	if (ErrorLog_UARTInitialized && IsSendErrorsToUARTEnabled())
	{
		HAL_UART_Transmit(ErrorLog_huart, (uint8_t *)ErrorLog_message, strlen(ErrorLog_message), HAL_MAX_DELAY);
		HAL_UART_Transmit(ErrorLog_huart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
	}
	IRQLineHandler_SetErrorMessagesExist();
}

uint16_t ErrorLog_getErrorCount()
{
	return ErrorLog_errorCount;
}

char* ErrorLog_getMessage()
{
	return ErrorLog_message;
}

void ErrorLog_SetUARTInitialized(UART_HandleTypeDef *huart, bool initialized)
{
	ErrorLog_UARTInitialized = initialized;
	ErrorLog_huart = huart;
}
