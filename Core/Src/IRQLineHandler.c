/*
 * PunchQueue.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */

#include "IRQLineHandler.h"


bool errorMessagesExist = false;
bool punchesExist = false;


void IRQLineHandler_SetErrorMessagesExist()
{
	errorMessagesExist = true;
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}

void IRQLineHandler_SetPunchesExist()
{
	punchesExist = true;
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}

void IRQLineHandler_ClearErrorMessagesExist()
{
	errorMessagesExist = false;
	if (!punchesExist) {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
	}
}

void IRQLineHandler_ClearPunchesExist()
{
	punchesExist = false;
	if (!errorMessagesExist) {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
	}
}

bool IRQLineHandler_GetErrorMessagesExist()
{
	return errorMessagesExist;
}
