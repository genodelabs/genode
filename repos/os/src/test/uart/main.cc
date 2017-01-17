/*
 * \brief  Test for UART driver
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/snprintf.h>
#include <base/component.h>
#include <timer_session/connection.h>
#include <uart_session/connection.h>

using namespace Genode;


struct Main
{
	Timer::Connection timer;
	Uart::Connection  uart;
	char              buf[100];

	Main(Env &env)
	{
		log("--- UART test started ---");

		for (unsigned i = 0; ; ++i) {
			int n = snprintf(buf, sizeof(buf), "UART test message %d\n", i);
			uart.write(buf, n);
			timer.msleep(2000);
		}
	}
};

void Component::construct(Env &env) { static Main main(env); }
