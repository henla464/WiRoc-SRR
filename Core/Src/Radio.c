/*
 * Radio.c
 *
 * Radio module — CC2500 send/receive, GDO0 GPIO config,
 * packet assembly, TX queue processing.
 */

#include "main.h"
#include "Radio.h"
#include "I2CSlave.h"
#include "ErrorLog.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

bool volatile RedChannelSyncWordInterrupt = false;
bool volatile RedChannelSyncWordDetected = false;
bool volatile BlueChannelSyncWordInterrupt = false;
bool volatile BlueChannelSyncWordDetected = false;
bool volatile txInProgress = false;  // true while a CC2500 TX is in progress on either channel
struct PortAndPin RedChannelChipSelectPortPin = { .GPIOx = GPIOA, .GPIO_Pin = GPIO_PIN_15, .InterruptIRQ = EXTI4_15_IRQn, .Channel = REDCHANNEL};
struct PortAndPin BlueChannelChipSelectPortPin = { .GPIOx = GPIOA, .GPIO_Pin = GPIO_PIN_5, .InterruptIRQ = EXTI4_15_IRQn, .Channel = BLUECHANNEL};

uint8_t PunchReplySequenceNo = 1;
uint8_t PunchReply[] = {0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x23, 0x00, 0x73, 0x60}; // first byte is SPI header byte

#define TX_RADIO_PACKET_MAX_SIZE 32
static uint8_t txRadioPacket[TX_RADIO_PACKET_MAX_SIZE];

struct Punch punch;

void ReconfigureCC2500(void);
static void InitCC2500(SPI_HandleTypeDef* phspi, struct PortAndPin * chipSelectPin ,uint8_t channel);
static uint8_t GetPunchReplyIncludingSpaceForCommandByte(struct Punch punch, uint8_t * punchReply);
static void ResumeRX_RedChannel(void);
static void ResumeRX_BlueChannel(void);
static void SendAckReply_RedChannel(void);
static void SendAckReply_BlueChannel(void);
static void Configure_GDO_INT_1_AsGPIO(void);
static void Configure_GDO_INT_2_AsGPIO(void);
void InitializeBothCC2500(void);
static void Configure_GDO_INT_1_AsRisingInterrupt(void);
static void Configure_GDO_INT_1_AsFallingInterrupt(void);
static void Configure_GDO_INT_2_AsFallingInterrupt(void);
static uint8_t BuildRadioPacketFromTxPunch(struct TxPunch * txPunch, uint8_t msgSeq);
static bool SendPunch_RedChannel(struct TxPunch * txPunch, uint8_t msgSeq);
static bool SendPunch_BlueChannel(struct TxPunch * txPunch, uint8_t msgSeq);
void ProcessOutgoingPunches(void);

static uint8_t ChooseChannelForPunch(uint8_t lastSentChannel)
{
	uint8_t channel;
	if (lastSentChannel == 0)
	{
		// First attempt: use the channel that last got an ACK
		channel = txLastAckedChannel;
	}
	else if (lastSentChannel == REDCHANNEL)
	{
		channel = BLUECHANNEL;
	}
	else
	{
		channel = REDCHANNEL;
	}

	// Fallback: if chosen channel is disabled, try the other
	if (channel == REDCHANNEL && !IsRedChannelEnabled())
	{
		channel = BLUECHANNEL;
	}
	if (channel == BLUECHANNEL && !IsBlueChannelEnabled())
	{
		channel = REDCHANNEL;
	}

	return channel;
}

static void Configure_GDO_INT_1_AsRisingInterrupt()
{
	RedChannelSyncWordInterrupt = false;
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	// Configure GPIO pins : PA4
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// EXTI interrupt init
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
}


static void Configure_GDO_INT_1_AsFallingInterrupt()
{
	RedChannelSyncWordInterrupt = true;
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
	RedChannelSyncWordInterrupt = false;
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}


static void Configure_GDO_INT_2_AsRisingInterrupt()
{
	BlueChannelSyncWordInterrupt = false;
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA1 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
}

