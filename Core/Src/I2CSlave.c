/*
 * I2CSlave.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 *
 * 			| Master      | Direction | Slave          |		| Master      | Direction | Slave          |
 *			|-------------|-----------|----------------|		|-------------|-----------|----------------|
 *			| Address + W | --------> | AddrCallback   |		| Address + W | --------> | AddrCallback   |
 *			| Data        | --------> | RxCpltCallback |		| Data        | --------> | RxCpltCallback |
 *			| STOP        | --------> | ListenCallback |		| Data        | --------> | RxCpltCallback |
 *			| Addr + R    | --------> | AddrCallback   |		| Data        | --------> | RxCpltCallback |
 *			| Data        | <-------- | TxCpltCallback |		  ...
 *			| STOP        | --------> | ListenCallback |		| STOP        | --------> | ListenCallback |
 */
#include "I2CSlave.h"
#include "main.h"
#include "string.h"
#include "ErrorLog.h"

#define I2CSLAVE_FIRMWAREVERSION 1
#define I2CSLAVE_REDCHANNELBIT  0b00000001
#define I2CSLAVE_BLUECHANNELBIT 0b00000010
#define I2CSLAVE_UARTERRORBIT 0b00000100
#define I2CSLAVE_REDCHANNELLISTENONLYBIT 0b00001000
#define I2CSLAVE_BLUECHANNELLISTENONLYBIT 0b00010000
#define I2CSLAVE_SENDMODEBIT 0b00100000


/*## REGISTER ADDRESSES ##*/
#define FIRMWAREVERSIONREGADDR 0x00   // Version of the firmware
#define HARDWAREFEATURESREGADDR 0x01  // Hardware features available: bit 0: RED Channel, bit 1: BLUE Channel, bit 2: Send errors on UART, bit 3: RED channel only listen, bit 4: BLUE channel only listen, bit 5: Send mode
#define SERIALNOREGADDR 0x02  // Serialno of the dongle
#define ERRORCOUNTREGADDR 0x03  // No of errors
#define STATUSREGADDR 0x04	  // Indicates what messages there is to fetch. bit 7: Error message, bit 0: Punch message
#define SETDATAINDEXREGADDR 0x05  // Index to the block data a register
#define HARDWAREFEATURESENABLEDISABLEREGADDR 0x06  // Hardware feature, enabled or disable: bit 0: RED Channel, bit 1: BLUE Channel, bit 2: Send errors on UART, bit 3: RED channel only listen, bit 4: BLUE channel only listen, bit 5: Send mode

// Length registers
#define PUNCHLENGTHREGADDR 0x20	  // Read punch message
#define ERRORLENGTHREGADDR 0x27
// Message data registers
#define PUNCHREGADDR 0x40	  // Read punch message
#define ERRORMSGREGADDR 0x47

struct Punch *I2CSlave_punchToSendPointer;
struct Punch I2CSlave_punchToSendBuffer;

uint8_t I2CSlave_PunchLength = sizeof(struct Punch);
uint8_t volatile I2CSlave_receivedRegister;
uint8_t I2CSlave_hardwareFeaturesAvailable = 0x1F;
uint8_t volatile I2CSlave_hardwareFeaturesEnableDisable = 0x03;
uint8_t volatile I2CSlave_serialNumber[4] = {5, 6, 7, 8};
uint8_t volatile I2CSlave_TransmitIndex = 0;
uint8_t volatile I2CSlave_ReceiveIndex = 0;
bool volatile channelConfigurationChanged = false;

uint8_t volatile I2CSlave_previousHardwareFeaturesEnableDisable = 0x03;

bool IsRedChannelEnabled()
{
	return (I2CSlave_hardwareFeaturesEnableDisable & I2CSLAVE_REDCHANNELBIT) > 0;
}

bool IsBlueChannelEnabled()
{
	return (I2CSlave_hardwareFeaturesEnableDisable & I2CSLAVE_BLUECHANNELBIT) > 0;
}

bool IsSendErrorsToUARTEnabled()
{
	return (I2CSlave_hardwareFeaturesEnableDisable & I2CSLAVE_UARTERRORBIT) > 0;
}

bool IsRedChannelListenOnlyEnabled()
{
	return (I2CSlave_hardwareFeaturesEnableDisable & I2CSLAVE_REDCHANNELLISTENONLYBIT) > 0;
}

bool IsBlueChannelListenOnlyEnabled()
{
	return (I2CSlave_hardwareFeaturesEnableDisable & I2CSLAVE_BLUECHANNELLISTENONLYBIT) > 0;
}

// Send mode note implemented yet
bool IsInSendMode()
{
	return (I2CSlave_hardwareFeaturesEnableDisable & I2CSLAVE_SENDMODEBIT) > 0;
}

