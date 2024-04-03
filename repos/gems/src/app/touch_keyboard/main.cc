/*
 * \brief  Simple touch-screen keyboard
 * \author Norman Feske
 * \date   2022-01-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <event_session/connection.h>
#include <dialog/runtime.h>
#include <util/color.h>

/* local includes */
#include <touch_keyboard_widget.h>

namespace Touch_keyboard {

	using namespace Dialog;

	struct Main;
}


struct Touch_keyboard::Main : Top_level_dialog
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };
	Attached_rom_dataspace _layout { _env, "layout" };

	Hosted<Touch_keyboard_widget> _keyboard { Id { "keyboard" }, _heap };

	Event::Connection _event_connection { _env };

	Runtime _runtime { _env, _heap };

	Runtime::View _view { _runtime, *this };

	/*
	 * Top_level_dialog interface
	 */

	void view(Scope<> &s) const override { s.widget(_keyboard); }

	void click(Clicked_at const &at) override { _keyboard.propagate(at); }

	void clack(Clacked_at const &at) override
	{
		_keyboard.propagate(at, [&] (Touch_keyboard_widget::Emit const &characters) {
			_event_connection.with_batch([&] (Event::Session_client::Batch &batch) {

				Utf8_ptr utf8_ptr(characters.string());

				for (; utf8_ptr.complete(); utf8_ptr = utf8_ptr.next()) {

					Codepoint c = utf8_ptr.codepoint();
					batch.submit( Input::Press_char { Input::KEY_UNKNOWN, c } );
					batch.submit( Input::Release    { Input::KEY_UNKNOWN } );
				}
			});
		});
	}

	void drag (Dragged_at const &)   override { }

	void _handle_config()
	{
		_config.update();
		_layout.update();

		Xml_node const config = _config.xml();

		_view.xpos = (int)config.attribute_value("xpos", 0L);
		_view.ypos = (int)config.attribute_value("ypos", 0L);

		_view.min_width  = config.attribute_value("min_width",  0U);
		_view.min_height = config.attribute_value("min_height", 0U);

		_view.opaque     = config.attribute_value("opaque", false);
		_view.background = config.attribute_value("background", Color(127, 127, 127, 255));

		_keyboard.configure(_layout.xml());

		_runtime.update_view_config();
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Main(Env &env) : Top_level_dialog("touch_keyboard"), _env(env)
	{
		_config.sigh(_config_handler);
		_layout.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Touch_keyboard::Main main(env); }

