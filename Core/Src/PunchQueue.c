/*
 * PunchQueue.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */

#include "PunchQueue.h"


struct PunchQueue incomingPunchQueue = { .PunchQueue_front = -1, .PunchQueue_rear = -1 };


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
bool PunchQueue_isFull(struct PunchQueue * queue)
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
bool PunchQueue_isEmpty(struct PunchQueue * queue)
{
	if (queue->PunchQueue_front == -1)
	{
		return true;
	}
	return false;
}


uint8_t PunchQueue_enQueue(struct PunchQueue * queue, struct Punch * punch)
{
	if (PunchQueue_isFull(queue))
	{
		return QUEUEISFULL;
	}
	else
	{
		if (queue->PunchQueue_rear >= 0 &&
				punch->payload[4] == queue->PunchQueue_items[queue->PunchQueue_rear].payload[4] &&
				punch->payload[5]  == queue->PunchQueue_items[queue->PunchQueue_rear].payload[5] &&
				punch->payload[6] == queue->PunchQueue_items[queue->PunchQueue_rear].payload[6] &&
				punch->payload[7] == queue->PunchQueue_items[queue->PunchQueue_rear].payload[7] &&
				punch->payload[11] == queue->PunchQueue_items[queue->PunchQueue_rear].payload[11] )
		{
			// Same punch as previously received
			return SAMEPUNCH;
		}
		if (queue->PunchQueue_front == -1)
		{
			queue->PunchQueue_front = 0;
		}
		queue->PunchQueue_rear = (queue->PunchQueue_rear + 1) % PUNCHQUEUE_SIZE;
		queue->PunchQueue_items[queue->PunchQueue_rear] = *punch;
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
		return ENQUEUESUCCESS;
	}
}

bool PunchQueue_deQueue(struct PunchQueue * queue, struct Punch * punch)
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
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
		}
		else
		{
			queue->PunchQueue_front = (queue->PunchQueue_front + 1) % PUNCHQUEUE_SIZE;
		}
		return true;
	}
}

bool PunchQueue_peek(struct PunchQueue * queue, struct Punch * punch, struct Punch ** punchID)
{
	if (PunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		*punch = queue->PunchQueue_items[queue->PunchQueue_front];
		*punchID = &queue->PunchQueue_items[queue->PunchQueue_front];
		return true;
	}
}

bool PunchQueue_pop(struct PunchQueue * queue)
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
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
		}
		else
		{
			queue->PunchQueue_front = (queue->PunchQueue_front + 1) % PUNCHQUEUE_SIZE;
		}
		return true;
	}
}

bool PunchQueue_popSafe(struct PunchQueue * queue, struct Punch * punchID)
{
	if (PunchQueue_isEmpty(queue))
	{
		return false;
	}
	else
	{
		// only pop if it is same punch
		if (&queue->PunchQueue_items[queue->PunchQueue_front] == punchID)
		{
			if (queue->PunchQueue_front == queue->PunchQueue_rear) {
				queue->PunchQueue_front = -1;
				queue->PunchQueue_rear = -1;
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
			}
			else
			{
				queue->PunchQueue_front = (queue->PunchQueue_front + 1) % PUNCHQUEUE_SIZE;
			}
			return true;
		}
		return false;
	}
}

