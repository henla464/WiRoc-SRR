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

// Safety bound for every CC2500 polling wait loop. One loop iteration is one or
// two byte SPI transactions at ~4 MHz, so 10000 iterations is ~50-100 ms. These
// waits run both in the main loop and inside the EXTI4_15 ISR; EXTI4_15 is at
// priority 0, the same as I2C1, so an unbounded wait there freezes the whole
// firmware and the I2C slave stops answering.
#define RADIO_POLL_MAX_LOOPS 10000U

#ifdef TEST_MODES_ENABLED
#define RADIO_READY_IDLE_MAX_LOOPS 1000U
#define CARRIER_FILL_BYTES 64
// Carrier fill data. Byte 0 is the SPI burst-write command placeholder that
// CC2500_WriteTXFifo() overwrites with 0x7F; bytes 1..64 are the constant data.
static uint8_t cwFillPacket[CARRIER_FILL_BYTES + 1];

// Test mode 2 (TX_AA_LOOP) packet: a normal variable-length packet whose data
// field is filled with 0xAA. Layout: byte 0 = SPI command placeholder, byte 1 =
// length byte, bytes 2..64 = 0xAA data. 63 data bytes keeps length+data within
// the 64-byte TX FIFO and is longer than a normal punch (~30 bytes).
#define AA_LOOP_DATA_BYTES 63U
#define AA_LOOP_INTER_PACKET_DELAY_MS 0U
static uint8_t aaFillPacket[AA_LOOP_DATA_BYTES + 2];