bool HasChannelConfigurationChanged()
{
	return channelConfigurationChanged;
}

void ClearHasChannelConfigurationChanged()
{
	channelConfigurationChanged = false;
}

void SetChannelConfigurationChanged()
{
	channelConfigurationChanged = true;
}


void I2C_Reset(I2C_HandleTypeDef *hi2c)
{
	RCC->APBRSTR1 |= (1 << 22);
	InitI2C();
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	uint8_t status = 0;
	switch(I2CSlave_receivedRegister)
	{
		case FIRMWAREVERSIONREGADDR:
		{
			// Only one byte to send
			break;
		}
		case HARDWAREFEATURESREGADDR:
		{
			// Only one byte to send
			break;
		}
		case SERIALNOREGADDR:
		{
			if (I2CSlave_TransmitIndex < sizeof(I2CSlave_serialNumber))
			{
				I2CSlave_TransmitIndex++;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *) &I2CSlave_serialNumber[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "SERIALNOREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
					/* enable listen again? log error and return? */
					Error_Handler();
				}
			}
			break;
		}
		case ERRORCOUNTREGADDR:
		{
			//		ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
			break;
		}
		case STATUSREGADDR:
		{
			// Only one byte to send
			break;
		}
		case SETDATAINDEXREGADDR:
		{
			// No bytes to send
			break;
		}
		case PUNCHLENGTHREGADDR:
		{
			// Only one byte to send
			break;
		}
		case ERRORLENGTHREGADDR:
		{
			// Only one byte to send
			break;
		}
		case PUNCHREGADDR:
		{
			I2CSlave_TransmitIndex++;
			if (I2CSlave_punchToSendPointer == NULL) {
				if (I2CSlave_TransmitIndex <= 1) {
					uint8_t zeroByte = 0;
					if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &zeroByte, 1, I2C_FIRST_FRAME)) != HAL_OK)
					{
						char msg[30];
						sprintf(msg, "PUNCHREGADDR:1ret: %u", status);
						ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
						Error_Handler();
					}
				}
			} else {
				uint8_t realIndexToUse = I2CSlave_TransmitIndex;
				uint8_t headerLength = 1;
				if (I2CSlave_TransmitIndex >= I2CSlave_punchToSendPointer->payloadLength + headerLength)
				{
					// the index is after the end of the payload, we need to skip remaining empty bytes in the payload array
					// and skip to the footer
					uint8_t remainingPayloadLength = sizeof(I2CSlave_punchToSendPointer->payload) - I2CSlave_punchToSendPointer->payloadLength;
					realIndexToUse += remainingPayloadLength;
				}

				if (realIndexToUse < I2CSlave_PunchLength)
				{
					if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, ((uint8_t *) I2CSlave_punchToSendPointer)+realIndexToUse, 1, I2C_FIRST_FRAME)) != HAL_OK)
					{
						char msg[30];
						sprintf(msg, "PUNCHREGADDR:ret: %u", status);
						ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
						Error_Handler();
					}
				}
				else
				{
					// Last byte sent
					if (I2CSlave_punchToSendPointer != NULL)
					{
						PunchQueue_pop(&incomingPunchQueue);
					}
				}
			}

			break;
		}
		case ERRORMSGREGADDR:
		{
			// Send one by one or maybe 30 or so each time? Or maybe we can assume all was sent in addr?
			if (I2CSlave_TransmitIndex < strlen(ErrorLog_getMessage()))
			{
				I2CSlave_TransmitIndex++;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)(ErrorLog_getMessage() + I2CSlave_TransmitIndex), 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "ERRORMSGREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
					/* enable listen again? log error and return? */
					Error_Handler();
				}
			} else {
				IRQLineHandler_ClearErrorMessagesExist();
			}
			break;
		}
	}

}

