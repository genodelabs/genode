/*
 * \brief  Timer service
 * \author Norman Feske
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <base/heap.h>
#include <cap_session/connection.h>
#include <os/server.h>

/* local includes */
#include "timer_root.h"

using namespace Genode;
using namespace Timer;


struct Main
{
	Server::Entrypoint    &ep;
	Sliced_heap            sliced_heap;
	Timer::Root_component  root;

	Main(Server::Entrypoint &ep)
	:
		ep(ep),
		sliced_heap(Genode::env()->ram_session(), Genode::env()->rm_session()),
		root(ep, &sliced_heap, 0)
	{
		/*
		 * Announce timer service at our parent.
		 */
		env()->parent()->announce(ep.manage(root));
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "timer_drv_ep";    }
	size_t stack_size()            { return 2048*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);   }
}
