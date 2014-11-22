/*
 * \brief  Completions and events
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>

/* local includes */
#include <lx.h>
#include <scheduler.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
#include <extern_c_end.h>

static bool const verbose = false;
#define PWRNV(...) do { if (verbose) PWRN(__VA_ARGS__); } while (0)

extern "C" {

/************************
 ** linux/completion.h **
 ************************/

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
		PWRNV("wait_queue_head_t is empty, wq: %p called from: %p", wq, __builtin_return_address(0));
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


static void __wait_completion(struct completion *work)
{
	// PERR("%s:%d: FIXME", __func__, __LINE__);
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


/******************
 ** linux/wait.h **
 ******************/

void prepare_to_wait(wait_queue_head_t *q, wait_queue_t *w, int state)
{
	if (!q) {
		PWRNV("prepare_to_wait: wait_queue_head_t is 0, ignore, called from: %p",
		     __builtin_return_address(0));
		return;
	}

	Wait_list *list = static_cast<Wait_list *>(q->list);
	Lx::Task *task = Lx::scheduler().current();

	task->wait_enqueue(list);
}


void prepare_to_wait_exclusive(wait_queue_head_t *q, wait_queue_t *w, int state)
{
	prepare_to_wait(q, w, state);
}


void finish_wait(wait_queue_head_t *q, wait_queue_t *w)
{
	if (!q) {
		PWRNV("finish_wait: wait_queue_head_t is 0, ignore, called from: %p",
		     __builtin_return_address(0));
		return;
	}

	Wait_list *list = static_cast<Wait_list *>(q->list);
	Lx::Task *task = Lx::scheduler().current();

	task->wait_dequeue(list);
}


/*******************
 ** linux/timer.h **
 *******************/

signed long schedule_timeout_uninterruptible(signed long timeout)
{
	// PERR("%s:%d: FIXME", __func__, __LINE__);
	return 0;
}


int wake_up_process(struct task_struct *tsk)
{
	// PERR("%s:%d: FIXME does task: %p needs to be woken up?", __func__, __LINE__, tsk);
	return 0;
}

} /* extern "C" */
