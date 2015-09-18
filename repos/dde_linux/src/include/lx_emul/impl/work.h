/*
 * \brief  Implementation of linux/workqueue.h
 * \author Stefan Kalkowski
 * \date   2015-10-26
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <lx_emul/impl/internal/work.h>


int schedule_work(struct work_struct *work)
{
	Lx::Work::work_queue().schedule(work);
	Lx::Work::work_queue().unblock();
	return 1;
}


static void _schedule_delayed_work(unsigned long w)
{
	schedule_work((struct work_struct *)w);
}


bool queue_delayed_work(struct workqueue_struct *wq,
                        struct delayed_work *dwork, unsigned long delay)
{
	/* treat delayed work without delay like any other work */
	if (delay == 0) {
		schedule_work(&dwork->work);
	} else {
		setup_timer(&dwork->timer, _schedule_delayed_work, (unsigned long)&dwork->work);
		mod_timer(&dwork->timer, delay);
	}
	return true;
}


int schedule_delayed_work(struct delayed_work *dwork, unsigned long delay)
{
	return queue_delayed_work(system_wq, dwork, delay);
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
		PERR("WARN: delayed_work %p is executed directly in current '%s' routine",
		      dwork, Lx::scheduler().current()->name());

		dwork->work.func(&dwork->work);
	}

	return pending;
}

