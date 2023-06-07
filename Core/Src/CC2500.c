#include "CC2500.h"
#include "main.h"
#include "string.h"
#include "ErrorLog.h"

uint8_t CC2500_WriteByteSPI(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t writevalue)
{
	uint8_t readvalue = 0;
	uint8_t status = 0;
	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_RESET);
	if ( (status = HAL_SPI_TransmitReceive(hspi, (uint8_t*) &writevalue, (uint8_t*) &readvalue, 1, 5 )) != HAL_OK)
	{
		char msg[50];
		sprintf(msg, "HAL_SPI_TransmitReceive returned: %u \r\n", status);
		ErrorLog_log("CC2500_WriteByteSPI", msg);
		Error_Handler();
	}
	while( hspi->State == HAL_SPI_STATE_BUSY );
	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_SET);
	return readvalue;
}

uint8_t CC2500_ReadRegister(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t registerAddress)
{
	uint8_t readValues[2] = {0};
	uint8_t writeValues[2] = { registerAddress | 0xC0, 0xDB};
	uint8_t status = 0;

	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_RESET);
	if ( (status = HAL_SPI_TransmitReceive(hspi, writeValues, readValues, 2, 5 )) != HAL_OK)
	{
		char msg[50];
		sprintf(msg, "HAL_SPI_TransmitReceive returned: %u \r\n", status);
		ErrorLog_log("CC2500_ReadRegister", msg);
		Error_Handler();
	}
	while( hspi->State == HAL_SPI_STATE_BUSY );

	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_SET);
	return readValues[1];
}

uint8_t CC2500_WriteRegister(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t registerAddress, uint8_t data)
{
	uint8_t readValues[2] = {0};
	uint8_t writeValues[2] = { registerAddress | 0x40, data};
	uint8_t status = 0;

	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_RESET);
	if ( (status = HAL_SPI_TransmitReceive(hspi, writeValues, readValues, 2, 5 )) != HAL_OK)
	{
		char msg[50];
		sprintf(msg, "HAL_SPI_TransmitReceive returned: %u \r\n", status);
		ErrorLog_log("CC2500_WriteRegister", msg);
		Error_Handler();
	}
	while( hspi->State == HAL_SPI_STATE_BUSY );

	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_SET);
	return readValues[1];
}



bool CC2500_WriteReadBytesSPI(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t* writeValues, uint8_t* readValues, uint8_t length)
{
	uint8_t status = 0;
	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_RESET);
	if ( (status = HAL_SPI_TransmitReceive(hspi, writeValues, readValues, length, 100 )) != HAL_OK)
	{
		char msg[50];
		sprintf(msg, "HAL_SPI_TransmitReceive returned: %u \r\n", status);
		ErrorLog_log("CC2500_WriteReadBytesSPI", msg);
		return false;
	}
	while( hspi->State == HAL_SPI_STATE_BUSY );
	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_SET);
	return true;
}


void CC2500_SetGDO2OutputPinConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x00, data);
}

void CC2500_SetGDO0OutputPinConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x02, data);
}

uint8_t CC2500_GetGDO0OutputPinConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t status = CC2500_ReadRegister(hspi,  chipSelectPin, 0x02);
	return status;
}

void CC2500_SetPacketLength(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t length)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x06, length);
}

void CC2500_SetPacketAutomationControl(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x08, data);
}

void CC2500_SetMainRadioControlStateMachineConfiguration1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x17, data);
}

void CC2500_SetMainRadioControlStateMachineConfiguration0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x18, data);
}

void CC2500_SetChannelNumber(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x0A, data);
}

void CC2500_SetFrequencySynthesizerControl1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x0B, data);
}

void CC2500_SetFrequencySynthesizerControl0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x0C, data);
}

void CC2500_SetFrequencyHighByte(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x0D, data);
}

void CC2500_SetFrequencyMiddleByte(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x0E, data);
}

void CC2500_SetFrequencyLowByte(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x0F, data);
}

void CC2500_SetModemConfiguration4(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x10, data);
}

void CC2500_SetModemConfiguration3(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x11, data);
}

void CC2500_SetModemConfiguration2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x12, data);
}

void CC2500_SetModemConfiguration1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x13, data);
}

void CC2500_SetModemConfiguration0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x14, data);
}

void CC2500_SetModemDeviationSetting(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x15, data);
}

void CC2500_SetFrequencyOffsetConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x19, data);
}

void CC2500_SetBitSynchronizationConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x1A, data);
}

void CC2500_SetAGCControl2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x1B, data);
}

void CC2500_SetAGCControl1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x1C, data);
}

void CC2500_SetAGCControl0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x1D, data);
}

void CC2500_SetFrontEndRXConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x21, data);
}

void CC2500_SetFrontEndTXConfiguration(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x22, data);
}

void CC2500_SetFrequencySynthesizerCalibration3(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x23, data);
}

void CC2500_SetFrequencySynthesizerCalibration2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x24, data);
}

