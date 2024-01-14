/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "CC2500.h"
#include <stdbool.h>
#include "PunchQueue.h"
#include "I2CSlave.h"
#include <string.h>
#include "ErrorLog.h"


//#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
bool isInitialized = false;
struct PortAndPin RedChannelChipSelectPortPin;
struct PortAndPin BlueChannelChipSelectPortPin;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
static void ConfigureCC2500(void);
static void InitCC2500(SPI_HandleTypeDef* phspi, struct PortAndPin * chipSelectPin ,uint8_t channel);
static uint8_t GetPunchReplyIncludingSpaceForCommandByte(struct Punch punch, uint8_t * punchReply);
static void AckSentEnableRX_RedChannel(void);
static void AckSentEnableRX_BlueChannel(void);
static void SendAckReply_RedChannel(void);
static void SendAckReply_BlueChannel(void);
static bool EnableI2CListen(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  RedChannelChipSelectPortPin.GPIOx = GPIOA;
  RedChannelChipSelectPortPin.GPIO_Pin = GPIO_PIN_15;
  RedChannelChipSelectPortPin.InterruptIRQ = EXTI4_15_IRQn;
  RedChannelChipSelectPortPin.Channel = REDCHANNEL;

  BlueChannelChipSelectPortPin.GPIOx = GPIOA;
  BlueChannelChipSelectPortPin.GPIO_Pin = GPIO_PIN_5;
  BlueChannelChipSelectPortPin.InterruptIRQ = EXTI4_15_IRQn;
  BlueChannelChipSelectPortPin.Channel = BLUECHANNEL;

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  // Configure the RED and BLUE Channel. If a channel should not be enabled then put the corresponding CC2500 into sleep mode.
  ConfigureCC2500();

  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);


  if (!EnableI2CListen())
  {
	  // Tried multiple times
	  HAL_I2C_DeInit(&hi2c1);
	  HAL_I2C_Init(&hi2c1);
	  if (!EnableI2CListen())
	  {
		  HAL_NVIC_SystemReset();
	  }
  }

  /* PA6 PA12 is interrupt from CC2500
   * To avoid them being triggered too early we enable irq here
   * instead of in MX_GPIO_Init() */
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
  HAL_Delay(1);
  isInitialized = true;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  // You can try to reset the peripheral to see if it frees up. You can reset the peripheral with the I2C_CR1_SWRST bit.
	  if (hi2c1.State == HAL_I2C_STATE_READY) {
		  if (!EnableI2CListen())
		  {
			  HAL_I2C_DeInit(&hi2c1);
			  HAL_I2C_Init(&hi2c1);
			  if (!EnableI2CListen())
			  {
				  HAL_NVIC_SystemReset();
			  }
		  }
	  }

	  HAL_Delay(1);

	  if (HasChannelConfigurationChanged())
	  {
		  ConfigureCC2500();
		  ClearHasChannelConfigurationChanged();
	  }

	  // go to sleep
	  //HAL_SuspendTick();
	  //HAL_PWR_EnableSleepOnExit();
	  //HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
	  //HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
	  //HAL_ResumeTick();

	  /*
	  if (PunchQueue_isFull())
	  {
		   for(int i=0;i<6;i++) {
			  char header[] = "Message\r\n";
			  HAL_UART_Transmit(&huart1, header, strlen(header), HAL_MAX_DELAY);

			  for(int idx=0;idx<33;idx++)
			  {
				  char format2[] = "0x%x\r\n";
				  char format3[] = "0x0%x\r\n";
				  if (PunchQueue_items[i][idx] < 0x0F)
				  {
					  char msg[100];
			  		  sprintf(msg, format3, PunchQueue_items[i][idx]);
			  		  HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
				  } else
				  {
					  char msg[100];
					  sprintf(msg, format2, PunchQueue_items[i][idx]);
					  HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
				  }
			  }
		  }
		  return;
	  }*/
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10707DBC;
  hi2c1.Init.OwnAddress1 = 64;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */
  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  char msg[] = "UART1 Started\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  ErrorLog_SetUARTInitialized(&huart1, true);

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_15, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pins : PB9 PB0 PB2 PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PC14 PC15 PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA1 PA2 PA7 PA8
                           PA11 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7|GPIO_PIN_8
                          |GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA6 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

}