/**
  * @brief  Rx Transfer completed callback.
  * @param  I2cHandle: I2C handle
  * @note   This example shows a simple way to report end of IT Rx transfer, and
  *         you can add your own implementation.
  * @retval None
  */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
	uint8_t status;
	switch(I2CSlave_receivedRegister)
	{
		case SERIALNOREGADDR:
		{
			if (I2CSlave_ReceiveIndex < 4)
			{
				I2CSlave_ReceiveIndex++;
				if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, (uint8_t *)&I2CSlave_serialNumber[I2CSlave_ReceiveIndex-1], 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[30];
					sprintf(msg, "SERIALNOREGADDR:ret: %u", status);
					ErrorLog_log("I2C_SlaveRxCpltCallback", msg);

					Error_Handler();
				}
			}
			break;
		}
		case HARDWAREFEATURESENABLEDISABLEREGADDR:
		{
			if (I2CSlave_ReceiveIndex <= 0) {
				I2CSlave_ReceiveIndex++;
				if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, (uint8_t *)&I2CSlave_hardwareFeaturesEnableDisable, 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[46];
					sprintf(msg, "HARDWAREFEATURESENABLEDISABLEREGADDR:ret: %u", status);
					ErrorLog_log("I2C_SlaveRxCpltCallback", msg);
					Error_Handler();
				}
			} else {
				if ((I2CSlave_previousHardwareFeaturesEnableDisable & 0b00011011) != (I2CSlave_hardwareFeaturesEnableDisable & 0b00011011))
				{
					SetChannelConfigurationChanged();
					I2CSlave_previousHardwareFeaturesEnableDisable = I2CSlave_hardwareFeaturesEnableDisable;
				}
			}
			break;
		}
		case SETDATAINDEXREGADDR:
		{
			if (I2CSlave_ReceiveIndex <= 0) {
				I2CSlave_ReceiveIndex++;
				if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, (uint8_t *)&I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[30];
					sprintf(msg, "SETDATAINDEXREGADDR:ret: %u", status);
					ErrorLog_log("I2C_SlaveRxCpltCallback", msg);
					Error_Handler();
				}
			}
			break;
		}
		case PUNCHREGADDR:
		{
			// this is not used now i guess, for sending punches?
			//if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, ??, 1, I2C_FIRST_FRAME)) != HAL_OK)
			//{
			//	char msg[30];
			//	sprintf(msg, "PUNCHREGADDR:ret: %u", status);
			//	ErrorLog_log("I2C_SlaveRxCpltCallback", msg);
			//	Error_Handler();
			//}
			break;
		}
	}
}

