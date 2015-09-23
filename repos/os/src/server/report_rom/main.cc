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
	using Genode::Xml_node;
	struct Main;
}


struct Server::Main
{
	Entrypoint &ep;

	Genode::Sliced_heap sliced_heap = { env()->ram_session(),
	                                    env()->rm_session() };

	Xml_node _rom_config_node() const
	{
		try {
			return Genode::config()->xml_node().sub_node("rom"); }
		catch (Xml_node::Nonexistent_sub_node) {
			PWRN("missing <rom> configuration");
			return Xml_node("<rom>");
		}
	}

	Rom::Registry rom_registry = { sliced_heap, _rom_config_node() };

	bool _verbose_config()
	{
		char const *attr = "verbose";
		return Genode::config()->xml_node().has_attribute(attr)
		    && Genode::config()->xml_node().attribute(attr).has_value("yes");
	}

	bool verbose = _verbose_config();

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
