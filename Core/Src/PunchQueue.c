/*
 * PunchQueue.c
 *
 *  Created on: May 27, 2023
 *      Author: henla464
 */

#include "PunchQueue.h"



struct Punch PunchQueue_items[PUNCHQUEUE_SIZE];
int8_t PunchQueue_front = -1, PunchQueue_rear = -1;


// Check if the queue is full
uint8_t PunchQueue_getNoOfItems()
{
	if (PunchQueue_rear < PunchQueue_front)
	{
		return PUNCHQUEUE_SIZE - (PunchQueue_front - PunchQueue_rear) + 1;
	}
	else
	{
		return PunchQueue_rear - PunchQueue_front + 1;
	}
}

// Check if the queue is full
bool PunchQueue_isFull()
{
	if ((PunchQueue_front == PunchQueue_rear + 1)
		  ||
		  (PunchQueue_front == 0 && PunchQueue_rear == PUNCHQUEUE_SIZE - 1))
	{
		return true;
	}
	return false;
}

// Check if the queue is empty
bool PunchQueue_isEmpty()
{
	if (PunchQueue_front == -1)
	{
		return true;
	}
	return false;
}


bool PunchQueue_enQueue(struct Punch * punch)
{
	if (PunchQueue_isFull())
	{
		return false;
	}
	else
	{
		if (PunchQueue_front == -1)
		{
			PunchQueue_front = 0;
		}
		PunchQueue_rear = (PunchQueue_rear + 1) % PUNCHQUEUE_SIZE;
		PunchQueue_items[PunchQueue_rear] = *punch;
		return true;
	}
}

bool PunchQueue_deQueue(struct Punch * punch)
{
	if (PunchQueue_isEmpty())
	{
		return false;
	}
	else
	{
		//for (uint8_t i = 0; i < PUNCH_LENGTH; i++)
		//{
		//	punch[i] = PunchQueue_items[PunchQueue_front][i];
		//}
		*punch = PunchQueue_items[PunchQueue_front];
		if (PunchQueue_front == PunchQueue_rear) {
			PunchQueue_front = -1;
			PunchQueue_rear = -1;
		}
		else
		{
			PunchQueue_front = (PunchQueue_front + 1) % PUNCHQUEUE_SIZE;
		}
		return true;
	}
}

bool PunchQueue_peek(struct Punch * punch)
{
	if (PunchQueue_isEmpty())
	{
		return false;
	}
	else
	{
		*punch = PunchQueue_items[PunchQueue_front];
		return true;
	}
}

bool PunchQueue_pop()
{
	if (PunchQueue_isEmpty())
	{
		return false;
	}
	else
	{
		if (PunchQueue_front == PunchQueue_rear) {
			PunchQueue_front = -1;
			PunchQueue_rear = -1;
		}
		else
		{
			PunchQueue_front = (PunchQueue_front + 1) % PUNCHQUEUE_SIZE;
		}
		return true;
	}
}

