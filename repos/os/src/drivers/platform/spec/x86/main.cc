/*
 * \brief  Platform driver for x86
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>

#include <util/reconstructible.h>

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
	Genode::Env &_env;
	Genode::Sliced_heap sliced_heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Genode::Constructible<Genode::Attached_rom_dataspace> acpi_rom;
	Genode::Constructible<Platform::Root> root;

	Genode::Constructible<Genode::Attached_rom_dataspace> system_state;
	Genode::Constructible<Genode::Attached_rom_dataspace> acpi_ready;

	Genode::Signal_handler<Platform::Main> _acpi_report;
	Genode::Signal_handler<Platform::Main> _system_report;

	Genode::Capability<Genode::Typed_root<Platform::Session_component> > root_cap;

	bool _system_rom = false;

	void acpi_update()
	{
		acpi_rom->update();

		if (!acpi_rom->valid() || root.constructed())
			return;

		const char * report_addr = acpi_rom->local_addr<const char>();

		root.construct(_env, sliced_heap, _config, report_addr);

		root_cap = _env.ep().manage(*root);

		if (_system_rom) {
			Genode::Parent::Service_name announce_for_acpi("Acpi");
			_env.parent().announce(announce_for_acpi, root_cap);
		} else
			_env.parent().announce(root_cap);
	}

	void system_update()
	{
		if (!_system_rom || !system_state.is_constructed() ||
		    !acpi_ready.is_constructed())
			return;

		system_state->update();
		acpi_ready->update();

		if (!root.is_constructed())
			return;

		if (system_state->is_valid()) {
			Genode::Xml_node system(system_state->local_addr<char>(),
			                        system_state->size());

			typedef Genode::String<16> Value;
			const Value state = system.attribute_value("state", Value("unknown"));

			if (state == "reset")
				root->system_reset();
		}
		if (acpi_ready->is_valid()) {
			Genode::Xml_node system(acpi_ready->local_addr<char>(),
			                        acpi_ready->size());

			typedef Genode::String<16> Value;
			const Value state = system.attribute_value("state", Value("unknown"));

			if (state == "acpi_ready" && root_cap.valid()) {
				_env.parent().announce(root_cap);
				root_cap = Genode::Capability<Genode::Typed_root<Platform::Session_component> > ();
			}
		}
	}

	Main(Genode::Env &env)
	:
		_env(env),
		_acpi_report(_env.ep(), *this, &Main::acpi_update),
		_system_report(_env.ep(), *this, &Main::system_update)
	{
		const Genode::Xml_node config = _config.xml();

		_system_rom = config.attribute_value("system", false);

		typedef Genode::String<8> Value;
		Value const wait_for_acpi = config.attribute_value("acpi", Value("yes"));

		if (_system_rom) {
			/* wait for system state changes, e.g. reset and acpi_ready */
			system_state.construct(env, "system");
			system_state->sigh(_system_report);
			acpi_ready.construct(env, "acpi_ready");
			acpi_ready->sigh(_system_report);
		}

		if (wait_for_acpi == "yes") {
			/* for ACPI support, wait for the first valid acpi report */
			acpi_rom.construct(env, "acpi");
			acpi_rom->sigh(_acpi_report);
			/* check if already valid */
			acpi_update();
			system_update();
			return;
		}

		/* non ACPI platform case */
		root.construct(_env, sliced_heap, _config, nullptr);
		_env.parent().announce(_env.ep().manage(*root));
	}
};


void Component::construct(Genode::Env &env) { static Platform::Main main(env); }
