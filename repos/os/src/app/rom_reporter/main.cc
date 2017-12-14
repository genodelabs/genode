/*
 * \brief  Reflect content of ROM module as a report
 * \author Norman Feske
 * \date   2017-12-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>

namespace Rom_reporter {
	using namespace Genode;
	struct Rom_module;
	struct Main;
}


struct Rom_reporter::Rom_module
{
	Env &_env;

	typedef String<160> Label;
	Label const _label;

	Attached_rom_dataspace _rom_ds { _env, _label.string() };

	size_t _report_size = 0;

	Constructible<Reporter> _reporter { };

	Signal_handler<Rom_module> _rom_update_handler {
		_env.ep(), *this, &Rom_module::_handle_rom_update };

	void _handle_rom_update()
	{
		_rom_ds.update();

		Xml_node const xml = _rom_ds.xml();

		size_t const content_size = xml.size();

		if (!_reporter.constructed() || (content_size > _report_size)) {
			_reporter.construct(_env, "", _label.string(), content_size);
			_reporter->enabled(true);
		}

		_reporter->report(xml.addr(), content_size);
	}

	Rom_module(Env &env, Label const &label) : _env(env), _label(label)
	{
		_rom_ds.sigh(_rom_update_handler);
		_handle_rom_update();
	}
};


struct Rom_reporter::Main
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Heap _heap { _env.ram(), _env.rm() };

	Main(Genode::Env &env) : _env(env)
	{
		_config.xml().for_each_sub_node("rom", [&] (Xml_node const &rom) {
			new (_heap)
				Rom_module(_env, rom.attribute_value("label",
				                                     Rom_module::Label()));
		});
	}
};


void Component::construct(Genode::Env &env) { static Rom_reporter::Main main(env); }
