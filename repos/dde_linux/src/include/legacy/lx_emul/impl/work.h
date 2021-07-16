/*
 * \brief  Implementation of linux/workqueue.h
 * \author Stefan Kalkowski
 * \date   2015-10-26
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <legacy/lx_kit/scheduler.h>
#include <legacy/lx_kit/work.h>


int schedule_work(struct work_struct *work)
{
	queue_work(work->wq ? work->wq : system_wq, work);
	return 1;
}


bool queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	work->wq = wq;

	/* a invalid func pointer will lead to pagefault with ip=0 sp=0 */
	if (!work || !work->func) {
		Genode::error("invalid work, called from ",
		              __builtin_return_address(0));
		return false;
	}

	/* check for separate work queue task */
	if (wq && wq->task) {
		Lx::Work *lx_work = (Lx::Work *)wq->task;
		lx_work->schedule(work);
		lx_work->unblock();
	} else {
		Lx::Work::work_queue().schedule(work);
		Lx::Work::work_queue().unblock();
	}

	return true;
}


void delayed_work_timer_fn(struct timer_list *t)
{
	struct delayed_work *dwork = from_timer(dwork, t, timer);
	queue_work(dwork->wq, &dwork->work);
}


bool queue_delayed_work(struct workqueue_struct *wq,
                        struct delayed_work *dwork, unsigned long delay)
{
	dwork->wq = wq;

	/* treat delayed work without delay like any other work */
	if (delay == 0) {
		delayed_work_timer_fn(&dwork->timer);
	} else {
		timer_setup(&dwork->timer, delayed_work_timer_fn, 0);
		mod_timer(&dwork->timer, jiffies + delay);
	}
	return true;
}


int schedule_delayed_work(struct delayed_work *dwork, unsigned long delay)
{
	return queue_delayed_work(dwork->wq ? dwork->wq : system_wq, dwork, delay);
}


bool cancel_work_sync(struct work_struct *work)
{
	/* check for separate work queue task */
	if (work->wq && work->wq->task) {
		Lx::Work *lx_work = (Lx::Work *)work->wq->task;

		return lx_work->cancel_work(work, true);
	}
	return false;
}


bool cancel_delayed_work(struct delayed_work *dwork)
{
	int pending = timer_pending(&dwork->timer);
	del_timer(&dwork->timer);

	/* if the timer is still pending dwork was not executed */
	return pending;
}


bool cancel_delayed_work_sync(struct delayed_work *dwork)
{
	bool pending = cancel_delayed_work(dwork);

	if (pending) {
		dwork->work.func(&dwork->work);
	}

	return pending;
}
