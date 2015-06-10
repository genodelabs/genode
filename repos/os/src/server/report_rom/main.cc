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

/* local includes */
#include <rom_service.h>
#include <report_service.h>


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

	Rom::Registry rom_registry = { sliced_heap };

	Xml_node _rom_config_node()
	{
		try {
			return Genode::config()->xml_node().sub_node("rom"); }
		catch (Xml_node::Nonexistent_sub_node) {
			PWRN("missing <rom> configuration");
			return Xml_node("<rom>");
		}
	}

	Xml_node rom_config = _rom_config_node();

	bool _verbose_config()
	{
		char const *attr = "verbose";
		return Genode::config()->xml_node().has_attribute(attr)
		    && Genode::config()->xml_node().attribute(attr).has_value("yes");
	}

	bool verbose = _verbose_config();

	Report::Root report_root = { ep, sliced_heap, rom_registry, verbose };
	Rom   ::Root    rom_root = { ep, sliced_heap, rom_registry, rom_config};

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
