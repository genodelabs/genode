/*
 * \brief  Implementation of linux/wait.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <legacy/lx_kit/scheduler.h>


void prepare_to_wait(wait_queue_head_t *q, wait_queue_entry_t *, int)
{
	if (!q) { return; }

	Wait_list *list = static_cast<Wait_list *>(q->list);
	Lx::Task *task = Lx::scheduler().current();

	task->wait_enqueue(list);
}


void prepare_to_wait_exclusive(wait_queue_head_t *q, wait_queue_entry_t *, int)
{
	prepare_to_wait(q, 0, 0);
}


void finish_wait(wait_queue_head_t *q, wait_queue_entry_t *)
{
	if (!q) { return; }

	Wait_list *list = static_cast<Wait_list *>(q->list);
	Lx::Task *task = Lx::scheduler().current();

	task->wait_dequeue(list);
}
