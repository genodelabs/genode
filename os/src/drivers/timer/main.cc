/*
 * \brief  Timer service
 * \author Norman Feske
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/sleep.h>
#include <base/heap.h>
#include <cap_session/connection.h>

#include "timer_root.h"

using namespace Genode;
using namespace Timer;


/**
 * Main program
 */
int main(int argc, char **argv)
{
	/*
	 * Initialize server entry point that serves the root interface.
	 */
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "timer_ep");

	/*
	 * Use sliced heap to allocate each session component at a separate
	 * dataspace.
	 */
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	/*
	 * Create root interface for timer service
	 */
	static Timer::Root_component timer_root(&ep, &sliced_heap, &cap);

	/*
	 * Announce timer service at our parent.
	 */
	env()->parent()->announce(ep.manage(&timer_root));

	sleep_forever();
	return 0;
}