// Test mode 3 (TX_NORMAL_5S): enqueue a normal punch into the TX queue on an
// interval (default 5 s, configurable via I2C TESTMODE3DELAYREGADDR in tenths
// of a second) and let ProcessOutgoingPunches() send it through the normal path.
#define TEST_MODE3_PAYLOAD_LENGTH TXPUNCH_MAX_PAYLOAD_SIZE
static bool mode3SendLogged = false;  // one-shot "punch actually sent" diagnostic
#endif

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

			// Diagnostic: capture every CRC-OK packet for ACK debugging.
			{
				struct TxPunch * frontPunch = TxPunchQueue_peekPtr(&outgoingTxPunchQueue);
				bool serialMatch = (punch.payloadLength >= 4
					&& memcmp(punch.payload, (const void *)I2CSlave_serialNumber, 4) == 0);
				bool chanMatch = (frontPunch != NULL
					&& frontPunch->lastSentChannel == punch.channel);
				char msg[120];
				sprintf(msg,
					"len=%u ch=%u b=%02X%02X%02X%02X ser=%d chan=%d front=%u",
					(unsigned)punch.payloadLength, (unsigned)punch.channel,
					(unsigned)punch.payload[0], (unsigned)punch.payload[1],
					(unsigned)punch.payload[2], (unsigned)punch.payload[3],
					serialMatch ? 1 : 0, chanMatch ? 1 : 0,
					(frontPunch != NULL) ? (unsigned)frontPunch->lastSentChannel : 0);
				ErrorLog_log("ReadMessage", msg);
			}

			// Check if this is an ACK addressed to us
			// ACK format: length 14, destination = our serial number
			if (punch.payloadLength == 13
				&& memcmp(punch.payload, (const void *)I2CSlave_serialNumber, 4) == 0)
			{
				// Only pop if this ACK matches the front punch's last TX channel
				struct TxPunch * txPunch = TxPunchQueue_peekPtr(&outgoingTxPunchQueue);
				if (txPunch != NULL && txPunch->lastSentChannel == chipSelectPortPin->Channel)
				{
					TxPunchQueue_pop(&outgoingTxPunchQueue);
					txLastAckedChannel = chipSelectPortPin->Channel;
					txMessagesAcked++;
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
	if (!CC2500_WriteTXFifo(&hspi1, &RedChannelChipSelectPortPin, PunchReply, replyLength - 1))
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
	if (!CC2500_WriteTXFifo(&hspi2, &BlueChannelChipSelectPortPin, PunchReply, replyLength - 1))
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
#ifdef TEST_MODES_ENABLED
	if (IsTestSignalModeActive())
	{
		// Test modes 1 & 2 own the radios — no normal TX. Test mode 3 runs
		// through here so its queued punches are sent normally.
		return;
	}
#endif

	// Normal operation requires send mode (bit 5). In test mode 3 the periodic
	// punch must also go out without send mode, since the test mode itself is
	// the trigger for sending.
	bool sendGateOpen = IsInSendMode();
#ifdef TEST_MODES_ENABLED
	if (!sendGateOpen && IsTestModeEnabled() && GetTestMode() == TEST_MODE_TX_NORMAL_5S)
	{
		sendGateOpen = true;
	}
#endif
	if (!sendGateOpen)
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
		txMessagesSent++;
#ifdef TEST_MODES_ENABLED
		if (!mode3SendLogged && IsTestModeEnabled() && GetTestMode() == TEST_MODE_TX_NORMAL_5S)
		{
			char msg[40];
			sprintf(msg, "mode 3 punch sent on ch %u", (unsigned)channel);
			ErrorLog_log("ProcessOutgoingPunches", msg);
			mode3SendLogged = true;
		}
#endif
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


#ifdef TEST_MODES_ENABLED
/*===========================================================================*/
/*  Test modes — EU RED / EN 300 328 RF test support                         */
/*===========================================================================*/

// Transmit on the first enabled channel. For RF conformance testing, enable
// exactly the channel under test in the features register (0x06).
static uint8_t GetTestTxChannel(void)
{
	if (IsRedChannelEnabled())
	{
		return REDCHANNEL;
	}
	if (IsBlueChannelEnabled())
	{
		return BLUECHANNEL;
	}
	return 0;
}

static SPI_HandleTypeDef * TestChannelSpi(uint8_t channel)
{
	return (channel == REDCHANNEL) ? &hspi1 : &hspi2;
}

static struct PortAndPin * TestChannelPortPin(uint8_t channel)
{
	return (channel == REDCHANNEL) ? &RedChannelChipSelectPortPin : &BlueChannelChipSelectPortPin;
}

// GDO0 pin for a channel (both live on GPIOA). Used to poll TX completion when
// GDO0 is configured as PA_PD.
static uint16_t TestChannelGdoPin(uint8_t channel)
{
	return (channel == REDCHANNEL) ? GPIO_PIN_12 : GPIO_PIN_6;
}

typedef enum
{
	AA_LOOP_STATE_TX_ACTIVE,  // packet in flight; poll GDO0 for PA_PD high
	AA_LOOP_STATE_DELAY       // inter-packet delay; send next when it elapses
} AALoopState_t;

static uint8_t       aaLoopChannel = 0;
static AALoopState_t aaLoopState = AA_LOOP_STATE_TX_ACTIVE;
static uint32_t      aaLoopDelayEndTick = 0;

// Test mode 3 state. The punch is built once in StartNormalPeriodic() and
// re-enqueued (by value) every (GetTestMode3DelayTenths() * 100) ms in
// MaintainNormalPeriodic().
static struct TxPunch testMode3Punch;
static uint32_t periodicNextSendTick = 0;

// Test mode 1: continuous unmodulated carrier. A constant data stream into the
// MSK modulator produces a single tone. Infinite packet length + no preamble/
// sync means the transmitter keeps emitting the FIFO contents, so the FIFO is
// topped up continuously in MaintainCarrier().
static void StartCarrier(uint8_t channel)
{
	if (channel == 0)
	{
		ErrorLog_log("StartCarrier", "no channel enabled");
		return;
	}

	SPI_HandleTypeDef * phspi = TestChannelSpi(channel);
	struct PortAndPin * cs = TestChannelPortPin(channel);

	for (uint8_t i = 1; i < sizeof(cwFillPacket); i++)
	{
		cwFillPacket[i] = 0xFF;
	}

	// Stop RX interrupts while the radio is reconfigured for carrier mode.
	HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
	if (channel == REDCHANNEL)
	{
		Configure_GDO_INT_1_AsGPIO();
	}
	else
	{
		Configure_GDO_INT_2_AsGPIO();
	}

	CC2500_ExitRXTX(phspi, cs);
	uint32_t idleLoops = 0;
	do {
		if (++idleLoops > RADIO_READY_IDLE_MAX_LOOPS)
		{
			ErrorLog_log("StartCarrier", "chip not ready/idle");
			HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
			return;
		}
	} while (!CC2500_GetIsReadyAndIdle(phspi, cs));

	CC2500_FlushTXFIFO(phspi, cs);

	// Infinite packet length, no CRC/whitening, MSK with no sync word.
	CC2500_SetPacketAutomationControl(phspi, cs, PKTCTRL0_INFINITE_PACKET_LENGTH);
	CC2500_SetModemConfiguration2(phspi, cs, MDMCFG2_MOD_FORMAT_MSK | MDMCFG2_SYNC_MODE_NONE);

	if (!CC2500_WriteTXFifo(phspi, cs, cwFillPacket, sizeof(cwFillPacket) - 1))
	{
		ErrorLog_log("StartCarrier", "WriteTXFifo failed");
		HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
		return;
	}

	txInProgress = true;
	CC2500_EnableTX(phspi, cs);
	// EXTI stays disabled while the carrier runs; MaintainCarrier() keeps the
	// TX FIFO topped up from the main loop.
}

static void MaintainCarrier(void)
{
	uint8_t channel = GetTestTxChannel();
	if (channel == 0)
	{
		return;
	}

	SPI_HandleTypeDef * phspi = TestChannelSpi(channel);
	struct PortAndPin * cs = TestChannelPortPin(channel);

	// The 64-byte TX FIFO drains in ~2 ms at 250 kbps. Top it up before it
	// underflows so the carrier stays continuous.
	if (CC2500_GetNoOfTXBytes(phspi, cs) <= 32)
	{
		CC2500_WriteTXFifo(phspi, cs, cwFillPacket, 32);
	}
}

// Load the pre-built 0xAA packet into the TX FIFO and start transmission.
// Called once by StartAALoop() and then again from MaintainAALoop() after each
// inter-packet delay.
static void SendAAPacket(void)
{
	SPI_HandleTypeDef * phspi = TestChannelSpi(aaLoopChannel);
	struct PortAndPin * cs = TestChannelPortPin(aaLoopChannel);

	CC2500_ExitRXTX(phspi, cs);
	uint32_t idleLoops = 0;
	do {
		if (++idleLoops > RADIO_READY_IDLE_MAX_LOOPS)
		{
			ErrorLog_log("SendAAPacket", "chip not ready/idle");
			return;
		}
	} while (!CC2500_GetIsReadyAndIdle(phspi, cs));

	CC2500_FlushTXFIFO(phspi, cs);
	if (!CC2500_WriteTXFifo(phspi, cs, aaFillPacket, AA_LOOP_DATA_BYTES + 1))
	{
		ErrorLog_log("SendAAPacket", "WriteTXFifo failed");
		return;
	}
	CC2500_EnableTX(phspi, cs);
}

// Test mode 2: continuous loop of 0xAA-filled packets. Packets keep the normal
// format (sync word + variable length + CRC), but the data field is all-0xAA
// and longer than a normal punch. GDO0 is switched to PA_PD so completion can
// be polled (PA_PD goes high when the PA powers down after each packet).
static void StartAALoop(uint8_t channel)
{
	if (channel == 0)
	{
		ErrorLog_log("StartAALoop", "no channel enabled");
		return;
	}

	aaLoopChannel = channel;

	// Build the all-0xAA packet once; byte 1 is the variable-length byte.
	aaFillPacket[1] = AA_LOOP_DATA_BYTES;
	for (uint8_t i = 2; i < sizeof(aaFillPacket); i++)
	{
		aaFillPacket[i] = 0xAA;
	}

	SPI_HandleTypeDef * phspi = TestChannelSpi(channel);
	struct PortAndPin * cs = TestChannelPortPin(channel);

	// Stop RX interrupts while the loop runs; GDO0 becomes PA_PD for polling.
	HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
	if (channel == REDCHANNEL)
	{
		Configure_GDO_INT_1_AsGPIO();
	}
	else
	{
		Configure_GDO_INT_2_AsGPIO();
	}

	// Move to IDLE so GDO0 can be reconfigured.
	CC2500_ExitRXTX(phspi, cs);
	uint32_t idleLoops = 0;
	do {
		if (++idleLoops > RADIO_READY_IDLE_MAX_LOOPS)
		{
			ErrorLog_log("StartAALoop", "chip not ready/idle");
			HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
			return;
		}
	} while (!CC2500_GetIsReadyAndIdle(phspi, cs));

	CC2500_SetGDO0OutputPinConfiguration(phspi, cs, GDOx_CFG_PA_PD);

	txInProgress = true;
	aaLoopState = AA_LOOP_STATE_TX_ACTIVE;
	SendAAPacket();
}

static void MaintainAALoop(void)
{
	if (aaLoopChannel == 0)
	{
		return;
	}

	if (aaLoopState == AA_LOOP_STATE_TX_ACTIVE)
	{
		// GDO0 = PA_PD is high when the PA is powered down, i.e. TX complete.
		if (HAL_GPIO_ReadPin(GPIOA, TestChannelGdoPin(aaLoopChannel)) == GPIO_PIN_SET)
		{
			aaLoopState = AA_LOOP_STATE_DELAY;
			aaLoopDelayEndTick = HAL_GetTick() + AA_LOOP_INTER_PACKET_DELAY_MS;
		}
	}
	else // AA_LOOP_STATE_DELAY
	{
		if ((int32_t)(HAL_GetTick() - aaLoopDelayEndTick) >= 0)
		{
			aaLoopState = AA_LOOP_STATE_TX_ACTIVE;
			SendAAPacket();
		}
	}
}

static void ExitTestMode(void)
{
	// Flag the configuration as changed so the main loop's existing
	// HasChannelConfigurationChanged() path runs ReconfigureCC2500() and
	// restores normal RX + re-arms EXTI — the same path as any I2C config change.
	SetChannelConfigurationChanged();
}

// Test mode 3: periodically send a normal punch using the existing TX path.
// Build the punch payload once (normal station punch format) and arm the timer
// so the first punch is enqueued immediately on entering the mode.
static void StartNormalPeriodic(void)
{
	testMode3Punch.payloadLength = TEST_MODE3_PAYLOAD_LENGTH;

	// Normal I2C punch payload layout (see PunchQueue.h):
	testMode3Punch.payload[0] = 0xD3;         // record type
	testMode3Punch.payload[1] = 0x0D; 		  // payload length
	testMode3Punch.payload[2] = 0x00;         // CN1
	testMode3Punch.payload[3] = 0x20;         // CN0  control no: 32
	testMode3Punch.payload[4] = 0x00;         // SI# 25447
	testMode3Punch.payload[5] = 0x00;         // SI#
	testMode3Punch.payload[6] = 0x63;         // SI#
	testMode3Punch.payload[7] = 0x67;         // SI#
	testMode3Punch.payload[8] = 0x36;         // Weekday AM/PM
	testMode3Punch.payload[9] = 0x90;         // TH
	testMode3Punch.payload[10] = 0x0A;        // TL
	testMode3Punch.payload[11] = 0xCD;        // TSS
	testMode3Punch.payload[12] = 0x00;        // MEM2
	testMode3Punch.payload[13] = 0x01;        // MEM3
	testMode3Punch.payload[14] = 0xCD;        // MEM4
	
	testMode3Punch.retryCount = 0;
	testMode3Punch.lastSentChannel = 0;
	testMode3Punch.nextRetryTick = 0;

	// Send the first punch as soon as the main loop reaches ProcessTestModes().
	periodicNextSendTick = 0;

	ErrorLog_log("StartNormalPeriodic", "test mode 3 start");
}

static void MaintainNormalPeriodic(void)
{
	if ((int32_t)(HAL_GetTick() - periodicNextSendTick) < 0)
	{
		return;  // interval not yet elapsed
	}

	static uint16_t testMode3PunchSequenceNumber = 0;
	static bool firstPunchLogged = false;

	// Reset per-punch state and enqueue a fresh copy for the normal send path.
	testMode3Punch.retryCount = 0;
	testMode3Punch.lastSentChannel = 0;
	testMode3Punch.nextRetryTick = 0;


	testMode3Punch.payload[10] = testMode3PunchSequenceNumber % 256; // TL
	testMode3Punch.payload[11] = testMode3PunchSequenceNumber % 256; // TSS
	testMode3Punch.payload[14] = (testMode3PunchSequenceNumber*2) % 256; // MEM0
	if (TxPunchQueue_enQueue(&outgoingTxPunchQueue, &testMode3Punch) == QUEUEISFULL)
	{
		ErrorLog_log("MaintainNormalPeriodic", "TX queue full");
	}
	else if (!firstPunchLogged)
	{
		ErrorLog_log("MaintainNormalPeriodic", "first punch enqueued");
		firstPunchLogged = true;
	}
	testMode3PunchSequenceNumber++;
	periodicNextSendTick = HAL_GetTick() + ((uint32_t)GetTestMode3DelayTenths() * 100U);
}

void ProcessTestModes(void)
{
	static bool testModeActive = false;
	static uint8_t activeMode = 0;
	static uint8_t activeChannel = 0;

	if (IsTestModeEnabled())
	{
		uint8_t mode = GetTestMode();
		uint8_t channel = GetTestTxChannel();

		// For modes 1 & 2 the radios are bound to a single channel, so restart
		// on mode OR enabled-channel change (e.g. 0x41 red -> 0x42 blue). A
		// channel change also triggers the main loop's ReconfigureCC2500(), which
		// powers down the old channel and leaves the new one in normal RX;
		// restarting here then reconfigures the new channel for the test mode.
		//
		// Mode 3 (periodic normal punch) uses the normal send path and is
		// channel-agnostic, so it only (re)starts when the mode changes.
		bool restartNeeded;
		if (mode == TEST_MODE_TX_NORMAL_5S)
		{
			restartNeeded = (!testModeActive) || (activeMode != mode);
		}
		else
		{
			restartNeeded = (!testModeActive) || (activeMode != mode) || (activeChannel != channel);
		}

		if (restartNeeded)
		{
			// Coming out of a test-signal mode (1 or 2) into the periodic normal
			// punch mode (3): the radios are still configured for carrier/0xAA
			// (GDO0 = PA_PD, txInProgress set, EXTI disabled). Flag a config
			// change so the main loop's ReconfigureCC2500() restores normal RX
			// and clears txInProgress before the first periodic punch is sent.
			if (testModeActive
			    && (activeMode == TEST_MODE_TX_CARRIER || activeMode == TEST_MODE_TX_AA_LOOP)
			    && mode == TEST_MODE_TX_NORMAL_5S)
			{
				SetChannelConfigurationChanged();
			}

			activeMode = mode;
			activeChannel = channel;
			testModeActive = true;

			if (mode == TEST_MODE_TX_NORMAL_5S)
			{
				// No channel lock-in or radio reconfiguration required; the
				// normal ProcessOutgoingPunches() path sends the queued punches.
				StartNormalPeriodic();
				return;
			}

			if (channel == 0)
			{
				// No enabled channel to transmit on. Park the AA-loop state so
				// it doesn't keep driving a stale channel; it will restart on
				// the next channel change.
				aaLoopChannel = 0;
				return;
			}

			if (mode == TEST_MODE_TX_CARRIER)
			{
				StartCarrier(channel);
			}
			else if (mode == TEST_MODE_TX_AA_LOOP)
			{
				StartAALoop(channel);
			}
			else
			{
				char msg[40];
				sprintf(msg, "unsupported test mode %u", (unsigned)mode);
				ErrorLog_log("ProcessTestModes", msg);
			}
		}
		else
		{
			if (mode == TEST_MODE_TX_CARRIER)
			{
				MaintainCarrier();
			}
			else if (mode == TEST_MODE_TX_AA_LOOP)
			{
				MaintainAALoop();
			}
			else if (mode == TEST_MODE_TX_NORMAL_5S)
			{
				MaintainNormalPeriodic();
			}
		}
	}
	else
	{
		if (testModeActive)
		{
			ExitTestMode();
			testModeActive = false;
			activeMode = 0;
			activeChannel = 0;
			aaLoopChannel = 0;
			periodicNextSendTick = 0;
		}
	}
}
#endif /* TEST_MODES_ENABLED */


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
#ifdef TEST_MODES_ENABLED
		if (IsTestSignalModeActive())
		{
			// Test modes 1 & 2 own the radios — ignore RX sync-word interrupts.
			return;
		}
#endif
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
#ifdef TEST_MODES_ENABLED
		if (IsTestSignalModeActive())
		{
			// Test modes 1 & 2 own the radios — ignore TX-complete interrupts.
			return;
		}
#endif
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
