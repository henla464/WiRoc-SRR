/*
 * I2CSlave.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */
#include "I2CSlave.h"
#include "main.h"


#define PUNCHREGADDR 0x00
#define SERIALNOREGADDR 0x01
#define ERRORREGADDR 0x02
/* Buffer used for transmission */

struct Punch * I2CSlave_punchToSend;

/* Buffer used for reception */
uint8_t I2CSlave_receivedRegister[1];
uint8_t I2CSlave_serialNumber[4] = {0};
__IO uint32_t I2C_Transfer_Complete = 0;

//__IO uint32_t     Transfer_Direction = 0;

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
	/* Toggle LED4: Transfer in transmission process is correct */
	I2C_Transfer_Complete = 1;
	PunchQueue_pop();
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
	I2C_Transfer_Complete = 1;
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
	//Transfer_Direction = TransferDirection;
	if (TransferDirection != 0)
	{
		/*##- Start the transmission process #####################################*/

		if (I2CSlave_receivedRegister[0] == PUNCHREGADDR)
		{
			if (PunchQueue_getNoOfItems() > 0)
			{
				PunchQueue_pop(I2CSlave_punchToSend);
				if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *) I2CSlave_punchToSend, sizeof(struct Punch), I2C_FIRST_AND_LAST_FRAME) != HAL_OK)
				{
					/* Transfer error in transmission process */
					Error_Handler();
				}
			}
		} else if (I2CSlave_receivedRegister[0] == SERIALNOREGADDR) {

		} else if (I2CSlave_receivedRegister[0] == ERRORREGADDR) {

		}
	}
	else
	{
		/*##- Put I2C peripheral in reception process ###########################*/
		if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, I2CSlave_receivedRegister, 1, I2C_FIRST_AND_LAST_FRAME) != HAL_OK)
		{
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
