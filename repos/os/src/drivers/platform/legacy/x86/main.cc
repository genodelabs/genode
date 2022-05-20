/*
 * \brief  Platform driver for x86
 * \author Norman Feske
 * \author Christian Helmuth
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
#include "acpi_devices.h"

namespace Platform {
	struct Main;

	namespace Nonpci { void acpi_device_registry(Acpi::Device_registry &); }
};

struct Platform::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Acpi::Device_registry _acpi_device_registry { };

	/*
	 * Use sliced heap to allocate each session component at a separate
	 * dataspace.
	 */
	Sliced_heap sliced_heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Constructible<Attached_rom_dataspace> acpi_rom { };
	Constructible<Platform::Root> root { };

	Constructible<Attached_rom_dataspace> system_state { };
	Constructible<Attached_rom_dataspace> acpi_ready { };

	Signal_handler<Main> _acpi_report    { _env.ep(), *this,
	                                       &Main::acpi_update };
	Signal_handler<Main> _system_report  { _env.ep(), *this,
	                                       &Main::system_update };
	Signal_handler<Main> _config_handler { _env.ep(), *this,
	                                       &Main::config_update };

	Capability<Typed_root<Platform::Session_component> > root_cap { };

	bool _acpi_ready    = false;

	void _attempt_acpi_reset();

	void acpi_update()
	{
		if (!root.constructed()) {
			acpi_rom->update();

			if (!acpi_rom->valid())
				return;

			bool msi_platform  = false;
			bool acpi_platform = false;

			try {
				Attached_rom_dataspace info { _env, "platform_info" };
				info.xml().with_sub_node("kernel", [&] (Xml_node const &node) {
					acpi_platform = node.attribute_value("acpi", acpi_platform);
					msi_platform  = node.attribute_value("msi" , msi_platform);
				});
			} catch (...) { }

			root.construct(_env, _heap, sliced_heap, _config,
			               acpi_rom->local_addr<const char>(), acpi_platform,
			               msi_platform);
		}

		if (root_cap.valid())
			return;

		/* don't announce service if no policy entry is available */
		if (!root->config_with_policy())
			return;

		root_cap = _env.ep().manage(*root);

		if (_acpi_ready) {
			Parent::Service_name announce_for_acpi("Acpi");
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
			Xml_node system(acpi_ready->local_addr<char>(), acpi_ready->size());

			typedef String<16> Value;
			const Value state = system.attribute_value("state", Value("unknown"));

			if (state == "acpi_ready" && root_cap.valid()) {
				_env.parent().announce(root_cap);
				root_cap = Capability<Typed_root<Platform::Session_component> > ();
			}
		}
	}

	void config_update()
	{
		_config.update();

		if (!_config.valid())
			return;

		if (!root_cap.valid())
			acpi_update();

		bool const system_state_was_constructed = system_state.constructed();

		system_state.conditional(_config.xml().attribute_value("system", false),
		                         _env, "system");

		if (system_state.constructed() && !system_state_was_constructed)
			system_state->sigh(_config_handler);

		if (system_state.constructed()) {
			system_state->update();
			if (system_state->xml().attribute_value("state", String<16>()) == "reset")
				_attempt_acpi_reset();
		}

		if (root.constructed()) {
			root->generate_pci_report();
			root->config_update();
		}

		_acpi_device_registry.init_devices(_heap, _config.xml());
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);

		if (_config.valid())
			_acpi_ready = _config.xml().attribute_value("acpi_ready", false);

		if (_acpi_ready) {
			acpi_ready.construct(env, "acpi_ready");
			acpi_ready->sigh(_system_report);
		}

		/* wait for the first valid acpi report */
		acpi_rom.construct(env, "acpi");
		acpi_rom->sigh(_acpi_report);

		/* check if already valid */
		config_update();
		acpi_update();
		system_update();

		Nonpci::acpi_device_registry(_acpi_device_registry);
	}
};


void Platform::Main::_attempt_acpi_reset()
{
	if (!acpi_rom.constructed())
		return;

	acpi_rom->xml().with_sub_node("reset", [&] (Xml_node reset) {

		uint16_t const io_port = reset.attribute_value("io_port", (uint16_t)0);
		uint8_t  const value   = reset.attribute_value("value",   (uint8_t)0);

		log("trigger reset by writing value ", value, " to I/O port ", Hex(io_port));

		try {
			Io_port_connection reset_port { _env, io_port, 1 };
			reset_port.outb(io_port, value);
		}
		catch (...) {
			error("unable to access reset I/O port ", Hex(io_port));
		}
	});
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Platform::Main main(env);
}
