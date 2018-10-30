/*
 * \brief  Testing CPU performance
 * \author Stefan Kalkowski
 * \date   2018-10-22
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <timer_session/connection.h>

#include <bogomips.h>

void Component::construct(Genode::Env &env)
{
	Timer::Connection timer(env);
	timer.msleep(2000);

	Genode::log("Cpu testsuite started");
	Genode::log("start bogomips");
	bogomips();
	Genode::log("finished bogomips");
};
