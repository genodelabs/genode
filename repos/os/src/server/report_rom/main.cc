/*
 * \brief  Server that aggregates reports and exposes them as ROM modules
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/env.h>
#include <report_rom/rom_service.h>
#include <report_rom/report_service.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>

/* local includes */
#include "rom_registry.h"


namespace Report_rom { struct Main; }


struct Report_rom::Main
{
	Genode::Env &env;

	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Rom::Registry rom_registry { sliced_heap, config_rom };

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	bool verbose = config_rom.xml().attribute_value("verbose", false);

	Report::Root report_root { env, sliced_heap, rom_registry, verbose };
	Rom   ::Root    rom_root { env, sliced_heap, rom_registry };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(report_root));
		env.parent().announce(env.ep().manage(rom_root));
	}
};


/***************
 ** Component **
 ***************/

Genode::size_t Component::stack_size() { return 4*1024*sizeof(long); }

void Component::construct(Genode::Env &env) { static Report_rom::Main main(env); }

