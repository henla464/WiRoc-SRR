/*
 * I2CSlave.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */
#include "I2CSlave.h"
#include "main.h"
#include "string.h"
#include "ErrorLog.h"

#define I2CSLAVE_FIRMWAREVERSION 1
#define I2CSLAVE_REDCHANNELBIT  0b00000001
#define I2CSLAVE_BLUECHANNELBIT 0b00000010

/*## REGISTER ADDRESSES ##*/
#define FIRMWAREVERSIONREGADDR 0x00   // Version of the firware
#define HARDWAREFEATURESREGADDR 0x01  // Hardware features available: bit 0: RED Channel, bit 1: BLUE Channel, bit 2: Send errors on UART
#define SERIALNOREGADDR 0x02  // Serialno of the dongle
#define ERRORCOUNTREGADDR 0x03  // Serialno of the dongle
#define STATUSREGADDR 0x04	  // Indicates what messages there is to fetch. bit 7: Error message, bit 0: Punch message
#define SETDATAINDEXREGADDR 0x05  // Index to the block data a register

// Length registers
#define PUNCHLENGTHREGADDR 0x20	  // Read punch message
#define ERRORLENGTHREGADDR 0x27
// Message data registers
#define PUNCHREGADDR 0x40	  // Read punch message
#define ERRORMSGREGADDR 0x47

struct Punch *I2CSlave_punchToSendPointer;
struct Punch *I2CSlave_punchToSendID;
struct Punch I2CSlave_punchToSendBuffer;

uint8_t I2CSlave_PunchLength = sizeof(struct Punch);
uint8_t I2CSlave_receivedRegister[2];
uint8_t I2CSlave_serialNumber[4] = {5, 6, 7, 8};
uint16_t I2CSlave_LastErrorCount = 0;
uint8_t I2CSlave_TransmitIndex = 0;
uint8_t I2CSlave_ReceiveIndex = 0;