/* USER CODE BEGIN 4 */
void InitI2C()
{
	MX_I2C1_Init();
}


static bool EnableI2CListen()
{
	if (hi2c1.State == HAL_I2C_STATE_LISTEN)
	{
		return true;
	}
	uint16_t noOfTries = 0;
	do
	{
		HAL_Delay(1);
		uint8_t retStatus;
		if((retStatus = HAL_I2C_EnableListen_IT(&hi2c1)) != HAL_OK)
		{
			char msg[60];
			sprintf(msg, "HAL_I2C_EnableListen_IT ret: %u hi2c1->State: %u", retStatus, hi2c1.State);
			ErrorLog_log("EnableI2CListen", msg);

			noOfTries++;
			if (noOfTries == 100)
			{
				return false;
			}
		}
	} while (hi2c1.State != HAL_I2C_STATE_LISTEN);
	return true;
}


static void Configure_GDO_INT_1_AsRisingInterrupt()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	// Configure GPIO pins : PA4
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// EXTI interrupt init
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
}


static void Configure_GDO_INT_1_AsFallingInterrupt()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
}

static void Configure_GDO_INT_1_AsGPIO()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}


static void Configure_GDO_INT_2_AsRisingInterrupt()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA1 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
}

static void Configure_GDO_INT_2_AsFallingInterrupt()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA1 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
}

static void Configure_GDO_INT_2_AsGPIO()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA1 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void ConfigureCC2500() {
	if (IsRedChannelEnabled())
	{
		InitCC2500(&hspi1, &RedChannelChipSelectPortPin, REDCHANNEL);
	}
	else
	{
		CC2500_Reset(&hspi1, &RedChannelChipSelectPortPin);
		HAL_Delay(1);
		CC2500_Reset(&hspi1, &RedChannelChipSelectPortPin);
		HAL_Delay(1);
		do {
		} while (!CC2500_GetIsReadyAndIdle(&hspi1, &RedChannelChipSelectPortPin));  // while not chip ready and IDLE
		CC2500_PowerDown(&hspi1, &RedChannelChipSelectPortPin);
	}

	if (IsBlueChannelEnabled())
	{
		InitCC2500(&hspi1, &BlueChannelChipSelectPortPin, BLUECHANNEL);
	}
	else
	{
		CC2500_Reset(&hspi2, &BlueChannelChipSelectPortPin);
		HAL_Delay(1);
		CC2500_Reset(&hspi2, &BlueChannelChipSelectPortPin);
		HAL_Delay(1);
		do {
		} while (!CC2500_GetIsReadyAndIdle(&hspi2, &BlueChannelChipSelectPortPin));  // while not chip ready and IDLE
		CC2500_PowerDown(&hspi2, &BlueChannelChipSelectPortPin);
	}
}

