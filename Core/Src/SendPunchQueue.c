/*
 * SendPunchQueue.c
 *
 *  Created on: May 4, 2025
 *      Author: henla464
 */

#include "SendPunchQueue.h"


volatile struct SendPunchQueue outgoingPunchQueue = { .SendPunchQueue_front = -1, .SendPunchQueue_rear = -1 };
volatile struct SendPunch lastSendPunch;

// Check if the queue is full
uint8_t SendPunchQueue_getNoOfItems(struct SendPunchQueue * queue)
{
	if (queue->SendPunchQueue_front == -1)
	{
		return 0;
	}
	if (queue->SendPunchQueue_rear < queue->SendPunchQueue_front)
	{
		return SENDPUNCHQUEUE_SIZE - (queue->SendPunchQueue_front - queue->SendPunchQueue_rear) + 1;
	}
	else
	{
		return queue->SendPunchQueue_rear - queue->SendPunchQueue_front + 1;
	}
}

// Check if the queue is full
bool SendPunchQueue_isFull(volatile struct SendPunchQueue * queue)
{
	if ((queue->SendPunchQueue_front == queue->SendPunchQueue_rear + 1)
		  ||
		  (queue->SendPunchQueue_front == 0 && queue->SendPunchQueue_rear == SENDPUNCHQUEUE_SIZE - 1))
	{
		return true;
	}
	return false;
}

// Check if the queue is empty
bool SendPunchQueue_isEmpty(volatile struct SendPunchQueue * queue)
{
	if (queue->SendPunchQueue_front == -1)
	{
		return true;
	}
	return false;
}

bool SendPunchQueue_isSamePunch(struct SendPunch * punch1, volatile struct SendPunch * punch2)
{
	uint8_t punchType1 = punch1->payload[PUNCHTYPE_INDEX_PAYLOAD];
	uint32_t senderId1 = 0;
	if (punchType1 == PUNCHTYPE_STATION) {
		senderId1 = (punch1->payload[4] << 24) + (punch1->payload[5] << 16) + (punch1->payload[6] << 8) + punch1->payload[7];
	} else if (punchType1 == PUNCHTYPE_AIR_PLUS_LAST_MESSAGE || punchType1 == PUNCHTYPE_AIR_PLUS_MULTIPLE_MESSAGES) {
		senderId1 = (punch1->payload[15] << 24) + (punch1->payload[16] << 16) + (punch1->payload[17] << 8) + punch1->payload[18];
	}
	uint32_t punchSequenceNo1 = punch1->payload[11];

	uint8_t punchType2 = punch2->payload[PUNCHTYPE_INDEX_PAYLOAD];
	uint32_t senderId2 = 0;
	if (punchType2 == PUNCHTYPE_STATION) {
		senderId2 = (punch2->payload[4] << 24) + (punch2->payload[5] << 16) + (punch2->payload[6] << 8) + punch2->payload[7];
	} else if (punchType2 == PUNCHTYPE_AIR_PLUS_LAST_MESSAGE || punchType2 == PUNCHTYPE_AIR_PLUS_MULTIPLE_MESSAGES) {
		senderId2 = (punch2->payload[15] << 24) + (punch2->payload[16] << 16) + (punch2->payload[17] << 8) + punch2->payload[18];
	}
	uint32_t punchSequenceNo2 = punch2->payload[11];

	return senderId1 == senderId2 && punchSequenceNo1 == punchSequenceNo2;
}

uint8_t SendPunchQueue_enQueue(volatile struct SendPunchQueue * queue, struct SendPunch * punch)
{
	if (SendPunchQueue_isFull(queue))
	{
		return QUEUEISFULL;
	}
	else
	{
		if (SendPunchQueue_isSamePunch(punch, &lastSendPunch))
		{
			// Same punch as last received
			return SAMEPUNCH;
		}
		if (queue->SendPunchQueue_front == -1)
		{
			queue->SendPunchQueue_front = 0;
		}
		queue->SendPunchQueue_rear = (queue->SendPunchQueue_rear + 1) % SENDPUNCHQUEUE_SIZE;
		queue->SendPunchQueue_items[queue->SendPunchQueue_rear] = *punch;
		lastSendPunch = *punch;
		IRQLineHandler_SetPunchesExist();
		return ENQUEUESUCCESS;
	}
}

bool SendPunchQueue_deQueue(volatile struct SendPunchQueue * queue, struct SendPunch * punch)
{
	if (SendPunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		*punch = queue->SendPunchQueue_items[queue->SendPunchQueue_front];
		if (queue->SendPunchQueue_front == queue->SendPunchQueue_rear) {
			queue->SendPunchQueue_front = -1;
			queue->SendPunchQueue_rear = -1;
			IRQLineHandler_ClearPunchesExist();
		}
		else
		{
			queue->SendPunchQueue_front = (queue->SendPunchQueue_front + 1) % SENDPUNCHQUEUE_SIZE;
		}
		return true;
	}
}

bool SendPunchQueue_peek(volatile struct SendPunchQueue * queue, struct SendPunch * punch)
{
	if (SendPunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		*punch = queue->SendPunchQueue_items[queue->SendPunchQueue_front];
		return true;
	}
}

bool SendPunchQueue_pop(volatile struct SendPunchQueue * queue)
{
	if (SendPunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		if (queue->SendPunchQueue_front == queue->SendPunchQueue_rear) {
			queue->SendPunchQueue_front = -1;
			queue->SendPunchQueue_rear = -1;
			//IRQLineHandler_ClearPunchesExist();
		}
		else
		{
			queue->SendPunchQueue_front = (queue->SendPunchQueue_front + 1) % SENDPUNCHQUEUE_SIZE;
		}
		return true;
	}
}

