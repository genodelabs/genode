/*
 * \brief  Implementation of linux/timer.h
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
#include <legacy/lx_kit/timer.h>


int mod_timer(struct timer_list *timer, unsigned long expires)
{
	if (!Lx::timer().find(timer))
		Lx::timer().add(timer, Lx::Timer::LIST);

	return Lx::timer().schedule(timer, expires);
}


void timer_setup(struct timer_list *timer,
                 void (*function)(struct timer_list *),
                 unsigned int flags)
{
	timer->function = function;
	timer->flags    = flags;

	/*
	 * For compatibility with 4.4.3 drivers, the 'data' member is passed as
	 * argument to the callback function. Since the 4.16.3 callback function
	 * interface has a 'timer_list' pointer as argument, the 'data' member
	 * points to the 'timer_list' object  itself when set up with this
	 * function.
	 */
	timer->data     = (unsigned long)timer;
}


int timer_pending(const struct timer_list *timer)
{
	bool pending = Lx::timer().pending(timer);

	return pending;
}


int del_timer(struct timer_list *timer)
{
	int rv = Lx::timer().del(timer);
	Lx::timer().schedule_next();

	return rv;
}


void hrtimer_init(struct hrtimer *timer, clockid_t clock_id, enum hrtimer_mode mode) { }


int hrtimer_start_range_ns(struct hrtimer *timer, ktime_t tim,
                           unsigned long delta_ns, const enum hrtimer_mode mode)
{
	unsigned long expires = tim / ((1000/HZ) * NSEC_PER_MSEC);

	/*
	 * Prevent truncation through rounding the values by adding 1 jiffy
	 * in this case.
	 */
	expires += (expires == jiffies);

	if (!Lx::timer().find(timer))
		Lx::timer().add(timer, Lx::Timer::HR);

	return Lx::timer().schedule(timer, expires);
}


bool hrtimer_active(const struct hrtimer *timer)
{
	return Lx::timer().find(timer);
}


int hrtimer_cancel(struct hrtimer *timer)
{
	int rv = Lx::timer().del(timer);
	Lx::timer().schedule_next();

	return rv;
}
