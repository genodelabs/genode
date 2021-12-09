/*
 * \brief  Test for UART driver
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>
#include <base/component.h>
#include <timer_session/connection.h>
#include <uart_session/connection.h>

using namespace Genode;


struct Main
{
	Timer::Connection timer;
	Uart::Connection  uart;

	Main(Env &env) : timer(env), uart(env)
	{
		log("--- UART test started ---");

		for (unsigned i = 0; ; ++i) {
			String<200> const msg("UART test message ", i);
			uart.write(msg.string(), msg.length() - 1);
			timer.msleep(2000);
		}
	}
};

void Component::construct(Env &env) { static Main main(env); }