static void Configure_GDO_INT_2_AsFallingInterrupt()
{
	BlueChannelSyncWordInterrupt = true;
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
	BlueChannelSyncWordInterrupt = false;
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/*Configure GPIO pins : PA1 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void InitializeBothCC2500()
{
	InitCC2500(&hspi1, &RedChannelChipSelectPortPin, REDCHANNEL);
	InitCC2500(&hspi2, &BlueChannelChipSelectPortPin, BLUECHANNEL);
	Configure_GDO_INT_1_AsFallingInterrupt();
	Configure_GDO_INT_2_AsFallingInterrupt();
}

void ReconfigureCC2500() {
	HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
	txInProgress = false;  // reconfig aborts any in-progress TX
	Configure_GDO_INT_1_AsGPIO();
	Configure_GDO_INT_2_AsGPIO();
	if (IsRedChannelEnabled())
	{
		ErrorLog_log("ConfigureCC2500", "Red");
		InitCC2500(&hspi1, &RedChannelChipSelectPortPin, REDCHANNEL);
		Configure_GDO_INT_1_AsFallingInterrupt();
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
		ErrorLog_log("ConfigureCC2500", "Blue");
		InitCC2500(&hspi2, &BlueChannelChipSelectPortPin, BLUECHANNEL);
		Configure_GDO_INT_2_AsFallingInterrupt();
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

	if (IsRedChannelEnabled() || IsBlueChannelEnabled())
	{
		HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
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
}

static void FlushRXFifo(SPI_HandleTypeDef* phspi, struct PortAndPin * chipSelectPortPin)
{
	CC2500_ExitRXTX(phspi, chipSelectPortPin);
	CC2500_FlushRXFIFO(phspi, chipSelectPortPin);
	CC2500_EnableRX(phspi, chipSelectPortPin);
}

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


// Max size = 1 + 1 + 4 + 4 + 1 + 1 + 1 + 1 + 3 + TXPUNCH_MAX_PAYLOAD_SIZE = 32

static uint8_t BuildRadioPacketFromTxPunch(struct TxPunch * txPunch, uint8_t msgSeq)
{
	// Byte 0 is placeholder for SPI command (will be set to 0x7F by WriteTXFifo)
	uint8_t idx = 1;
	// Length byte: count of bytes after this byte (15 header + payload)
	uint8_t length = 15 + txPunch->payloadLength;
	txRadioPacket[idx++] = length;
	// Destination address: "siok" = 0x73696F6B (fixed magic address)
	txRadioPacket[idx++] = 0x73;
	txRadioPacket[idx++] = 0x69;
	txRadioPacket[idx++] = 0x6F;
	txRadioPacket[idx++] = 0x6B;
	// Source address: I2CSlave_serialNumber (dongle ID)
	txRadioPacket[idx++] = I2CSlave_serialNumber[0];
	txRadioPacket[idx++] = I2CSlave_serialNumber[1];
	txRadioPacket[idx++] = I2CSlave_serialNumber[2];
	txRadioPacket[idx++] = I2CSlave_serialNumber[3];
	// PORT
	txRadioPacket[idx++] = 0x3F;
	// Device Info
	txRadioPacket[idx++] = 0x03;
	// Message sequence number (passed in pre-incremented from caller)
	txRadioPacket[idx++] = msgSeq;
	// Punch sequence number (incremented for each NEW punch sent)
	txRadioPacket[idx++] = txPunchSequenceNumber;
	// don't know the meaning of these two bytes
	txRadioPacket[idx++] = 0x62;
	txRadioPacket[idx++] = 0x6A;
	// Punch from SRR unit and from SI Air card different, SRR unit sends 0xB6
	txRadioPacket[idx++] = 0xB6;
	// TxPunch payload (already contains record type, length, CN1, CN0, SI#, etc.)
	for (uint8_t i = 0; i < txPunch->payloadLength; i++)
	{
		txRadioPacket[idx++] = txPunch->payload[i];
	}
	return idx; // total bytes in buffer including command placeholder
}


struct Punch punch;
static bool ReadMessage(SPI_HandleTypeDef* phspi, struct PortAndPin * chipSelectPortPin)
{
	uint8_t noOfRxBytes1 = 0;
	uint8_t noOfRxBytes2 = 0;
	do {
		noOfRxBytes1 = CC2500_GetNoOfRXBytes(phspi, chipSelectPortPin);
		if (noOfRxBytes1 == 0) {
			// When the length doesn't make sense flush
			FlushRXFifo(phspi, chipSelectPortPin);
			return false;
		}
		noOfRxBytes2 = CC2500_GetNoOfRXBytes(phspi, chipSelectPortPin);
	} while (noOfRxBytes1 != noOfRxBytes2);

	// read the length byte
	if (!CC2500_ReadRXFifo(phspi, chipSelectPortPin, &punch.payloadLength, 1))
	{
		ErrorLog_log("ReadMessage", "CC2500_ReadRXFifo ret false (1)");
		FlushRXFifo(phspi, chipSelectPortPin);
		return false;
	}

	uint8_t headerLength = 1; // the length byte
	uint8_t footerLength = 2; // the rssi and crc/link quality bytes
	uint8_t lengthOfMessageReceived = punch.payloadLength + headerLength + footerLength;
	if (noOfRxBytes2 >= lengthOfMessageReceived && punch.payloadLength <= sizeof(punch.payload))
	{
		// Full message received, and it fits into the punch.payload array!
		if (!CC2500_ReadRXFifo(phspi, chipSelectPortPin, punch.payload, punch.payloadLength))
		{
			ErrorLog_log("ReadMessage", "CC2500_ReadRXFifo ret false (2)");
			FlushRXFifo(phspi, chipSelectPortPin);
			return false;
		}
		if (!CC2500_ReadRXFifo(phspi, chipSelectPortPin, (uint8_t *)&punch.messageStatus, footerLength))
		{
			ErrorLog_log("ReadMessage", "CC2500_ReadRXFifo ret false (3)");
			FlushRXFifo(phspi, chipSelectPortPin);
			return false;
		}
		if (punch.messageStatus.crc & 0x80)
		{
			// CRC OK
			punch.channel = chipSelectPortPin->Channel;

			// Check if this is an ACK addressed to us
			// ACK format: length 14, destination = our serial number
			if (punch.payloadLength == 14
				&& memcmp(punch.payload, (const void *)I2CSlave_serialNumber, 4) == 0)
			{
				// Only pop if this ACK matches the front punch's last TX channel
				struct TxPunch * txPunch = TxPunchQueue_peekPtr(&outgoingTxPunchQueue);
				if (txPunch != NULL && txPunch->lastSentChannel == chipSelectPortPin->Channel)
				{
					TxPunchQueue_pop(&outgoingTxPunchQueue);
					txLastAckedChannel = chipSelectPortPin->Channel;
				}
				// don't enqueue ACKs, don't send ACK reply back
				return false;
			}

			// Not an ACK — enqueue as incoming punch
			uint8_t enqueueResult = PunchQueue_enQueue(&incomingPunchQueue, &punch);
			if (enqueueResult == QUEUEISFULL)
			{
				// queue full so don't ack
				ErrorLog_log("ReadMessage", "Queue full");
				FlushRXFifo(phspi, chipSelectPortPin);
				return false;
			}
			// when enqueueResult is  ENQUEUESUCCESS or SAMEPUNCH then punch should be acked (unless in listen only mode)
		} else {
			FlushRXFifo(phspi, chipSelectPortPin);
			return false;
		}
	} else {
		char msg[100];
		sprintf(msg, "Received too few or too many bytes: %u channel: %u payloadlength: %u", noOfRxBytes2, chipSelectPortPin->Channel, punch.payloadLength);
		FlushRXFifo(phspi, chipSelectPortPin);
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
	if (!CC2500_WriteTXFifo(&hspi1, &RedChannelChipSelectPortPin, PunchReply, replyLength))
	{
		ErrorLog_log("SendAckReply_RedChannel", "WriteTXFifo failed");
		ResumeRX_RedChannel();
		return;
	}

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
	if (!CC2500_WriteTXFifo(&hspi2, &BlueChannelChipSelectPortPin, PunchReply, replyLength))
	{
		ErrorLog_log("SendAckReply_BlueChannel", "WriteTXFifo failed");
		ResumeRX_BlueChannel();
		return;
	}

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


/*===========================================================================*/
/*  SendPunch — transmit a TxPunch over CC2500 (I2C → radio)                */
/*===========================================================================*/

static bool SendPunch_RedChannel(struct TxPunch * txPunch, uint8_t msgSeq)
{
	CC2500_ExitRXTX(&hspi1, &RedChannelChipSelectPortPin);
	do {
	} while (!CC2500_GetIsReadyAndIdle(&hspi1, &RedChannelChipSelectPortPin));
	CC2500_FlushRXFIFO(&hspi1, &RedChannelChipSelectPortPin);

	uint8_t packetLength = BuildRadioPacketFromTxPunch(txPunch, msgSeq);
	if (!CC2500_WriteTXFifo(&hspi1, &RedChannelChipSelectPortPin, txRadioPacket, packetLength - 1))
	{
		ErrorLog_log("SendPunch_RedChannel", "WriteTXFifo failed");
		return false;
	}

	// Switch GDO0 to PA_PD so we get a rising edge when TX completes
	Configure_GDO_INT_1_AsGPIO();
	CC2500_SetGDO0OutputPinConfiguration(&hspi1, &RedChannelChipSelectPortPin, GDOx_CFG_PA_PD);

	CC2500_EnableRX(&hspi1, &RedChannelChipSelectPortPin);

	// Wait for channel clear (CCA)
	uint8_t packetStatus;
	do {
		packetStatus = CC2500_GetGDOxStatusAndPacketStatus(&hspi1, &RedChannelChipSelectPortPin);
	} while (!(packetStatus & 0x10));

	// Enable rising interrupt for TX-complete detection
	Configure_GDO_INT_1_AsRisingInterrupt();
	HAL_NVIC_EnableIRQ(RedChannelChipSelectPortPin.InterruptIRQ);

	txInProgress = true;
	CC2500_EnableTX(&hspi1, &RedChannelChipSelectPortPin);

	txPunch->lastSentChannel = REDCHANNEL;
	return true;
}

static bool SendPunch_BlueChannel(struct TxPunch * txPunch, uint8_t msgSeq)
{
	CC2500_ExitRXTX(&hspi2, &BlueChannelChipSelectPortPin);
	do {
	} while (!CC2500_GetIsReadyAndIdle(&hspi2, &BlueChannelChipSelectPortPin));
	CC2500_FlushRXFIFO(&hspi2, &BlueChannelChipSelectPortPin);

	uint8_t packetLength = BuildRadioPacketFromTxPunch(txPunch, msgSeq);
	if (!CC2500_WriteTXFifo(&hspi2, &BlueChannelChipSelectPortPin, txRadioPacket, packetLength - 1))
	{
		ErrorLog_log("SendPunch_BlueChannel", "WriteTXFifo failed");
		return false;
	}

	// Switch GDO0 to PA_PD so we get a rising edge when TX completes
	Configure_GDO_INT_2_AsGPIO();
	CC2500_SetGDO0OutputPinConfiguration(&hspi2, &BlueChannelChipSelectPortPin, GDOx_CFG_PA_PD);

	CC2500_EnableRX(&hspi2, &BlueChannelChipSelectPortPin);

	// Wait for channel clear (CCA)
	uint8_t packetStatus;
	do {
		packetStatus = CC2500_GetGDOxStatusAndPacketStatus(&hspi2, &BlueChannelChipSelectPortPin);
	} while (!(packetStatus & 0x10));

	// Enable rising interrupt for TX-complete detection
	Configure_GDO_INT_2_AsRisingInterrupt();
	HAL_NVIC_EnableIRQ(BlueChannelChipSelectPortPin.InterruptIRQ);

	txInProgress = true;
	CC2500_EnableTX(&hspi2, &BlueChannelChipSelectPortPin);

	txPunch->lastSentChannel = BLUECHANNEL;
	return true;
}

void ProcessOutgoingPunches(void)
{
	if (!IsInSendMode())
	{
		return;
	}
	if (txInProgress)
	{
		// Previous TX still in progress, wait for rising ISR
		return;
	}

	// Snapshot queue state — minimal IRQ-disabled window
	HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
	if (TxPunchQueue_isEmpty(&outgoingTxPunchQueue))
	{
		HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
		return;
	}
	struct TxPunch * txPunch = TxPunchQueue_peekPtr(&outgoingTxPunchQueue);
	if (txPunch == NULL)
	{
		HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
		return;
	}
	uint8_t retryCount = txPunch->retryCount;
	uint32_t nextRetryTick = txPunch->nextRetryTick;
	uint8_t lastSentChannel = txPunch->lastSentChannel;
	HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

	// Max retries exhausted — wait for final ACK grace period, then pop
	if (retryCount >= MAX_TX_RETRIES)
	{
		if (HAL_GetTick() >= nextRetryTick)
		{
			HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
			TxPunchQueue_pop(&outgoingTxPunchQueue);
			HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
		}
		return;
	}

	// Exponential backoff: 64, 128, 256, 512, 1024 ms between retries
	if (retryCount > 0 && HAL_GetTick() < nextRetryTick)
	{
		return;
	}

	uint8_t channel = ChooseChannelForPunch(lastSentChannel);

	if ((channel == REDCHANNEL && !IsRedChannelEnabled()) ||
	    (channel == BLUECHANNEL && !IsBlueChannelEnabled()))
	{
		// Neither channel is enabled, abandon this punch
		HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
		TxPunchQueue_pop(&outgoingTxPunchQueue);
		HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
		return;
	}

	// Increment punch sequence number on first send attempt (not on retries)
	if (retryCount == 0)
	{
		txPunchSequenceNumber++;
	}

	txMessageSequenceNumber++;

	// Re-disable interrupts for SPI and queue operations
	HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);

	bool success;
	if (channel == REDCHANNEL)
	{
		success = SendPunch_RedChannel(txPunch, txMessageSequenceNumber);
	}
	else
	{
		success = SendPunch_BlueChannel(txPunch, txMessageSequenceNumber);
	}

	if (success)
	{
		txPunch->retryCount++;
		txPunch->lastSentChannel = channel;

		// Schedule next retry with exponential backoff
		// Inter-retry delays: 64, 128, 256, 512, 1024 ms
		// After final retry: 64 ms ACK grace period before abandoning
		uint32_t delayMs;
		if (txPunch->retryCount < MAX_TX_RETRIES)
		{
			delayMs = 64U << (txPunch->retryCount - 1);
		}
		else
		{
			delayMs = 64;
		}
		txPunch->nextRetryTick = HAL_GetTick() + delayMs;
	}
	else
	{
		// TX init failed — re-enable IRQ that SendPunch_*Channel didn't
		HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
	}

	// If success, ISR was re-enabled inside SendPunch_*Channel above;
	// the rising ISR will call ResumeRX to clear txInProgress
	// and switch back to RX mode
}


