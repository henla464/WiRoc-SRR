/*
 * CC2500.h
 *
 *  Created on: May 24, 2023
 *      Author: henla464
 */

#ifndef SRC_CC2500_H_
#define SRC_CC2500_H_
#include "stm32g0xx_hal.h"
#include "stdint.h"
#include <stdbool.h>


#define TEMP_SENSOR_ENABLE 0x40
#define GDO0_INV 0x20
// Asserts when sync word has been sent / received, and de-asserts at the end of the packet. In RX, the pin will de-assert
// when the optional address check fails or the RX FIFO overflows. In TX the pin will de-assert if the TX FIFO underflows.
#define GDOx_CFG_ASSERT_SYNC_WORD 0x06
//PA_PD. Note: PA_PD will have the same signal level in SLEEP and TX states.
// To control an external PA or RX/TX
//switch in applications where the SLEEP state is used it is recommended to use GDOx_CFGx=0x2F instead.
// Power amplifier power down
#define GDOx_CFG_PA_PD 0x1B

#define MCSM1_CCA_MODE_DEFAULT 0x30
#define MCSM1_CCA_MODE_ALWAYS 0x00
#define MCSM1_RXOFF_MODE_STAY_IN_RX 0x0C
#define MCSM1_TXOFF_MODE_RX 0x03

#define MCSM0_FS_AUTOCAL_WHEN_GOING_TO_RX_TX_FROM_IDLE 0x10
#define MCSM0_PO_TIMEOUT_EXPIRE_COUNT_64 0x08

#define PKTCTRL0_CRC_EN 0x04
#define PKTCTRL0_VARIABLE_PACKET_LENGTH 0x01

#define PATABLE_0DBM 0xFE

#define MDMCFG4_CHANBW_66kHz 0x20
#define MDMCFG4_DRATE_E_13_66kHz 0x0D

#define MDMCFG3_DRATE_M_125Baud 0x3B

#define MDMCFG2_MOD_FORMAT_MSK 0x70
#define MDMCFG2_SYNC_MODE_30_32 0x03

#define MDMCFG1_NUM_PREAMBLE_4 0x20
#define MDMCFG1_CHANSPC_E_249_9kHz 0x03

#define MDMCFG0_CHANSPC_M_249_9kHz 0x3B

#define DEVIATN_DEVIATION_E_MSK_1_785kHz 0x00
#define DEVIATN_DEVIATION_M_MSK_1_785kHz 0x01

#define FOCCFG_FOC_PRE_K_4K 0x18
#define FOCCFG_FOC_POST_K_Kdiv2 0x04
#define FOCCFG_FOC_POST_FOC_LIMIT_BWdiv8 0x01

#define BSCFG_BS_PRE_KI_0Kl 0x00
#define BSCFG_BS_PRE_KP_2Kp 0x10
#define BSCFG_BS_POST_KI_KlDiv2 0x08
#define BSCFG_BS_POST_KP_KP 0x04

#define AGCCTRL2_MAX_DVGA_GAIN 0xC0
#define AGCCTRL2_MAGN_TARGET_42 0x07

#define AGCCTRL1_AGC_LNA_PRIORITY_0 0x00

#define AGCCTRL0_HYST_LEVEL_MEDIUM 0x80
#define AGCCTRL0_WAIT_TIME_32 0x30

#define FREND1_LNA_CURRENT_2 0x80
#define FREND1_LNA2MIX_CURRENT_3 0x30
#define FREND1_LODIV_BUF_CURRENT_RX_1 0x04
#define FREND1_MIX_CURRENT_2 0x02

#define FREND0_LODIV_BUF_CURRENT_TX_1 0x10

#define FSCAL3_FSCAL3_HIGH_3 0xC0
#define FSCAL3_CHP_CURR_CAL_EN 0x20
#define FSCAL3_FSCAL3_LOW_A 0x0A

#define FSCAL2_FSCAL2_DEFAULT 0x0A

#define FSCAL1_FSCAL1_0 0x00

#define FSCAL0_FSCAL0_17 0x11

#define TEST2_136 0x88
#define TEST1_49 0x31

#define TEST0_HIGH_2 0x08
#define TEST0_VCO_SEL_CAL_EN 0x02
#define TEST0_LOW_1 0x01

struct PortAndPin {
	GPIO_TypeDef *GPIOx;
	uint16_t GPIO_Pin;
	IRQn_Type InterruptIRQ;
};

uint8_t CC2500_WriteByteSPI(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t writevalue);
uint8_t CC2500_ReadRegister(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t registerAddress);
uint8_t CC2500_WriteRegister(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t registerAddress, uint8_t data);
void CC2500_WriteReadBytesSPI(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t* writeValues, uint8_t* readValues, uint8_t length);

void CC2500_Reset(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_SetPacketLength(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t length);
uint8_t CC2500_GetPacketLength(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetChipPartNumber(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetChipVersion(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
//void CC2500_SetGDO0OutputPinConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetGDO0OutputPinConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
uint8_t CC2500_GetGDO0OutputPinConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_SetGDO2OutputPinConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetMainRadioControlStateMachineConfiguration0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetMainRadioControlStateMachineConfiguration1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetPacketAutomationControl(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetOutputPower(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
uint8_t CC2500_GetOutputPower(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_SetChannelNumber(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencySynthesizerControl0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencySynthesizerControl1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencyHighByte(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencyMiddleByte(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencyLowByte(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetModemConfiguration4(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetModemConfiguration3(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetModemConfiguration2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetModemConfiguration1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetModemConfiguration0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetModemDeviationSetting(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencyOffsetConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetBitSynchronizationConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetAGCControl2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetAGCControl1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetAGCControl0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrontEndRXConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrontEndTXConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencySynthesizerCalibration3(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencySynthesizerCalibration2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencySynthesizerCalibration1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetFrequencySynthesizerCalibration0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetTest2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetTest1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_SetTest0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data);
void CC2500_ExitRXTX(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetStatusByteWrite(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetStatusByteRead(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_FlushRXFIFO(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_FlushTXFIFO(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_EnableRX(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_EnableTX(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetGDOxStatusAndPacketStatus(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
bool CC2500_GetIsReadyAndIdle(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
bool CC2500_GetIsReadyAndRXFifoOverflow(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
bool CC2500_GetIsReadyAndTXFifoUnderflow(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetRXBytesOverflowAndNumberOfBytes(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetNoOfRXBytes(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetNoOfTXBytes(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
uint8_t CC2500_GetRSSI(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin);
void CC2500_ReadRXFifo(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t* readValues, uint8_t length);
void CC2500_WriteTXFifo(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t* txBytes, uint8_t length);

#endif /* SRC_CC2500_H_ */
