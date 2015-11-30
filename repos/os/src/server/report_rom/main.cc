/*
 * \brief  Server that aggregates reports and exposes them as ROM modules
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/env.h>
#include <os/server.h>
#include <os/config.h>
#include <report_rom/rom_service.h>
#include <report_rom/report_service.h>

/* local includes */
#include "rom_registry.h"


namespace Server {
	using Genode::env;
	struct Main;
}


struct Server::Main
{
	Entrypoint &ep;

	Genode::Sliced_heap sliced_heap = { env()->ram_session(),
	                                    env()->rm_session() };

	Rom::Registry rom_registry = { sliced_heap };

	bool verbose = Genode::config()->xml_node().attribute_value("verbose", false);

	Report::Root report_root = { ep, sliced_heap, rom_registry, verbose };
	Rom   ::Root    rom_root = { ep, sliced_heap, rom_registry };

	Main(Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(report_root));
		env()->parent()->announce(ep.manage(rom_root));
	}
};


namespace Server {

	char const *name() { return "report_rom_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Main main(ep);
	}
}
