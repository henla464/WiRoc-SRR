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

#define PUNCHQUEUE_SIZE 6
#define PUNCH_LENGTH 30

extern int8_t PunchQueue_front;
extern int8_t PunchQueue_rear;


struct MessageStatus {
  uint8_t rssi;
  uint8_t crc;
};

struct Punch {
  uint8_t payloadLength;
  uint8_t payload[PUNCH_LENGTH];
  struct MessageStatus messageStatus;
  uint8_t channel;
};

struct PunchQueue {
	struct Punch PunchQueue_items[PUNCHQUEUE_SIZE];
	int8_t PunchQueue_front;
	int8_t PunchQueue_rear;
};


extern struct PunchQueue incomingPunchQueue;
extern struct PunchQueue outgoingPunchQueue;

uint8_t PunchQueue_getNoOfItems();
bool PunchQueue_isFull(struct PunchQueue * queue);
bool PunchQueue_isEmpty(struct PunchQueue * queue);
bool PunchQueue_enQueue(struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_deQueue(struct PunchQueue * queue, struct Punch * punch);
bool PunchQueue_peek(struct PunchQueue * queue, struct Punch * punch, struct Punch ** punchID);
bool PunchQueue_pop(struct PunchQueue * queue);
bool PunchQueue_popSafe(struct PunchQueue * queue, struct Punch * punchID);

#endif /* INC_PUNCHQUEUE_H_ */