static void ResumeRX_RedChannel()
{
	// must be in idle, but is probably in RX now...
	txInProgress = false;
	CC2500_FlushTXFIFO(&hspi1, &RedChannelChipSelectPortPin);
	Configure_GDO_INT_1_AsGPIO();
	CC2500_SetGDO0OutputPinConfiguration(&hspi1, &RedChannelChipSelectPortPin, GDOx_CFG_ASSERT_SYNC_WORD);
	//Configure_GDO0 as falling interrupt
	Configure_GDO_INT_1_AsFallingInterrupt();
	HAL_NVIC_EnableIRQ(RedChannelChipSelectPortPin.InterruptIRQ);
	CC2500_EnableRX(&hspi1, &RedChannelChipSelectPortPin);
}

static void ResumeRX_BlueChannel()
{
	// must be in idle, but is probably in RX now...
	txInProgress = false;
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
			// We should reset RedChannelSyncWordDetected now that we received
			// a falling edge and interrupt will not be stuck high
			RedChannelSyncWordDetected = false;
			if (IsRedChannelEnabled()) {

				ReadMessage_RedChannel();
			} else {
				// If radio is active in receive but channel should be off then it is
				// probably good to flush the RX fifo
				uint8_t noOfRxBytes1 = 0;
				uint8_t noOfRxBytes2 = 0;
				do {
					noOfRxBytes1 = CC2500_GetNoOfRXBytes(&hspi1, &RedChannelChipSelectPortPin);
					if (noOfRxBytes1 == 0) {
						break;
					}
					noOfRxBytes2 = CC2500_GetNoOfRXBytes(&hspi1, &RedChannelChipSelectPortPin);
				} while (noOfRxBytes1 != noOfRxBytes2);
				FlushRXFifo(&hspi1, &RedChannelChipSelectPortPin);
			}

		}
		else if(GPIO_Pin == GPIO_PIN_6) // PA6 - second CC2500
		{
			// We should reset BlueChannelSyncWordDetected now that we received
			// a falling edge and interrupt will not be stuck high
			BlueChannelSyncWordDetected = false;
			if (IsBlueChannelEnabled()) {

				ReadMessage_BlueChannel();
			} else {
				// If radio is active in receive but channel should be off then it is
				// probably good to flush the RX fifo
				uint8_t noOfRxBytes1 = 0;
				uint8_t noOfRxBytes2 = 0;
				do {
					noOfRxBytes1 = CC2500_GetNoOfRXBytes(&hspi2, &BlueChannelChipSelectPortPin);
					if (noOfRxBytes1 == 0) {
						break;
					}
					noOfRxBytes2 = CC2500_GetNoOfRXBytes(&hspi2, &BlueChannelChipSelectPortPin);
				} while (noOfRxBytes1 != noOfRxBytes2);
				FlushRXFifo(&hspi2, &BlueChannelChipSelectPortPin);
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

				ResumeRX_RedChannel();
			}
		}
		else if(GPIO_Pin == GPIO_PIN_6) // PA6 - second CC2500
		{
			if (IsBlueChannelEnabled()) {

				ResumeRX_BlueChannel();
			}
		}
	}
}
