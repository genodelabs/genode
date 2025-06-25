/*
 * \brief  Input-focus policy for the nitpicker GUI server
 * \author Norman Feske
 * \date   2017-11-19
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
#include <os/session_policy.h>
#include <os/reporter.h>

namespace Nit_focus {
	using namespace Genode;
	struct Main;
}


struct Nit_focus::Main
{
	Env &_env;

	Attached_rom_dataspace _config_rom  { _env, "config" };
	Attached_rom_dataspace _clicked_rom { _env, "clicked" };

	Expanding_reporter _focus_reporter { _env, "focus" };

	void _handle_update()
	{
		_clicked_rom.update();
		_config_rom.update();

		using Label = String<160>;
		Label const label = _clicked_rom.node().attribute_value("label", Label());

		with_matching_policy(label, _config_rom.node(), [&] (Node const &policy) {
			if (policy.attribute_value("focus", true)) {
				_focus_reporter.generate([&] (Xml_generator &xml) {
					xml.attribute("label", label); });
			}
		}, [&] { /* don't change focus on policy mismatch */ });
	}

	Signal_handler<Main> _update_handler {
		_env.ep(), *this, &Main::_handle_update };

	Main(Env &env) : _env(env)
	{
		_clicked_rom.sigh(_update_handler);
		_handle_update();
	}
};


void Component::construct(Genode::Env &env) { static Nit_focus::Main main(env); }
