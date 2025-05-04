/*
 * PunchQueue.h
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */

#ifndef INC_SENDPUNCHQUEUE_H_
#define INC_SENDPUNCHQUEUE_H_
#include <stdio.h>
#include <stdbool.h>
#include "stm32g0xx_hal.h"
#include "IRQLineHandler.h"
#include "PunchQueue.h"

#define SENDPUNCHQUEUE_SIZE 10
//#define PUNCH_LENGTH_STATION 30
//#define MAX_PUNCH_LENGTH PUNCH_LENGTH_STATION

//#define QUEUEISFULL 1
//#define SAMEPUNCH 2
//#define ENQUEUESUCCESS 0

//#define PUNCHTYPE_INDEX_PAYLOAD 14
//#define PUNCHTYPE_STATION 0xb6
//#define PUNCHTYPE_AIR_PLUS_LAST_MESSAGE 0xb1
//#define PUNCHTYPE_AIR_PLUS_MULTIPLE_MESSAGES 0xb7

//struct MessageStatus {
//  uint8_t rssi;
//  uint8_t crc;
//};

struct SendPunch {
  uint8_t payloadLength;
  uint8_t payload[MAX_PUNCH_LENGTH];
//  struct MessageStatus messageStatus;
//  uint8_t channel;
};

struct SendPunchQueue {
	struct SendPunch SendPunchQueue_items[SENDPUNCHQUEUE_SIZE];
	int8_t SendPunchQueue_front;
	int8_t SendPunchQueue_rear;
};


volatile extern struct SendPunchQueue outgoingPunchQueue;

uint8_t SendPunchQueue_getNoOfItems();
bool SendPunchQueue_isFull(volatile struct SendPunchQueue * queue);
bool SendPunchQueue_isEmpty(volatile struct SendPunchQueue * queue);
bool SendPunchQueue_isSamePunch(struct SendPunch * punch1, volatile struct SendPunch * punch2);
uint8_t SendPunchQueue_enQueue(volatile struct SendPunchQueue * queue, struct SendPunch * punch);
bool SendPunchQueue_deQueue(volatile struct SendPunchQueue * queue, struct SendPunch * punch);
bool SendPunchQueue_peek(volatile struct SendPunchQueue * queue, struct SendPunch * punch);
bool SendPunchQueue_pop(volatile struct SendPunchQueue * queue);
bool SendPunchQueue_popSafe(volatile struct SendPunchQueue * queue, struct SendPunch * punchID);

#endif /* INC_SENDPUNCHQUEUE_H_ */
