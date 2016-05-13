/*
 * \brief  Platform driver for x86
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/component.h>
#include <base/env.h>

#include <util/volatile_object.h>

#include <os/attached_rom_dataspace.h>

#include "pci_session_component.h"
#include "pci_device_config.h"
#include "device_pd.h"

namespace Platform {
	struct Main;
};

struct Platform::Main
{
	/*
	 * Use sliced heap to allocate each session component at a separate
	 * dataspace.
	 */
	Genode::Sliced_heap sliced_heap;

	Genode::Env &_env;

	Genode::Lazy_volatile_object<Genode::Attached_rom_dataspace> acpi_rom;
	Genode::Lazy_volatile_object<Platform::Root> root;

	Genode::Signal_handler<Platform::Main> _acpi_report;

	void acpi_update()
	{
		acpi_rom->update();

		if (!acpi_rom->valid() || root.constructed())
			return;

		const char * report_addr = acpi_rom->local_addr<const char>();

		root.construct(_env, &sliced_heap, report_addr);
		_env.parent().announce(_env.ep().manage(*root));
	}

	Main(Genode::Env &env)
	:
		sliced_heap(env.ram(), env.rm()),
		_env(env),
		_acpi_report(_env.ep(), *this, &Main::acpi_update)
	{
		typedef Genode::String<8> Value;
		Value const wait_for_acpi = Genode::config()->xml_node().attribute_value("acpi", Value("yes"));

		if (wait_for_acpi == "yes") {
			/* for ACPI support, wait for the first valid acpi report */
			acpi_rom.construct("acpi");
			acpi_rom->sigh(_acpi_report);
			return;
		}

		/* non ACPI platform case */
		root.construct(_env, &sliced_heap, nullptr);
		_env.parent().announce(_env.ep().manage(*root));
	}
};

Genode::size_t Component::stack_size()      { return STACK_SIZE; }
void Component::construct(Genode::Env &env) { static Platform::Main main(env); }