static void InitCC2500(SPI_HandleTypeDef* phspi, struct PortAndPin * chipSelectPortPin, uint8_t channel)
{
	CC2500_Reset(phspi, chipSelectPortPin);
	HAL_Delay(1);
	CC2500_Reset(phspi, chipSelectPortPin);
	HAL_Delay(1);
	uint8_t writelength;
	writelength = 0x40;  // was 0x29=41
	CC2500_SetPacketLength(phspi, chipSelectPortPin, writelength);
	uint8_t readlength;
	readlength = CC2500_GetPacketLength(phspi, chipSelectPortPin);
	if ( readlength != writelength ) {
		char msg[100];
		sprintf(msg, "readLen: %u writeLen: %u", readlength, writelength);
		ErrorLog_log("InitCC2500", msg);
		HAL_NVIC_SystemReset();
	}


	CC2500_SetGDO0OutputPinConfiguration(phspi, chipSelectPortPin, GDOx_CFG_ASSERT_SYNC_WORD);

	// Switch to RX after sending packet, Stay in RX after receiving packet, Keep CCA_MODE default
	CC2500_SetMainRadioControlStateMachineConfiguration1(phspi, chipSelectPortPin, MCSM1_CCA_MODE_DEFAULT |	MCSM1_RXOFF_MODE_STAY_IN_RX | MCSM1_TXOFF_MODE_RX );

	CC2500_SetMainRadioControlStateMachineConfiguration0(phspi, chipSelectPortPin, MCSM0_FS_AUTOCAL_WHEN_GOING_TO_RX_TX_FROM_IDLE | MCSM0_PO_TIMEOUT_EXPIRE_COUNT_64);

	// Whitening OFF, normal packet format, CRC Enabled, Variable packet length mode. First byte after sync word.
	CC2500_SetPacketAutomationControl(phspi, chipSelectPortPin, PKTCTRL0_CRC_EN | PKTCTRL0_VARIABLE_PACKET_LENGTH);

	// 178 kHz
	CC2500_SetFrequencySynthesizerControl1(phspi, chipSelectPortPin, 0x07);
	// offset = 0
	CC2500_SetFrequencySynthesizerControl0(phspi, chipSelectPortPin, 0x00);

	// frequency 2424999878 :  multiply with 2^16 and divide by 26MHz to get frequency to set
	// dec 6112492 = hex 5d44ec
	CC2500_SetFrequencyHighByte(phspi, chipSelectPortPin, 0x5D);
	CC2500_SetFrequencyMiddleByte(phspi, chipSelectPortPin, 0x44);
	CC2500_SetFrequencyLowByte(phspi, chipSelectPortPin, 0xEC);

	CC2500_SetModemConfiguration4(phspi, chipSelectPortPin, MDMCFG4_CHANBW_66kHz | MDMCFG4_DRATE_E_13_66kHz);
	CC2500_SetModemConfiguration3(phspi, chipSelectPortPin, MDMCFG3_DRATE_M_125Baud);
	CC2500_SetModemConfiguration2(phspi, chipSelectPortPin, MDMCFG2_MOD_FORMAT_MSK | MDMCFG2_SYNC_MODE_30_32);
	// CHANSPC_E = 3 and CHANSPC_M = 59 => Channel spacing = 249.9 kHz
	CC2500_SetModemConfiguration1(phspi, chipSelectPortPin, MDMCFG1_NUM_PREAMBLE_4 | MDMCFG1_CHANSPC_E_249_9kHz);
	CC2500_SetModemConfiguration0(phspi, chipSelectPortPin, MDMCFG0_CHANSPC_M_249_9kHz);

	CC2500_SetModemDeviationSetting(phspi, chipSelectPortPin, DEVIATN_DEVIATION_E_MSK_1_785kHz | DEVIATN_DEVIATION_M_MSK_1_785kHz);
	CC2500_SetFrequencyOffsetConfiguration(phspi, chipSelectPortPin, FOCCFG_FOC_PRE_K_4K | FOCCFG_FOC_POST_K_Kdiv2 | FOCCFG_FOC_POST_FOC_LIMIT_BWdiv8);

	CC2500_SetBitSynchronizationConfiguration(phspi, chipSelectPortPin, BSCFG_BS_PRE_KI_0Kl | BSCFG_BS_PRE_KP_2Kp | BSCFG_BS_POST_KI_KlDiv2 | BSCFG_BS_POST_KP_KP);

	CC2500_SetAGCControl2(phspi, chipSelectPortPin, AGCCTRL2_MAX_DVGA_GAIN | AGCCTRL2_MAGN_TARGET_42);
	CC2500_SetAGCControl1(phspi, chipSelectPortPin, AGCCTRL1_AGC_LNA_PRIORITY_0);
	CC2500_SetAGCControl0(phspi, chipSelectPortPin, AGCCTRL0_HYST_LEVEL_MEDIUM | AGCCTRL0_WAIT_TIME_32);

	CC2500_SetFrontEndRXConfiguration(phspi, chipSelectPortPin, FREND1_LNA_CURRENT_2 | FREND1_LNA2MIX_CURRENT_3 | FREND1_LODIV_BUF_CURRENT_RX_1 | FREND1_MIX_CURRENT_2);
	CC2500_SetFrontEndTXConfiguration(phspi, chipSelectPortPin, FREND0_LODIV_BUF_CURRENT_TX_1);

	CC2500_SetFrequencySynthesizerCalibration3(phspi, chipSelectPortPin, FSCAL3_FSCAL3_HIGH_3 | FSCAL3_CHP_CURR_CAL_EN | FSCAL3_FSCAL3_LOW_A);
	CC2500_SetFrequencySynthesizerCalibration2(phspi, chipSelectPortPin, FSCAL2_FSCAL2_DEFAULT);
	CC2500_SetFrequencySynthesizerCalibration1(phspi, chipSelectPortPin, FSCAL1_FSCAL1_0);
	CC2500_SetFrequencySynthesizerCalibration0(phspi, chipSelectPortPin, FSCAL0_FSCAL0_17);

	CC2500_SetTest2(phspi, chipSelectPortPin, TEST2_136);
	CC2500_SetTest1(phspi, chipSelectPortPin, TEST1_49);
	CC2500_SetTest0(phspi, chipSelectPortPin, TEST0_HIGH_2 | TEST0_VCO_SEL_CAL_EN | TEST0_LOW_1);

	CC2500_ExitRXTX(phspi, chipSelectPortPin);
	uint8_t status;
	status = CC2500_GetStatusByteRead(phspi, chipSelectPortPin);  // Read instead??
	if ((status & 0x0F) > 0) {
		CC2500_FlushRXFIFO(phspi, chipSelectPortPin);
	}

	CC2500_SetChannelNumber(phspi, chipSelectPortPin, channel);
	CC2500_SetOutputPower(phspi, chipSelectPortPin, PATABLE_0DBM);
	CC2500_EnableRX(phspi, chipSelectPortPin);

	// wait for CARRIER SENSE
	uint8_t pktstatus;
	uint8_t noOfTries = 0;
	while(((pktstatus = CC2500_GetGDOxStatusAndPacketStatus(phspi, chipSelectPortPin)) & 0x40) > 0)
	{
		noOfTries++;
		if (noOfTries > 200) {
			char msg[100];
			sprintf(msg, "Carrier sense not reached, pktstatus: %u", pktstatus);
			ErrorLog_log("InitCC2500", msg);
			HAL_NVIC_SystemReset();
		}
	}

	CC2500_ExitRXTX(phspi, chipSelectPortPin);
	status = CC2500_GetStatusByteWrite(phspi, chipSelectPortPin);
	if ((status & 0x0F) > 0) {
		CC2500_FlushRXFIFO(phspi, chipSelectPortPin);
	}

	CC2500_SetGDO0OutputPinConfiguration(phspi, chipSelectPortPin, GDOx_CFG_ASSERT_SYNC_WORD);
	CC2500_EnableRX(phspi, chipSelectPortPin);



	/*
	for (uint8_t addr = 0x00;addr <= 0x2E; addr++)
	{
		uint8_t val;
		val = CC2500_ReadRegister(phspi, spiNumber, addr);
		int value = val;
		int address = addr;
		char format[] = "0x%x\r\n";
		char format2[] = "0x0%x\r\n";
		char msg[40];
		if (value < 0x0F)
		{
			sprintf(msg, format2, value);
			HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
		} else {
			sprintf(msg, format, value);
			HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
		}
	}*/

}

