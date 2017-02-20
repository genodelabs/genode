/*
 * \brief  Implementation of linux/sched.h
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
#include <lx_kit/scheduler.h>


static void unblock_task(unsigned long task)
{
	Lx::Task *t = (Lx::Task *)task;

	t->unblock();
}


signed long schedule_timeout(signed long timeout)
{
	struct timer_list timer;

	unsigned long expire = timeout + jiffies;
	setup_timer(&timer, unblock_task, (unsigned long)Lx::scheduler().current());
	mod_timer(&timer, expire);

	Lx::scheduler().current()->block_and_schedule();

	del_timer(&timer);

	timeout = (expire - jiffies);

	return timeout < 0 ? 0 : timeout;
}
