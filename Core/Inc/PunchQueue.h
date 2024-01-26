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

#define PUNCHQUEUE_SIZE 6
#define PUNCH_LENGTH_STATION 30
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
  uint8_t payload[PUNCH_LENGTH_STATION];
  struct MessageStatus messageStatus;
  uint8_t channel;
};

struct PunchQueue {
	struct Punch PunchQueue_items[PUNCHQUEUE_SIZE];
	int8_t PunchQueue_front;
	int8_t PunchQueue_rear;
};


volatile extern struct PunchQueue incomingPunchQueue;

uint8_t PunchQueue_getNoOfItems();
bool PunchQueue_isFull(volatile struct PunchQueue * queue);
bool PunchQueue_isEmpty(volatile struct PunchQueue * queue);
bool PunchQueue_isSamePunch(struct Punch * punch1, volatile struct Punch * punch2);
uint8_t PunchQueue_enQueue(volatile struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_deQueue(volatile struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_peek(volatile struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_pop(volatile struct PunchQueue * queue);
bool PunchQueue_popSafe(volatile struct PunchQueue * queue, struct Punch * punchID);

#endif /* INC_PUNCHQUEUE_H_ */
