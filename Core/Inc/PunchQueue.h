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
};

extern struct Punch PunchQueue_items[PUNCHQUEUE_SIZE];


uint8_t PunchQueue_getNoOfItems();
bool PunchQueue_isFull();
bool PunchQueue_isEmpty();
bool PunchQueue_enQueue(struct Punch * punch);
bool PunchQueue_deQueue(struct Punch * punch);
bool PunchQueue_peek(struct Punch * punch);
bool PunchQueue_pop();

#endif /* INC_PUNCHQUEUE_H_ */
