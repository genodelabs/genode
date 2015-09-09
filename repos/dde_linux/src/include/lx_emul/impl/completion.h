/*
 * \brief  Implementation of linux/completion.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

typedef Lx::Task::List_element Wait_le;
typedef Lx::Task::List         Wait_list;


void init_waitqueue_head(wait_queue_head_t *wq)
{
	wq->list = new (Genode::env()->heap()) Wait_list;
}


int waitqueue_active(wait_queue_head_t *wq)
{
	Wait_list *list = static_cast<Wait_list*>(wq->list);
	if (!list)
		return 0;

	return list->first() ? 1 : 0;
}


void __wake_up(wait_queue_head_t *wq, bool all)
{
	Wait_list *list = static_cast<Wait_list *>(wq->list);
	if (!list) {
		PWRN("wait_queue_head_t is empty, wq: %p called from: %p", wq, __builtin_return_address(0));
		return;
	}

	Wait_le *le = list->first();
	do {
		if (!le)
			return;

		le->object()->unblock();
	} while (all && (le = le->next()));
}


void wake_up_interruptible_sync_poll(wait_queue_head_t *wq, int)
{
	__wake_up(wq, false);
}


void __wait_event(wait_queue_head_t wq)
{
	Wait_list *list = static_cast<Wait_list *>(wq.list);
	if (!list) {
		PERR("__wait_event(): empty list in wq: %p", &wq);
		Genode::sleep_forever();
	}

	Lx::Task *task = Lx::scheduler().current();

	task->wait_enqueue(list);
	task->block_and_schedule();
	task->wait_dequeue(list);
}


void init_completion(struct completion *work)
{
	work->done = 0;
}


void complete(struct completion *work)
{
	work->done = 1;
}


unsigned long wait_for_completion_timeout(struct completion *work,
                                          unsigned long timeout)
{
	__wait_completion(work);
	return 1;
}


int wait_for_completion_interruptible(struct completion *work)
{
	__wait_completion(work);
	return 0;
}


long wait_for_completion_interruptible_timeout(struct completion *work,
                                               unsigned long timeout)
{
	__wait_completion(work);
	return 1;
}


void wait_for_completion(struct completion *work)
{
	__wait_completion(work);
}
