/*
 * \brief  PS/2 driver for x86
 * \author Norman Feske
 * \date   2007-09-21
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <util/retry.h>

/* os includes */
#include <event_session/connection.h>
#include <legacy/x86/platform_session/connection.h>
#include <timer_session/connection.h>

/* local includes */
#include "i8042.h"
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"
#include "verbose.h"
#include "led_state.h"

namespace Ps2 {

	using namespace Genode;

	struct Main;
}


struct Ps2::Main
{
	Env &_env;

	Event::Connection _event { _env };

	Platform::Connection _platform { _env };

	Timer::Connection _timer { _env };

	Platform::Device_capability _ps2_device_cap()
	{
		return _platform.with_upgrade([&] () {
			return _platform.device("PS2"); });
	}

	Platform::Device_client _device_ps2 { _ps2_device_cap() };

	enum { REG_IOPORT_DATA = 0, REG_IOPORT_STATUS = 1 };

	I8042 _i8042 { _device_ps2.io_port(REG_IOPORT_DATA),
	               _device_ps2.io_port(REG_IOPORT_STATUS) };

	Attached_rom_dataspace _config { _env, "config" };

	Constructible<Attached_rom_dataspace> _system { };

	Reconstructible<Verbose> _verbose { _config.xml() };

	Keyboard _keyboard { _i8042.kbd_interface(), _i8042.kbd_xlate(), *_verbose };

	Mouse _mouse { _i8042.aux_interface(), _timer, *_verbose };

	Irq_handler _keyboard_irq { _env.ep(), _keyboard, _event, _device_ps2.irq(0) };
	Irq_handler _mouse_irq    { _env.ep(), _mouse,    _event, _device_ps2.irq(1) };

	Led_state _capslock { _env, "capslock" },
	          _numlock  { _env, "numlock"  },
	          _scrlock  { _env, "scrlock"  };

	void _handle_config()
	{
		_config.update();

		Xml_node config = _config.xml();

		bool const system_was_constructed = _system.constructed();

		_system.conditional(config.attribute_value("system", false), _env, "system");

		if (_system.constructed() && !system_was_constructed)
			_system->sigh(_config_handler);

		if (_system.constructed()) {
			_system->update();

			if (_system->xml().attribute_value("state", String<16>()) == "reset") {
				log("trying to perform system reset via PS/2 port 0x64");
				_i8042.cpu_reset();
			}
		}

		_verbose.construct(config);

		_capslock.update(config, _config_handler);
		_numlock .update(config, _config_handler);
		_scrlock .update(config, _config_handler);

		_keyboard.led_enabled(Keyboard::CAPSLOCK_LED, _capslock.enabled());
		_keyboard.led_enabled(Keyboard::NUMLOCK_LED,  _numlock .enabled());
		_keyboard.led_enabled(Keyboard::SCRLOCK_LED,  _scrlock .enabled());
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct( Genode::Env &env) { static Ps2::Main ps2(env); }