/**
  * @brief  Slave Address Match callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  TransferDirection: Master request Transfer Direction (Write/Read), value of @ref I2C_XferOptions_definition
  * @param  AddrMatchCode: Address Match Code
  * @retval None
  */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
	uint8_t status;
	if (TransferDirection != 0)
	{
		/*##- TRANSMIT ##*/
		switch(I2CSlave_receivedRegister)
		{
			case FIRMWAREVERSIONREGADDR:
			{
				uint8_t firmwareVersion = I2CSLAVE_FIRMWAREVERSION;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &firmwareVersion, 1, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "FIRMWAREVERSIONREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);

					Error_Handler();
				}
				break;
			}
			case HARDWAREFEATURESREGADDR:
			{
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &I2CSlave_hardwareFeaturesAvailable, 1, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "HARDWAREFEATURESREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					Error_Handler();
				}
				break;
			}
			case HARDWAREFEATURESENABLEDISABLEREGADDR:
			{
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)&I2CSlave_hardwareFeaturesEnableDisable, 1, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "HARDWAREFEATURESENABLEDISABLEREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					Error_Handler();
				}
				break;
			}
			case SERIALNOREGADDR:
			{
				I2CSlave_TransmitIndex=0;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)&I2CSlave_serialNumber[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "SERIALNOREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					Error_Handler();
				}
				break;
			}
			case ERRORCOUNTREGADDR:
			{
				// Send both bytes
				uint16_t errorCount = ErrorLog_getErrorCount();
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t*)&errorCount, 2, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "ERRORCOUNTREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					Error_Handler();
				}
				break;
			}
			case STATUSREGADDR:
			{
				uint8_t statusValToReturn = 0x00;
				if (PunchQueue_getNoOfItems(&incomingPunchQueue)> 0)
				{
					statusValToReturn |= 0x01;
				}

				if (IRQLineHandler_GetErrorMessagesExist())
				{
					statusValToReturn |= 0x80;
				}

				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &statusValToReturn, 1, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "STATUSREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					/* enable listen again? log error and return? */
					Error_Handler();
				}
				break;
			}
			case SETDATAINDEXREGADDR:
			{

				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)&I2CSlave_TransmitIndex, 1, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "SETDATAINDEXREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					/* enable listen again? log error and return? */
					Error_Handler();
				}
				break;
			}
			case PUNCHLENGTHREGADDR:
			{
				uint8_t lengthOfPunch = 0;
				if (PunchQueue_getNoOfItems(&incomingPunchQueue) > 0)
				{
					PunchQueue_peek(&incomingPunchQueue, &I2CSlave_punchToSendBuffer);
					uint8_t headerLength = 1;
					uint8_t footerLengthIncludingChannel = 3;
					lengthOfPunch = I2CSlave_punchToSendBuffer.payloadLength + headerLength + footerLengthIncludingChannel;
				}

				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &lengthOfPunch, 1, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "PUNCHLENGTHREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					/* enable listen again? log error and return? */
					Error_Handler();
				}
				break;
			}
			case ERRORLENGTHREGADDR:
			{
				uint8_t lengthOfErrorMsg = strlen(ErrorLog_getMessage());
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &lengthOfErrorMsg, 1, I2C_LAST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "ERRORLENGTHREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					/* enable listen again? log error and return? */
					Error_Handler();
				}
				break;
			}
			case PUNCHREGADDR:
			{
				if (PunchQueue_getNoOfItems(&incomingPunchQueue) > 0)
				{
					PunchQueue_peek(&incomingPunchQueue, &I2CSlave_punchToSendBuffer);
					I2CSlave_punchToSendPointer = &I2CSlave_punchToSendBuffer;
					uint8_t headerLength = 1;
					uint8_t footerLengthIncludingChannel = 3;
					I2CSlave_PunchLength = I2CSlave_punchToSendBuffer.payloadLength + headerLength + footerLengthIncludingChannel;
				} else {
					I2CSlave_punchToSendPointer = NULL;
				}
				if (I2CSlave_TransmitIndex < I2CSlave_PunchLength)
				{
					if (I2CSlave_punchToSendPointer == NULL) {
						uint8_t zeroByte = 0;
						if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &zeroByte, 1, I2C_FIRST_FRAME)) != HAL_OK)
						{
							char msg[30];
							sprintf(msg, "PUNCHREGADDR:1ret: %u", status);
							ErrorLog_log("HAL_I2C_AddrCallback", msg);
							Error_Handler();
						}
					}
					else
					{
						if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, ((uint8_t *) I2CSlave_punchToSendPointer) + I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME)) != HAL_OK)
						{
							char msg[30];
							sprintf(msg, "PUNCHREGADDR:2ret: %u", status);
							ErrorLog_log("HAL_I2C_AddrCallback", msg);
							Error_Handler();
						}
					}
				}
				break;
			}
			case ERRORMSGREGADDR:
			{
				// Some i2c libraries has message length limitations, try to stay compatible with that? Now we try to send all at once.
				// We could send only 30 bytes or so each time...
				// Note that trasmitindex must be set first by separate i2c call to the transmitindex register
				uint8_t length = strlen(ErrorLog_getMessage());
				uint8_t lengthToSend = length - I2CSlave_TransmitIndex;
				if (I2CSlave_TransmitIndex < length)
				{
					if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)(ErrorLog_getMessage()+I2CSlave_TransmitIndex), lengthToSend, I2C_FIRST_FRAME)) != HAL_OK)
					{
						char msg[100];
						sprintf(msg, "ERRORMSGREGADDR:ret: %u", status);
						ErrorLog_log("HAL_I2C_AddrCallback", msg);
						/* enable listen again? log error and return? */
						Error_Handler();
					}
					I2CSlave_TransmitIndex += lengthToSend;
				}
				if (I2CSlave_TransmitIndex >= strlen(ErrorLog_getMessage()))
				{
					IRQLineHandler_ClearErrorMessagesExist();
				}
				break;
			}
		}
	}
	else
	{
		/*##- Put I2C peripheral in reception process ###########################*/
		I2CSlave_ReceiveIndex = 0;
		if ((status = HAL_I2C_Slave_Seq_Receive_IT(hi2c, (uint8_t *)&I2CSlave_receivedRegister, 1, I2C_FIRST_FRAME)) != HAL_OK)
		{
			char msg[100];
			sprintf(msg, "ELSE:ret: %u", status);
			ErrorLog_log("HAL_I2C_AddrCallback", msg);
			/* Transfer error in reception process */
			Error_Handler();
		}
	}
}

/**
  * @brief  Listen Complete callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval None
  */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
	uint8_t status = 0;
	/* restart listening for master requests */
	if((status = HAL_I2C_EnableListen_IT(hi2c)) != HAL_OK)
	{
		char msg[50];
		sprintf(msg, "ret: %u", status);
		ErrorLog_log("I2C_ListenCpltCallback", msg);
		Error_Handler();
	}
}

/**
  * @brief  I2C error callbacks.
  * @param  I2cHandle: I2C handle
  * @note   This example shows a simple way to report transfer error, and you can
  *         add your own implementation.
  * @retval None
  */

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *I2cHandle)
{
  /** Error_Handler() function is called when error occurs.
    * 1- When Slave doesn't acknowledge its address, Master restarts communication.
    * 2- When Master doesn't acknowledge the last data transferred, Slave doesn't care in this example.
    */
	uint8_t status = 0;
	if ((status = HAL_I2C_GetError(I2cHandle)) != HAL_I2C_ERROR_AF)
	{
		char msg[35] = {0};
		sprintf(msg, "I2C_GetError ret: %u", status);
		ErrorLog_log("I2C_ErrorCallback", msg);
		Error_Handler();
	}
}
