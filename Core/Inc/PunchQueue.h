/*
 * PunchQueue.h
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */

#ifndef INC_PUNCHQUEUE_H_
#define INC_PUNCHQUEUE_H_
#include <stdio.h>
#include <stdbool.h>
#include "stm32g0xx_hal.h"
#include "IRQLineHandler.h"

#define PUNCHQUEUE_SIZE 10
#define PUNCH_LENGTH_STATION 30
#define MAX_PUNCH_LENGTH PUNCH_LENGTH_STATION
#define PUNCH_LENGTH_AIR_PLUS_LAST_MESSAGE 25
#define PUNCH_LENGTH_AIR_PLUS_MULTIPLE_MESSAGES 27  // when SI card sends all or all unsent punches
#define QUEUEISFULL 1
#define SAMEPUNCH 2
#define ENQUEUESUCCESS 0

#define PUNCHTYPE_INDEX_PAYLOAD 14
#define PUNCHTYPE_STATION 0xb6
#define PUNCHTYPE_AIR_PLUS_LAST_MESSAGE 0xb1
#define PUNCHTYPE_AIR_PLUS_MULTIPLE_MESSAGES 0xb7

struct MessageStatus {
  uint8_t rssi;
  uint8_t crc;
};

struct Punch {
  uint8_t payloadLength;
  uint8_t payload[MAX_PUNCH_LENGTH];
  struct MessageStatus messageStatus;
  uint8_t channel;
};

struct PunchQueue {
	struct Punch PunchQueue_items[PUNCHQUEUE_SIZE];
	int8_t PunchQueue_front;
	int8_t PunchQueue_rear;
};


volatile extern struct PunchQueue incomingPunchQueue;

uint8_t PunchQueue_getNoOfItems(volatile struct PunchQueue * queue);
bool PunchQueue_isFull(volatile struct PunchQueue * queue);
bool PunchQueue_isEmpty(volatile struct PunchQueue * queue);
bool PunchQueue_isSamePunch(struct Punch * punch1, const struct Punch * punch2);
uint8_t PunchQueue_enQueue(volatile struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_deQueue(volatile struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_peek(volatile struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_pop(volatile struct PunchQueue * queue);

/*----------------------------------------------------------------------------*/
/*  TX Punch types — punches received over I2C to be transmitted via CC2500  */
/*----------------------------------------------------------------------------*/

// I2C payload: Record type(1) + Length(1) + CN1(1) + CN0(1) + SI#(4)
//            + Week/Day/AMPM(1) + TH(1) + TL(1) + TSS(1) + MEM2(1)
//            + MEM1(1) + MEM0(1) = up to 15 bytes
#define TXPUNCH_MAX_PAYLOAD_SIZE 15
// I2C transfer: 1 length byte + up to TXPUNCH_MAX_PAYLOAD_SIZE payload bytes
#define TXPUNCH_I2C_MAX_TRANSFER_SIZE (TXPUNCH_MAX_PAYLOAD_SIZE + 1)
#define TX_PUNCHQUEUE_SIZE 10
#define MAX_TX_RETRIES 6

struct TxPunch {
  uint8_t payloadLength;                       // actual payload length (from I2C length byte)
  uint8_t payload[TXPUNCH_MAX_PAYLOAD_SIZE];   // raw I2C punch payload
  uint8_t retryCount;
  uint8_t lastSentChannel;                     // REDCHANNEL/BLUECHANNEL last tried, 0=never sent
  uint32_t nextRetryTick;                      // HAL_GetTick() value when next retry is allowed
};

struct TxPunchQueue {
  struct TxPunch items[TX_PUNCHQUEUE_SIZE];
  int8_t front;
  int8_t rear;
};

volatile extern struct TxPunchQueue outgoingTxPunchQueue;
extern volatile uint8_t txLastAckedChannel;      // channel last ACKed on (REDCHANNEL/BLUECHANNEL)
extern volatile uint8_t txMessageSequenceNumber;  // incremented for every CC2500 transmit attempt
extern volatile uint8_t txPunchSequenceNumber;    // incremented for each NEW punch sent (not retries)
extern volatile uint16_t txMessagesAcked;        // punches confirmed by a received ACK (popped from queue)
extern volatile uint16_t txMessagesSent;         // CC2500 TX attempts (messages, incl. retries), test mode 3 or normal send

uint8_t TxPunchQueue_getNoOfItems(volatile struct TxPunchQueue * queue);
bool TxPunchQueue_isFull(volatile struct TxPunchQueue * queue);
bool TxPunchQueue_isEmpty(volatile struct TxPunchQueue * queue);
uint8_t TxPunchQueue_enQueue(volatile struct TxPunchQueue * queue, struct TxPunch * punch);
bool TxPunchQueue_deQueue(volatile struct TxPunchQueue * queue, struct TxPunch * punch);
bool TxPunchQueue_peek(volatile struct TxPunchQueue * queue, struct TxPunch * punch);
struct TxPunch * TxPunchQueue_peekPtr(volatile struct TxPunchQueue * queue);
bool TxPunchQueue_pop(volatile struct TxPunchQueue * queue);

#endif /* INC_PUNCHQUEUE_H_ */