uint8_t PunchReplySequenceNo = 1;
uint8_t PunchReply[] = {0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x23, 0x00, 0x73, 0x60};
static uint8_t GetPunchReplyIncludingSpaceForCommandByte(struct Punch punch, uint8_t * punchReply)
{
	punchReply[2] = punch.payload[4]; 	// station serialno
	punchReply[3] = punch.payload[5]; 	// station serialno
	punchReply[4] = punch.payload[6]; 	// station serialno
	punchReply[5] = punch.payload[7]; 	// station serialno
	punchReply[6] = I2CSlave_serialNumber[0];
	punchReply[7] = I2CSlave_serialNumber[1];
	punchReply[8] = I2CSlave_serialNumber[2];
	punchReply[9] = I2CSlave_serialNumber[3];
	punchReply[12] = PunchReplySequenceNo;
	PunchReplySequenceNo++;
	return 15;
}

struct Punch punch;
static bool ReadMessage(SPI_HandleTypeDef* phspi, struct PortAndPin * chipSelectPortPin)
{
	uint8_t noOfRxBytes1 = 0;
	uint8_t noOfRxBytes2 = 0;
	do {
		noOfRxBytes1 = CC2500_GetNoOfRXBytes(phspi, chipSelectPortPin);
		if (noOfRxBytes1 == 0) {
			return false;
		}
		noOfRxBytes2 = CC2500_GetNoOfRXBytes(phspi, chipSelectPortPin);
	} while (noOfRxBytes1 != noOfRxBytes2);

	// read the length byte
	if (!CC2500_ReadRXFifo(phspi, chipSelectPortPin, &punch.payloadLength, 1))
	{
		ErrorLog_log("ReadMessage", "CC2500_ReadRXFifo ret false (1)");
		return false;
	}

	if (noOfRxBytes2 >= punch.payloadLength + 3 && punch.payloadLength <= sizeof(punch.payload))
	{
		if (!CC2500_ReadRXFifo(phspi, chipSelectPortPin, punch.payload, punch.payloadLength))
		{
			ErrorLog_log("ReadMessage", "CC2500_ReadRXFifo ret false (2)");
			return false;
		}
		if (!CC2500_ReadRXFifo(phspi, chipSelectPortPin, (uint8_t *)&punch.messageStatus, 2))
		{
			ErrorLog_log("ReadMessage", "CC2500_ReadRXFifo ret false (3)");
			return false;
		}
		if (punch.messageStatus.crc & 0x80)
		{
			// CRC OK
			punch.channel = chipSelectPortPin->Channel;
			uint8_t enqueueResult = PunchQueue_enQueue(&incomingPunchQueue, &punch);
			if (enqueueResult == QUEUEISFULL)
			{
				// queue full so don't ack
				ErrorLog_log("ReadMessage", "Queue full");
				return false;
			}
			// when enqueueResult is  ENQUEUESUCCESS or SAMEPUNCH then punch should be acked (unless in listen only mode)
		} else {
			return false;
		}
	} else {
		ErrorLog_log("ReadMessage", "Received too few bytes or the payload length byte in message is too large so will flush");
		CC2500_ExitRXTX(phspi, chipSelectPortPin);
		CC2500_FlushRXFIFO(phspi, chipSelectPortPin);
		CC2500_EnableRX(phspi, chipSelectPortPin);
		return false;
	}

	return true;
}

