/*
 * \brief  Write content of ROM module to LOG
 * \author Norman Feske
 * \date   2015-09-22
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rom_session/connection.h>
#include <base/signal.h>
#include <os/config.h>
#include <os/attached_rom_dataspace.h>
#include <util/print_lines.h>
#include <os/server.h>
#include <util/volatile_object.h>

namespace Rom_logger { struct Main; }


struct Rom_logger::Main
{
	Server::Entrypoint &_ep;

	Genode::Lazy_volatile_object<Genode::Attached_rom_dataspace> _rom_ds;

	typedef Genode::String<100> Rom_name;

	/**
	 * Name of currently requested ROM module
	 *
	 * Solely used to detect configuration changes.
	 */
	Rom_name _rom_name;

	/**
	 * Signal handler that is invoked when the configuration or the ROM module
	 * changes.
	 */
	void _handle_update(unsigned);

	Genode::Signal_rpc_member<Main> _update_dispatcher =
		{ _ep, *this, &Main::_handle_update };

	Main(Server::Entrypoint &ep) : _ep(ep)
	{
		Genode::config()->sigh(_update_dispatcher);
		_handle_update(0);
	}
};


void Rom_logger::Main::_handle_update(unsigned)
{
	using Genode::config;

	config()->reload();

	/*
	 * Query name of ROM module from config
	 */
	Rom_name rom_name;
	try {
		rom_name = config()->xml_node().attribute_value("rom", rom_name);

	} catch (...) {
		PWRN("could not determine ROM name from config");
		return;
	}

	/*
	 * If ROM name changed, reconstruct '_rom_ds'
	 */
	if (rom_name != _rom_name) {
		_rom_ds.construct(rom_name.string());
		_rom_ds->sigh(_update_dispatcher);
		_rom_name = rom_name;
	}

	/*
	 * Update ROM module and print content to LOG
	 */
	if (_rom_ds.is_constructed()) {
		_rom_ds->update();

		if (_rom_ds->is_valid()) {
			PLOG("ROM '%s':", _rom_name.string());

			Genode::print_lines<200>(_rom_ds->local_addr<char>(), _rom_ds->size(),
			                         [&] (char const *line) { PLOG("  %s", line); });
		} else {
			PLOG("ROM '%s' is invalid", _rom_name.string());
		}
	}
}


namespace Server {

	char const *name() { return "ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Rom_logger::Main main(ep);
	}
}
