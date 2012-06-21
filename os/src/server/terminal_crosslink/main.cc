/*
 * \brief  A server for connecting two 'Terminal' sessions
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <cap_session/connection.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <base/sleep.h>

/* local includes */
#include "terminal_root.h"

int main(int argc, char **argv)
{
	using namespace Genode;

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, Terminal::STACK_SIZE, "terminal_ep");

	static Terminal::Root terminal_root(&ep, env()->heap(), cap);
	env()->parent()->announce(ep.manage(&terminal_root));

	sleep_forever();
	return 0;
}
