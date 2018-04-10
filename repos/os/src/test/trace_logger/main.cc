/*
 * \brief  Test functionality of the trace logger
 * \author Martin Stein
 * \date   2017-01-23
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>
#include <trace/timestamp.h>

using namespace Genode;


void Component::construct(Genode::Env &env)
{
	Timer::Connection timer(env);
	for (unsigned i = 0; ; i++) {
		timer.msleep(100);
		Thread::trace(String<32>(i, " ", Trace::timestamp()).string());
	}
	env.parent().exit(0);
}
