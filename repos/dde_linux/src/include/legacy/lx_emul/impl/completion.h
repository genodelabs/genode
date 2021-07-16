/*
 * \brief  Implementation of linux/completion.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kint includes */
#include <legacy/lx_kit/env.h>
#include <legacy/lx_kit/internal/task.h>

typedef Lx::Task::List_element Wait_le;
typedef Lx::Task::List         Wait_list;


void init_waitqueue_head(wait_queue_head_t *wq)
{
	static_assert(sizeof(wq->wait_list_reserved) >= sizeof(Wait_list));
	Genode::construct_at<Wait_list>(wq->wait_list_reserved);
	wq->list = &wq->wait_list_reserved;
}

void add_wait_queue(wait_queue_head_t *q, wait_queue_entry_t *wait)
{
	printk("%s called\n", __func__);
}

void remove_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *wait)
{
	printk("%s called\n", __func__);
}


int waitqueue_active(wait_queue_head_t *wq)
{
	Wait_list *list = static_cast<Wait_list*>(wq->list);
	if (!list) { return 0; }

	return list->first() ? 1 : 0;
}


void __wake_up(wait_queue_head_t *wq, bool all)
{
	Wait_list *list = static_cast<Wait_list *>(wq->list);
	if (!list) { return; }

	Wait_le *le = list->first();
	do {
		if (!le) { return; }

		le->object()->unblock();
	} while (all && (le = le->next()));
}


void wake_up_interruptible_sync_poll(wait_queue_head_t *wq, int)
{
	__wake_up(wq, false);
}


void ___wait_event(wait_queue_head_t *wq)
{
	Wait_list *list = static_cast<Wait_list *>(wq->list);
	if (!list) {
		init_waitqueue_head(wq);
		list = static_cast<Wait_list *>(wq->list);
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
	if (work->done != UINT_MAX)
		work->done++;

	Lx::Task *task = static_cast<Lx::Task*>(work->task);
	if (task) { task->unblock(); }
}

void complete_all(struct completion *work)
{
	work->done = UINT_MAX;

	Lx::Task *task = static_cast<Lx::Task*>(work->task);
	if (task) { task->unblock(); }
}

bool try_wait_for_completion(struct completion *work)
{
	int ret = 1;

	if (!work->done)
		ret = 0;
	else if (work->done != UINT_MAX)
		work->done--;

	return ret;
}

unsigned long wait_for_completion_timeout(struct completion *work,
                                          unsigned long timeout)
{
	return __wait_completion(work, timeout);
}


int wait_for_completion_interruptible(struct completion *work)
{
	__wait_completion(work, 0);
	return 0;
}


long wait_for_completion_interruptible_timeout(struct completion *work,
                                               unsigned long timeout)
{
	return __wait_completion(work, timeout);
}


void wait_for_completion(struct completion *work)
{
	__wait_completion(work, 0);
}
