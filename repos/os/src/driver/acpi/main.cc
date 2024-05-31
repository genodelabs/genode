/*
 * \brief  Service and session interface
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 */

 /*
  * Copyright (C) 2009-2017 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <util/xml_generator.h>

/* local includes */
#include <acpi.h>
#include <smbios_table_reporter.h>


namespace Acpi {
	using namespace Genode;

	struct Main;
}

struct Acpi::Main
{
	Genode::Env           &_env;
	Genode::Heap           _heap { _env.ram(), _env.rm() };
	Attached_rom_dataspace _config { _env, "config" };

	struct Acpi_reporter
	{
		Acpi_reporter(Env &env, Heap &heap, Xml_node const &config_xml)
		{
			try {
				Acpi::generate_report(env, heap, config_xml);
			} catch (Genode::Xml_generator::Buffer_exceeded) {
				error("ACPI report too large - failure");
				throw;
			} catch (...) {
				error("Unknown exception occured - failure");
				throw;
			}
		}
	};

	Acpi_reporter         _acpi_reporter { _env, _heap, _config.xml() };
	Smbios_table_reporter _smbt_reporter { _env, _heap };

	Main(Env &env) : _env(env) { }
};


void Component::construct(Genode::Env &env) { static Acpi::Main main(env); }
