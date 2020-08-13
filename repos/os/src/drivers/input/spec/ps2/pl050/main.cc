/*
 * \brief  PS/2 driver for PL050
 * \author Norman Feske
 * \date   2007-09-21
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <platform_session/connection.h>
#include <timer_session/connection.h>
#include <event_session/connection.h>

/* local includes */
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"
#include "pl050.h"
#include "led_state.h"

namespace Ps2 { struct Main; }


struct Ps2::Main
{
	using Device = Platform::Device_client;

	Genode::Env        & _env;
	Platform::Connection _platform { _env };

	Device _device_0 { _platform.device_by_index(0) };
	Device _device_1 { _platform.device_by_index(1) };
	Pl050  _pl050    { _env, _device_0.io_mem_dataspace(),
	                         _device_1.io_mem_dataspace() };

	Event::Connection                _event   { _env };
	Timer::Connection                _timer   { _env };
	Genode::Attached_rom_dataspace   _config  { _env, "config" };
	Genode::Reconstructible<Verbose> _verbose { _config.xml()  };

	Mouse    _mouse    { _pl050.aux_interface(), _timer, *_verbose };
	Keyboard _keyboard { _pl050.kbd_interface(), false, *_verbose };

	Irq_handler _mouse_irq    { _env.ep(), _mouse,    _event, _device_1.irq() };
	Irq_handler _keyboard_irq { _env.ep(), _keyboard, _event, _device_0.irq() };

	Led_state _capslock { _env, "capslock" },
	          _numlock  { _env, "numlock"  },
	          _scrlock  { _env, "scrlock"  };

	void _handle_config()
	{
		_config.update();

		Genode::Xml_node config = _config.xml();

		_verbose.construct(config);

		_capslock.update(config, _config_handler);
		_numlock .update(config, _config_handler);
		_scrlock .update(config, _config_handler);

		_keyboard.led_enabled(Keyboard::CAPSLOCK_LED, _capslock.enabled());
		_keyboard.led_enabled(Keyboard::NUMLOCK_LED,  _numlock .enabled());
		_keyboard.led_enabled(Keyboard::SCRLOCK_LED,  _scrlock .enabled());
	}

	Genode::Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Main(Genode::Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Ps2::Main main(env); }
