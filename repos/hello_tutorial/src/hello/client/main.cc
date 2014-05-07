/*
 * \brief  Test client for the Hello RPC interface
 * \author Björn Döbel
 * \date   2008-03-20
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <hello_session/client.h>
#include <hello_session/connection.h>

#include <timer_session/connection.h>

using namespace Genode;

int main(void)
{
	Hello::Connection h;

	Timer::Connection timer;

	while (1) {
		h.say_hello();

		int foo = h.add(2, 5);
		PDBG("Added 2 + 5 = %d", foo);
		timer.msleep(1000);
	}

	return 0;
}