void CC2500_SetFrequencySynthesizerCalibration1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x25, data);
}

void CC2500_SetFrequencySynthesizerCalibration0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x26, data);
}

void CC2500_SetTest2(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x2C, data);
}

void CC2500_SetTest1(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x2D, data);
}

void CC2500_SetTest0(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	CC2500_WriteRegister(hspi, chipSelectPin, 0x2E, data);
}

void CC2500_Reset(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	CC2500_WriteByteSPI(hspi, chipSelectPin, 0x30);
}

void CC2500_EnableRX(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	CC2500_WriteByteSPI(hspi, chipSelectPin, 0x34);
}

void CC2500_EnableTX(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	CC2500_WriteByteSPI(hspi, chipSelectPin, 0x35);
}

void CC2500_ExitRXTX(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	CC2500_WriteByteSPI(hspi, chipSelectPin, 0x36);
}

void CC2500_FlushRXFIFO(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	CC2500_WriteByteSPI(hspi, chipSelectPin, 0x3A);
}

void CC2500_FlushTXFIFO(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	CC2500_WriteByteSPI(hspi, chipSelectPin, 0x3B);
}

uint8_t CC2500_GetStatusByteWrite(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	return CC2500_WriteByteSPI(hspi, chipSelectPin, 0x3D);
}

bool CC2500_GetIsReadyAndIdle(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	return (CC2500_GetStatusByteWrite(hspi, chipSelectPin) & 0xF0) == 0x00;
}

bool CC2500_GetIsReadyAndRXFifoOverflow(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	return (CC2500_GetStatusByteWrite(hspi, chipSelectPin) & 0xF0) == 0x60;
}

bool CC2500_GetIsReadyAndTXFifoUnderflow(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	return (CC2500_GetStatusByteWrite(hspi, chipSelectPin) & 0xF0) == 0x70;
}

void CC2500_SetOutputPower(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t data)
{
	// make sure the counter for the PATABLE index is reset to 0
	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_SET);
	CC2500_WriteRegister(hspi, chipSelectPin, 0x3E, data);
}

uint8_t CC2500_GetOutputPower(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	// make sure the counter for the PATABLE index is reset to 0
	HAL_GPIO_WritePin(chipSelectPin->GPIOx, chipSelectPin->GPIO_Pin, GPIO_PIN_SET);
	uint8_t paTableIndex0 = CC2500_ReadRegister(hspi, chipSelectPin, 0x3E);
	return paTableIndex0;
}

uint8_t CC2500_GetStatusByteRead(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	return CC2500_WriteByteSPI(hspi, chipSelectPin, 0xBD);
}

uint8_t CC2500_GetRSSI(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t rssi = CC2500_ReadRegister(hspi, chipSelectPin, 0x34);
	return rssi;
}

uint8_t CC2500_GetPacketLength(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t packetLength = CC2500_ReadRegister(hspi, chipSelectPin, 0x06);
	return packetLength;
}

uint8_t CC2500_GetChipPartNumber(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t partNumber = CC2500_ReadRegister(hspi, chipSelectPin, 0x30);
	return partNumber;
}


uint8_t CC2500_GetChipVersion(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t version = CC2500_ReadRegister(hspi, chipSelectPin, 0x31);
	return version;
}

uint8_t CC2500_GetGDOxStatusAndPacketStatus(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t status = CC2500_ReadRegister(hspi, chipSelectPin, 0x38);
	return status;
}

uint8_t CC2500_GetRXBytesOverflowAndNumberOfBytes(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t status = CC2500_ReadRegister(hspi, chipSelectPin, 0x3B);
	return status;
}

uint8_t CC2500_GetNoOfRXBytes(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t status = CC2500_ReadRegister(hspi, chipSelectPin, 0x3B);
	return status & 0x7F;
}

uint8_t CC2500_GetNoOfTXBytes(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin)
{
	uint8_t status = CC2500_ReadRegister(hspi, chipSelectPin, 0x3A);
	return status & 0x7F;
}

uint8_t writeBuffer[100] = {0};
uint8_t readBuffer[100] = {0};
bool CC2500_ReadRXFifo(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t* readValues, uint8_t length)
{
	writeBuffer[0] = 0xFF; // burst read RX FIFO
	if (!CC2500_WriteReadBytesSPI(hspi, chipSelectPin, (uint8_t *) writeBuffer, readValues, length + 1))
	{
		return false;
	}
	for(uint8_t i=0; i < length; i++)
	{
		readValues[i] = readValues[i+1];
	}
	readValues[length] = 0x00;
	return true;
}

bool CC2500_WriteTXFifo(SPI_HandleTypeDef* hspi, struct PortAndPin * chipSelectPin, uint8_t* txBytes, uint8_t length)
{
	txBytes[0] = 0x7F;
	if (!CC2500_WriteReadBytesSPI(hspi, chipSelectPin, txBytes, readBuffer, length+1))
	{
		return false;
	}
	return true;
}
