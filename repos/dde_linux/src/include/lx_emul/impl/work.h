/*
 * \brief  Implementation of linux/workqueue.h
 * \author Stefan Kalkowski
 * \date   2015-10-26
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux kit includes */
#include <lx_kit/scheduler.h>
#include <lx_kit/work.h>


int schedule_work(struct work_struct *work)
{
	queue_work(work->wq ? work->wq : system_wq, work);
	return 1;
}


bool queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	work->wq = wq;

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


static void _schedule_delayed_work(unsigned long w)
{
	delayed_work     *work = (delayed_work *)w;
	workqueue_struct *wq   = work->wq;

	/* check for separate work queue task */
	if (wq && wq->task) {
		Lx::Work *lx_work = (Lx::Work *)wq->task;
		lx_work->schedule_delayed(work, 0);
		lx_work->unblock();
	} else {
		Lx::Work::work_queue().schedule_delayed(work, 0);
		Lx::Work::work_queue().unblock();
	}
}


bool queue_delayed_work(struct workqueue_struct *wq,
                        struct delayed_work *dwork, unsigned long delay)
{
	dwork->wq = wq;

	/* treat delayed work without delay like any other work */
	if (delay == 0) {
		_schedule_delayed_work((unsigned long)dwork);
	} else {
		setup_timer(&dwork->timer, _schedule_delayed_work, (unsigned long)dwork);
		mod_timer(&dwork->timer, delay);
	}
	return true;
}


int schedule_delayed_work(struct delayed_work *dwork, unsigned long delay)
{
	return queue_delayed_work(dwork->wq ? dwork->wq : system_wq, dwork, delay);
}


bool cancel_work_sync(struct work_struct *work)
{
	return Lx::Work::work_queue().cancel_work(work, true);
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
		Genode::error("WARN: delayed_work ", dwork, " is executed directly in "
		              "current '", Lx::scheduler().current()->name(), "' routine");

		dwork->work.func(&dwork->work);
	}

	return pending;
}

