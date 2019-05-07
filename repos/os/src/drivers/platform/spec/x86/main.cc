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

	Genode::Constructible<Genode::Attached_rom_dataspace> acpi_rom { };
	Genode::Constructible<Platform::Root> root { };

	Genode::Constructible<Genode::Attached_rom_dataspace> system_state { };
	Genode::Constructible<Genode::Attached_rom_dataspace> acpi_ready { };

	Genode::Signal_handler<Platform::Main> _acpi_report;
	Genode::Signal_handler<Platform::Main> _system_report;

	Genode::Capability<Genode::Typed_root<Platform::Session_component> > root_cap { };

	bool const _acpi_platform;
	bool _acpi_ready    = false;

	void acpi_update()
	{
		acpi_rom->update();

		if (!acpi_rom->valid() || root.constructed())
			return;

		const char * report_addr = acpi_rom->local_addr<const char>();

		root.construct(_env, sliced_heap, _config, report_addr, _acpi_platform);

		root_cap = _env.ep().manage(*root);

		if (_acpi_ready) {
			Genode::Parent::Service_name announce_for_acpi("Acpi");
			_env.parent().announce(announce_for_acpi, root_cap);
		} else
			_env.parent().announce(root_cap);
	}

	void system_update()
	{
		if (acpi_ready.constructed())
			acpi_ready->update();

		if (!root.constructed())
			return;

		if (acpi_ready.constructed() && acpi_ready->valid()) {
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

	static bool acpi_platform(Genode::Env & env)
	{
		using Name = String<32>;
		try {
			Genode::Attached_rom_dataspace info { env, "platform_info" };
			Name kernel =
				info.xml().sub_node("kernel").attribute_value("name", Name());
			if (kernel == "hw"   ||
			    kernel == "nova" ||
			    kernel == "foc"  ||
			    kernel == "sel4") { return true; }
		} catch (...) {}
		return false;
	}

	Main(Genode::Env &env)
	:
		_env(env),
		_acpi_report(_env.ep(), *this, &Main::acpi_update),
		_system_report(_env.ep(), *this, &Main::system_update),
		_acpi_platform(acpi_platform(env))
	{
		const Genode::Xml_node config = _config.xml();

		_acpi_ready = config.attribute_value("acpi_ready", false);

		if (_acpi_ready) {
			acpi_ready.construct(env, "acpi_ready");
			acpi_ready->sigh(_system_report);
		}

		/* wait for the first valid acpi report */
		acpi_rom.construct(env, "acpi");
		acpi_rom->sigh(_acpi_report);
		/* check if already valid */
		acpi_update();
		system_update();
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Platform::Main main(env);
}