static void ReadMessage_RedChannel()
{
	if (!ReadMessage(&hspi1, &RedChannelChipSelectPortPin))
	{
		return;
	}
	if (IsRedChannelListenOnlyEnabled())
	{
		return;
	}
	SendAckReply_RedChannel();
}

static void ReadMessage_BlueChannel()
{
	if (!ReadMessage(&hspi2, &BlueChannelChipSelectPortPin))
	{
		return;
	}
	if (IsBlueChannelListenOnlyEnabled())
	{
		return;
	}
	SendAckReply_BlueChannel();
}


static void SendAckReply_RedChannel()
{
	CC2500_ExitRXTX(&hspi1, &RedChannelChipSelectPortPin);
	do {
	} while (!CC2500_GetIsReadyAndIdle(&hspi1, &RedChannelChipSelectPortPin));  // while not chip ready and IDLE
	CC2500_FlushRXFIFO(&hspi1, &RedChannelChipSelectPortPin);


	uint8_t replyLength = GetPunchReplyIncludingSpaceForCommandByte(punch, PunchReply);
	CC2500_WriteTXFifo(&hspi1, &RedChannelChipSelectPortPin, PunchReply, replyLength);

	// Disable interrupt, change GDO0 to PA_PD
	Configure_GDO_INT_1_AsGPIO();
	CC2500_SetGDO0OutputPinConfiguration(&hspi1, &RedChannelChipSelectPortPin, GDOx_CFG_PA_PD);

	CC2500_EnableRX(&hspi1, &RedChannelChipSelectPortPin);

	uint8_t packetStatus;
	do {
		//for(int i=0;i<2000;i++); // add a abit of delay here?
		packetStatus = CC2500_GetGDOxStatusAndPacketStatus(&hspi1, &RedChannelChipSelectPortPin);
	} while (!(packetStatus & 0x10)); // wait for channel clear

	// Enable rising interrupts for CC2500-GDO0
	Configure_GDO_INT_1_AsRisingInterrupt();
	HAL_NVIC_EnableIRQ(RedChannelChipSelectPortPin.InterruptIRQ);

	CC2500_EnableTX(&hspi1, &RedChannelChipSelectPortPin);
}

