/*
 * PunchQueue.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */

#include "PunchQueue.h"
#include "main.h"


volatile struct PunchQueue incomingPunchQueue = { .PunchQueue_front = -1, .PunchQueue_rear = -1 };
volatile struct Punch lastPunch;

volatile struct TxPunchQueue outgoingTxPunchQueue = { .front = -1, .rear = -1 };
volatile uint8_t txLastAckedChannel = BLUECHANNEL;  // initial default

// Check if the queue is full
uint8_t PunchQueue_getNoOfItems(struct PunchQueue * queue)
{
	if (queue->PunchQueue_front == -1)
	{
		return 0;
	}
	if (queue->PunchQueue_rear < queue->PunchQueue_front)
	{
		return PUNCHQUEUE_SIZE - (queue->PunchQueue_front - queue->PunchQueue_rear) + 1;
	}
	else
	{
		return queue->PunchQueue_rear - queue->PunchQueue_front + 1;
	}
}

// Check if the queue is full
bool PunchQueue_isFull(volatile struct PunchQueue * queue)
{
	if ((queue->PunchQueue_front == queue->PunchQueue_rear + 1)
		  ||
		  (queue->PunchQueue_front == 0 && queue->PunchQueue_rear == PUNCHQUEUE_SIZE - 1))
	{
		return true;
	}
	return false;
}

// Check if the queue is empty
bool PunchQueue_isEmpty(volatile struct PunchQueue * queue)
{
	if (queue->PunchQueue_front == -1)
	{
		return true;
	}
	return false;
}

bool PunchQueue_isSamePunch(struct Punch * punch1, volatile struct Punch * punch2)
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

uint8_t PunchQueue_enQueue(volatile struct PunchQueue * queue, struct Punch * punch)
{
	if (PunchQueue_isFull(queue))
	{
		return QUEUEISFULL;
	}
	else
	{
		if (PunchQueue_isSamePunch(punch, &lastPunch))
		{
			// Same punch as last received
			return SAMEPUNCH;
		}
		if (queue->PunchQueue_front == -1)
		{
			queue->PunchQueue_front = 0;
		}
		queue->PunchQueue_rear = (queue->PunchQueue_rear + 1) % PUNCHQUEUE_SIZE;
		queue->PunchQueue_items[queue->PunchQueue_rear] = *punch;
		lastPunch = *punch;
		IRQLineHandler_SetPunchesExist();
		return ENQUEUESUCCESS;
	}
}

bool PunchQueue_deQueue(volatile struct PunchQueue * queue, struct Punch * punch)
{
	if (PunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		*punch = queue->PunchQueue_items[queue->PunchQueue_front];
		if (queue->PunchQueue_front == queue->PunchQueue_rear) {
			queue->PunchQueue_front = -1;
			queue->PunchQueue_rear = -1;
			IRQLineHandler_ClearPunchesExist();
		}
		else
		{
			queue->PunchQueue_front = (queue->PunchQueue_front + 1) % PUNCHQUEUE_SIZE;
		}
		return true;
	}
}

bool PunchQueue_peek(volatile struct PunchQueue * queue, struct Punch * punch)
{
	if (PunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		*punch = queue->PunchQueue_items[queue->PunchQueue_front];
		return true;
	}
}

bool PunchQueue_pop(volatile struct PunchQueue * queue)
{
	if (PunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		if (queue->PunchQueue_front == queue->PunchQueue_rear) {
			queue->PunchQueue_front = -1;
			queue->PunchQueue_rear = -1;
			IRQLineHandler_ClearPunchesExist();
		}
		else
		{
			queue->PunchQueue_front = (queue->PunchQueue_front + 1) % PUNCHQUEUE_SIZE;
		}
		return true;
	}
}




/*============================================================================*/
/*  TxPunchQueue functions — for outgoing punches (I2C → CC2500)             */
/*============================================================================*/

uint8_t TxPunchQueue_getNoOfItems(volatile struct TxPunchQueue * queue)
{
	if (queue->front == -1)
	{
		return 0;
	}
	if (queue->rear < queue->front)
	{
		return TX_PUNCHQUEUE_SIZE - (queue->front - queue->rear) + 1;
	}
	else
	{
		return queue->rear - queue->front + 1;
	}
}

bool TxPunchQueue_isFull(volatile struct TxPunchQueue * queue)
{
	if ((queue->front == queue->rear + 1)
		  ||
		  (queue->front == 0 && queue->rear == TX_PUNCHQUEUE_SIZE - 1))
	{
		return true;
	}
	return false;
}

bool TxPunchQueue_isEmpty(volatile struct TxPunchQueue * queue)
{
	if (queue->front == -1)
	{
		return true;
	}
	return false;
}

uint8_t TxPunchQueue_enQueue(volatile struct TxPunchQueue * queue, struct TxPunch * punch)
{
	if (TxPunchQueue_isFull(queue))
	{
		return QUEUEISFULL;
	}
	if (queue->front == -1)
	{
		queue->front = 0;
	}
	queue->rear = (queue->rear + 1) % TX_PUNCHQUEUE_SIZE;
	queue->items[queue->rear] = *punch;
	return ENQUEUESUCCESS;
}

bool TxPunchQueue_deQueue(volatile struct TxPunchQueue * queue, struct TxPunch * punch)
{
	if (TxPunchQueue_isEmpty(queue))
	{
		return false;
	}
	*punch = queue->items[queue->front];
	if (queue->front == queue->rear)
	{
		queue->front = -1;
		queue->rear = -1;
	}
	else
	{
		queue->front = (queue->front + 1) % TX_PUNCHQUEUE_SIZE;
	}
	return true;
}

bool TxPunchQueue_peek(volatile struct TxPunchQueue * queue, struct TxPunch * punch)
{
	if (TxPunchQueue_isEmpty(queue))
	{
		return false;
	}
	*punch = queue->items[queue->front];
	return true;
}

bool TxPunchQueue_pop(volatile struct TxPunchQueue * queue)
{
	if (TxPunchQueue_isEmpty(queue))
	{
		return false;
	}
	if (queue->front == queue->rear)
	{
		queue->front = -1;
		queue->rear = -1;
	}
	else
	{
		queue->front = (queue->front + 1) % TX_PUNCHQUEUE_SIZE;
	}
	return true;
}