void I2C_Reset(I2C_HandleTypeDef *hi2c)
{
	RCC->APBRSTR1 |= (1 << 22);
	InitI2C();
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	uint8_t status = 0;
	switch(I2CSlave_receivedRegister[0])
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
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &I2CSlave_serialNumber[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME)) != HAL_OK)
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
			uint8_t errorCount = 0;
			if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &errorCount, 1, I2C_FIRST_FRAME)) != HAL_OK)
			{
				char msg[100];
				sprintf(msg, "ERRORCOUNTREGADDR:ret: %u", status);
				ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
				/* enable listen again? log error and return? */
				Error_Handler();
			}
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
			if (I2CSlave_TransmitIndex < I2CSlave_PunchLength)
			{
				if (I2CSlave_punchToSendPointer == NULL) {
					uint8_t zeroByte = 0;
					if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &zeroByte, 1, I2C_FIRST_FRAME)) != HAL_OK)
					{
						char msg[30];
						sprintf(msg, "PUNCHREGADDR:1ret: %u", status);
						ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
						Error_Handler();
					}
				} else {
					I2CSlave_TransmitIndex++;
					if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, ((uint8_t *) I2CSlave_punchToSendPointer)+I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME)) != HAL_OK)
					{
						char msg[30];
						sprintf(msg, "PUNCHREGADDR:ret: %u", status);
						ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
						Error_Handler();
					}
				}
			}
			if (I2CSlave_TransmitIndex + 1 == I2CSlave_PunchLength)
			{
				// Last byte sent
				if (I2CSlave_punchToSendPointer != NULL)
				{
					PunchQueue_popSafe(&incomingPunchQueue, I2CSlave_punchToSendID); // in theory another punch could have been added to the queue and we now pops the wrong one...make a safe pop version
				}
			}
			break;
		}
		case ERRORMSGREGADDR:
		{
			if (I2CSlave_TransmitIndex < strlen(ErrorLog_getMessage()))
			{
				I2CSlave_TransmitIndex++;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)&ErrorLog_getMessage()[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "ERRORMSGREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_SlaveTxCpltCallback", msg);
					/* enable listen again? log error and return? */
					Error_Handler();
				}
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
	switch(I2CSlave_receivedRegister[0])
	{
		case SERIALNOREGADDR:
		{
			if (I2CSlave_ReceiveIndex < 4)
			{
				if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &I2CSlave_serialNumber[I2CSlave_ReceiveIndex], 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[30];
					sprintf(msg, "SERIALNOREGADDR:ret: %u", status);
					ErrorLog_log("I2C_SlaveRxCpltCallback", msg);

					Error_Handler();
				}
				I2CSlave_ReceiveIndex++;
			}
			break;
		}
		case HARDWAREFEATURESREGADDR:
		{
			uint8_t featuresRegister;
			if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &featuresRegister, 1, I2C_FIRST_FRAME)) != HAL_OK)
			{
				char msg[35];
				sprintf(msg, "HARDWAREFEATURESREGADDR:ret: %u", status);
				ErrorLog_log("I2C_SlaveRxCpltCallback", msg);
				Error_Handler();
			}
			ErrorLog_printErrorsToUARTEnabled = (featuresRegister & 0x04);
			break;
		}
		case SETDATAINDEXREGADDR:
		{
			if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME)) != HAL_OK)
			{
				char msg[30];
				sprintf(msg, "SETDATAINDEXREGADDR:ret: %u", status);
				ErrorLog_log("I2C_SlaveRxCpltCallback", msg);
				Error_Handler();
			}
			break;
		}
		case PUNCHREGADDR:
		{
			if((status = HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &I2CSlave_receivedRegister[1], 1, I2C_FIRST_FRAME)) != HAL_OK)
			{
				char msg[30];
				sprintf(msg, "PUNCHREGADDR:ret: %u", status);
				ErrorLog_log("I2C_SlaveRxCpltCallback", msg);
				Error_Handler();
			}
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
		switch(I2CSlave_receivedRegister[0])
		{
			case FIRMWAREVERSIONREGADDR:
			{
				uint8_t firmwareVersion = I2CSLAVE_FIRMWAREVERSION;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &firmwareVersion, 1, I2C_FIRST_FRAME)) != HAL_OK)
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
				uint8_t hardwareFeatures = I2CSLAVE_REDCHANNELBIT | I2CSLAVE_BLUECHANNELBIT;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &hardwareFeatures, 1, I2C_FIRST_FRAME)) != HAL_OK)
				{
					char msg[100];
					sprintf(msg, "HARDWAREFEATURESREGADDR:ret: %u", status);
					ErrorLog_log("HAL_I2C_AddrCallback", msg);
					Error_Handler();
				}
				break;
			}
			case SERIALNOREGADDR:
			{
				I2CSlave_TransmitIndex=0;
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &I2CSlave_serialNumber[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME)) != HAL_OK)
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
				uint8_t errorCount = ErrorLog_getErrorCount();
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &errorCount, 1, I2C_FIRST_FRAME)) != HAL_OK)
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

				uint16_t errorCount = ErrorLog_getErrorCount();
				if (errorCount > I2CSlave_LastErrorCount)
				{
					statusValToReturn |= 0x80;
				}

				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &statusValToReturn, 1, I2C_FIRST_FRAME)) != HAL_OK)
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
				// Only write
				break;
			}
			case PUNCHLENGTHREGADDR:
			{
				uint8_t lengthOfPunch = 0;
				if (PunchQueue_getNoOfItems(&incomingPunchQueue) > 0)
				{
					PunchQueue_peek(&incomingPunchQueue, &I2CSlave_punchToSendBuffer, &I2CSlave_punchToSendID);
					lengthOfPunch = I2CSlave_punchToSendBuffer.payloadLength + 4;
				}

				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &lengthOfPunch, 1, I2C_FIRST_FRAME)) != HAL_OK)
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
				if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &lengthOfErrorMsg, 1, I2C_FIRST_FRAME)) != HAL_OK)
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
					PunchQueue_peek(&incomingPunchQueue, &I2CSlave_punchToSendBuffer, &I2CSlave_punchToSendID);
					I2CSlave_punchToSendPointer = &I2CSlave_punchToSendBuffer;
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
				if (I2CSlave_TransmitIndex < strlen(ErrorLog_getMessage()))
				{
					if ((status = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)ErrorLog_getMessage() + I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME)) != HAL_OK)
					{
						char msg[100];
						sprintf(msg, "ERRORMSGREGADDR:ret: %u", status);
						ErrorLog_log("HAL_I2C_AddrCallback", msg);
						/* enable listen again? log error and return? */
						Error_Handler();
					}
				} else {
					I2CSlave_LastErrorCount = ErrorLog_getErrorCount();
				}
				break;
			}
		}
	}
	else
	{
		/*##- Put I2C peripheral in reception process ###########################*/
		if ((status = HAL_I2C_Slave_Seq_Receive_IT(hi2c, I2CSlave_receivedRegister, 1, I2C_FIRST_FRAME)) != HAL_OK)
		{
			char msg[100];
			sprintf(msg, "ELSE:ret: %u", status);
			ErrorLog_log("HAL_I2C_AddrCallback", msg);
			/* Transfer error in reception process */
			Error_Handler();
		}
		if (I2CSlave_receivedRegister[0] == SERIALNOREGADDR)
		{
			I2CSlave_ReceiveIndex = 0;
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