static void SendAckReply_BlueChannel()
{
	CC2500_ExitRXTX(&hspi2, &BlueChannelChipSelectPortPin);
	do {
	} while (!CC2500_GetIsReadyAndIdle(&hspi2, &BlueChannelChipSelectPortPin));  // while not chip ready and IDLE
	CC2500_FlushRXFIFO(&hspi2, &BlueChannelChipSelectPortPin);


	uint8_t replyLength = GetPunchReplyIncludingSpaceForCommandByte(punch, PunchReply);
	CC2500_WriteTXFifo(&hspi2, &BlueChannelChipSelectPortPin, PunchReply, replyLength);

	// Disable interrupt, change GDO0 to PA_PD
	Configure_GDO_INT_2_AsGPIO();
	CC2500_SetGDO0OutputPinConfiguration(&hspi2, &BlueChannelChipSelectPortPin, GDOx_CFG_PA_PD);

	CC2500_EnableRX(&hspi2, &BlueChannelChipSelectPortPin);

	uint8_t packetStatus;
	do {
		//for(int i=0;i<2000;i++); // add a abit of delay here?
		packetStatus = CC2500_GetGDOxStatusAndPacketStatus(&hspi2, &BlueChannelChipSelectPortPin);
	} while (!(packetStatus & 0x10)); // wait for channel clear

	// Enable rising interrupts for CC2500-GDO0
	Configure_GDO_INT_2_AsRisingInterrupt();
	HAL_NVIC_EnableIRQ(BlueChannelChipSelectPortPin.InterruptIRQ);

	CC2500_EnableTX(&hspi2, &BlueChannelChipSelectPortPin);
}

static void AckSentEnableRX_RedChannel()
{
	// must be in idle, but is probably in RX now...
	CC2500_FlushTXFIFO(&hspi1, &RedChannelChipSelectPortPin);
	Configure_GDO_INT_1_AsGPIO();
	CC2500_SetGDO0OutputPinConfiguration(&hspi1, &RedChannelChipSelectPortPin, GDOx_CFG_ASSERT_SYNC_WORD);
	//Configure_GDO0 as falling interrupt
	Configure_GDO_INT_1_AsFallingInterrupt();
	HAL_NVIC_EnableIRQ(RedChannelChipSelectPortPin.InterruptIRQ);
	CC2500_EnableRX(&hspi1, &RedChannelChipSelectPortPin);
}

static void AckSentEnableRX_BlueChannel()
{
	// must be in idle, but is probably in RX now...
	CC2500_FlushTXFIFO(&hspi2, &BlueChannelChipSelectPortPin);
	Configure_GDO_INT_2_AsGPIO();
	CC2500_SetGDO0OutputPinConfiguration(&hspi2, &BlueChannelChipSelectPortPin, GDOx_CFG_ASSERT_SYNC_WORD);
	//Configure_GDO0 as falling interrupt
	Configure_GDO_INT_2_AsFallingInterrupt();
	HAL_NVIC_EnableIRQ(BlueChannelChipSelectPortPin.InterruptIRQ);
	CC2500_EnableRX(&hspi2, &BlueChannelChipSelectPortPin);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
	if (isInitialized)
	{
		if(GPIO_Pin == GPIO_PIN_12) // PA12 - first CC2500
		{
			if (IsRedChannelEnabled()) {
				//HAL_ResumeTick();
				//SystemClock_Config();
				ReadMessage_RedChannel();
			}
		}
		else if(GPIO_Pin == GPIO_PIN_6) // PA6 - second CC2500
		{
			if (IsBlueChannelEnabled()) {
				//HAL_ResumeTick();
				//SystemClock_Config();
				ReadMessage_BlueChannel();
			}
		}
	}
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
	if (isInitialized)
	{
		if(GPIO_Pin == GPIO_PIN_12) // PA12 - first CC2500
		{
			if (IsRedChannelEnabled()) {
				//HAL_ResumeTick();
				//SystemClock_Config();
				AckSentEnableRX_RedChannel();
			}
		}
		else if(GPIO_Pin == GPIO_PIN_6) // PA6 - second CC2500
		{
			if (IsBlueChannelEnabled()) {
				//HAL_ResumeTick();
				//SystemClock_Config();
				AckSentEnableRX_BlueChannel();
			}
		}
	}
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	do {

	}while (1);

	__disable_irq();
	char msg[100];
	sprintf(msg, "FILE: %s LINE: %u\r\n", file, line);
	ErrorLog_log("_Error_Hanler", msg);

	HAL_NVIC_SystemReset();
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
