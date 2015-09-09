/*
 * \brief  Implementation of linux/timer.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <lx_emul/impl/internal/timer.h>


void init_timer(struct timer_list *timer) { }


int mod_timer(struct timer_list *timer, unsigned long expires)
{
	if (!Lx::timer().find(timer))
		Lx::timer().add(timer);

	return Lx::timer().schedule(timer, expires);
}


void setup_timer(struct timer_list *timer,void (*function)(unsigned long),
                 unsigned long data)
{
	timer->function = function;
	timer->data     = data;
	init_timer(timer);
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
