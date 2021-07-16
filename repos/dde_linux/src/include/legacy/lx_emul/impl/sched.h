/*
 * \brief  Implementation of linux/sched.h
 * \author Norman Feske
 * \author Stefan Kalkowski
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

struct process_timer {
	struct timer_list  timer;
	Lx::Task          &task;

	process_timer(Lx::Task &task) : task(task) {}
};


static void process_timeout(struct timer_list *list)
{
	struct process_timer *timeout = from_timer(timeout, list, timer);
	timeout->task.unblock();
}


signed long schedule_timeout(signed long timeout)
{
	Lx::Task & cur_task = *Lx::scheduler().current();
	struct process_timer timer { cur_task };
	timer_setup(&timer.timer, process_timeout, 0);

	unsigned long expire = timeout + jiffies;
	mod_timer(&timer.timer, expire);

	cur_task.block_and_schedule();

	del_timer(&timer.timer);

	timeout = (expire - jiffies);

	return timeout < 0 ? 0 : timeout;
}
