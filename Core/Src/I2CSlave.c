/*
 * I2CSlave.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */
#include "I2CSlave.h"
#include "main.h"
#include "string.h"

#define I2CSLAVE_FIRMWAREVERSION 1
#define I2CSLAVE_REDCHANNELBIT  0b00000001
#define I2CSLAVE_BLUECHANNELBIT 0b00000010

/*## REGISTER ADDRESSES ##*/
#define FIRMWAREVERSIONREGADDR 0x00   // Version of the firware
#define HARDWAREFEATURESREGADDR 0x01  // Hardware features available: bit 0: RED Channel, bit 1: BLUE Channel
#define SERIALNOREGADDR 0x02  // Serialno of the dongle
#define ERRORCOUNTREGADDR 0x03  // Serialno of the dongle
#define STATUSREGADDR 0x04	  // Indicates what messages there is to fetch. bit 7: Error message, bit 0: Punch message
#define SETDATAINDEXREGADDR 0x05  // Index to the block data a register

// Length registers
#define PUNCHLENGTHREGADDR 0x20	  // Read punch message
#define ERRORLENGTHREGADDR 0x27
// Message data registers
#define PUNCHREGADDR 0x41	  // Read punch message
#define ERRORMSGREGADDR 0x47

struct Punch * I2CSlave_punchToSend;
struct Punch I2CSlave_emptyPunch = { .payloadLength = 99, .payload = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30}, .messageStatus = { .rssi = 100, .crc = 1}};
uint8_t I2CSlave_PunchLength = sizeof(struct Punch);
//uint8_t I2CSlave_transmitBuffer[256];

/* Buffer used for reception */
uint8_t I2CSlave_receivedRegister[2];
uint8_t I2CSlave_serialNumber[4] = {5, 6, 7, 8};
char I2CSlave_errorMessage[] = "This is an error message!";
//__IO uint32_t I2C_Transfer_Complete = 0;

uint8_t I2CSlave_TransmitIndex = 0;
uint8_t I2CSlave_ReceiveIndex = 0;

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
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
				HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &I2CSlave_serialNumber[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME);
			}
			break;
		}
		case ERRORCOUNTREGADDR:
		{
			uint8_t errorCount = 0;
			if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &errorCount, 1, I2C_FIRST_FRAME) != HAL_OK)
			{
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
				I2CSlave_TransmitIndex++;
				if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, ((uint8_t *) I2CSlave_punchToSend)+I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME) != HAL_OK)
				{
					Error_Handler();
				}
			}
			if (I2CSlave_TransmitIndex + 1 == I2CSlave_PunchLength)
			{
				// Last byte sent
				if (&I2CSlave_emptyPunch != I2CSlave_punchToSend)
				{
					PunchQueue_pop(); // in theory another punch could have been added to the queue and we now pops the wrong one...make a safe pop version
				}
			}
			break;
		}
		case ERRORMSGREGADDR:
		{
			if (I2CSlave_TransmitIndex < strlen(I2CSlave_errorMessage))
			{
				I2CSlave_TransmitIndex++;
				if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)&I2CSlave_errorMessage[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME) != HAL_OK)
				{
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
	switch(I2CSlave_receivedRegister[0])
	{
		case SERIALNOREGADDR:
		{
			if (I2CSlave_ReceiveIndex < 4)
			{
				if(HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &I2CSlave_serialNumber[I2CSlave_ReceiveIndex], 1, I2C_FIRST_FRAME) != HAL_OK)
				{
					Error_Handler();
				}
				I2CSlave_ReceiveIndex++;
			}
			break;
		}
		case SETDATAINDEXREGADDR:
		{
			if(HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME) != HAL_OK)
			{
				 Error_Handler();
			}
			break;
		}
		case PUNCHREGADDR:
		{
			if(HAL_I2C_Slave_Seq_Receive_IT(I2cHandle, &I2CSlave_receivedRegister[1], 1, I2C_FIRST_FRAME) != HAL_OK)
			{
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
	if (TransferDirection != 0)
	{
		/*##- TRANSMIT ##*/
		switch(I2CSlave_receivedRegister[0])
		{
			case FIRMWAREVERSIONREGADDR:
			{
				uint8_t firmwareVersion = I2CSLAVE_FIRMWAREVERSION;
				HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &firmwareVersion, 1, I2C_FIRST_FRAME);
				break;
			}
			case HARDWAREFEATURESREGADDR:
			{
				uint8_t hardwareFeatures = I2CSLAVE_REDCHANNELBIT | I2CSLAVE_BLUECHANNELBIT;
				HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &hardwareFeatures, 1, I2C_FIRST_FRAME);
				break;
			}
			case SERIALNOREGADDR:
			{
				I2CSlave_TransmitIndex=0;
				HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &I2CSlave_serialNumber[I2CSlave_TransmitIndex], 1, I2C_FIRST_FRAME);
				break;
			}
			case ERRORCOUNTREGADDR:
			{
				uint8_t errorCount = 1;
				HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &errorCount, 1, I2C_FIRST_FRAME);
				break;
			}
			case STATUSREGADDR:
			{
				uint8_t status = 0x00;
				if (PunchQueue_getNoOfItems()> 0)
				{
					status |= 0x01;
				}
				// todo: set error bit if error msg exists

				if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &status, 1, I2C_FIRST_FRAME) != HAL_OK)
				{
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
				uint8_t lengthOfPunch = sizeof(struct Punch);
				if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &lengthOfPunch, 1, I2C_FIRST_FRAME) != HAL_OK)
				{
					/* enable listen again? log error and return? */
					Error_Handler();
				}
				break;
			}
			case ERRORLENGTHREGADDR:
			{
				uint8_t lengthOfErrorMsg = strlen(I2CSlave_errorMessage); // todo:
				if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &lengthOfErrorMsg, 1, I2C_FIRST_FRAME) != HAL_OK)
				{
					/* enable listen again? log error and return? */
					Error_Handler();
				}
				break;
			}
			case PUNCHREGADDR:
			{
				if (PunchQueue_getNoOfItems() > 0)
				{
					PunchQueue_peek(I2CSlave_punchToSend);
				} else {
					I2CSlave_punchToSend = &I2CSlave_emptyPunch;
				}
				if (I2CSlave_TransmitIndex < I2CSlave_PunchLength)
				{

					if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, ((uint8_t *) I2CSlave_punchToSend) + I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME) != HAL_OK)
					{
						Error_Handler();
					}
				}
				break;
			}
			case ERRORMSGREGADDR:
			{
				if (I2CSlave_TransmitIndex < strlen(I2CSlave_errorMessage))
				{
					if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)&I2CSlave_errorMessage + I2CSlave_TransmitIndex, 1, I2C_FIRST_FRAME) != HAL_OK)
					{
						/* enable listen again? log error and return? */
						Error_Handler();
					}
				}
				break;
			}
		}
	}
	else
	{
		/*##- Put I2C peripheral in reception process ###########################*/
		if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, I2CSlave_receivedRegister, 1, I2C_FIRST_FRAME) != HAL_OK)
		{
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
	/* restart listening for master requests */
	if(HAL_I2C_EnableListen_IT(hi2c) != HAL_OK)
	{
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
  if (HAL_I2C_GetError(I2cHandle) != HAL_I2C_ERROR_AF)
  {
    Error_Handler();
  }
}
