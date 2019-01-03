/*
 * \brief  Timer accuracy test
 * \author Norman Feske
 * \author Martin Stein
 * \date   2010-03-09
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/component.h>

using namespace Genode;


struct Main
{
	Timer::Connection    timer;
	Signal_handler<Main> timer_handler;
	unsigned             duration_us { 0 };

	void handle_timer()
	{
		duration_us += 1000 * 1000;
		timer.trigger_once(duration_us);
		log("");
	}

	Main(Env &env) : timer(env), timer_handler(env.ep(), *this, &Main::handle_timer)
	{
		timer.sigh(timer_handler);
		handle_timer();
	}
};


void Component::construct(Env &env) { static Main main(env); }
