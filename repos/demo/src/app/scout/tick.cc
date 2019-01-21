/*
 * \brief   Timed event scheduler
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <scout/tick.h>

using namespace Scout;


static Tick       *head = 0;   /* head of tick list                        */
static Tick::time  now  = 0;   /* recent time (updated by handle function) */

void Tick::_enqueue()
{
	/* do not enqueue twice */
	if (++_active > 1) {
		_active--;
		return;
	}

	/* if ticklist is empty add first element */
	if (!head) {
		_next = 0;
		head = this;
		return;
	}

	/* if deadline is smaller than any other deadline, put it on the head */
	if ((int)_deadline - (int)now < (int)head->_deadline - (int)now) {
		_next = head;
		head = this;
		return;
	}

	/* find list element with a higher deadline */
	Tick *curr = head;
	while (curr->_next && ((int)curr->_next->_deadline - (int)now < (int)_deadline - (int)now))
		curr = curr->_next;

	/* if end of list is reached, append new element */
	if (curr->_next == 0) {
		curr->_next = this;
		return;
	}

	/* insert element in middle of list */
	_next = curr->_next;
	curr->_next = this;
}


void Tick::_dequeue()
{
	if (!head) return;

	if (head == this) {
		head = _next;
		return;
	}

	/* find predecessor in tick queue */
	Tick *curr;
	for (curr = head; curr && (curr->_next != this); curr = curr->_next);

	/* tick is not enqueued */
	if (!curr) return;

	/* skip us in tick queue */
	curr->_next = _next;

	_next = 0;
}


void Tick::schedule(time period)
{
	_period    = period;
	_deadline  = now;       /* first deadline is overdue */
	_enqueue();
}


int Tick::ticks_scheduled()
{
	int num_ticks = 0;
	for (Tick *curr = head; curr; curr = curr->_next, num_ticks++);
	return num_ticks;
}


void Tick::handle(time curr_time)
{
	Tick *curr;
	now = curr_time;

	while ((curr = head) && ((int)head->_deadline - (int)now < 0)) {

		/* remove tick from head of the list */
		head = curr->_next;

		curr->_next = 0;
		curr->_active--;

		/* do not reschedule if tick function returns 0 */
		if (!curr->on_tick()) continue;

		/* schedule next event */
		if (curr->_deadline == 0)
			curr->_deadline = now;

		curr->_deadline += curr->_period;
		curr->_enqueue();
	}
}
