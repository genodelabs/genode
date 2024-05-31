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
#include <platform_session/device.h>
#include <timer_session/connection.h>
#include <event_session/connection.h>

/* local includes */
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "pl050.h"
#include "led_state.h"

namespace Ps2 {

	using namespace Genode;

	struct Main;
}


struct Ps2::Main
{
	Env & _env;

	Platform::Connection _platform { _env };

	using Device = Platform::Device;

	Device _device { _platform };

	Device::Mmio<0> _mmio_keyboard { _device, { 0 } };
	Device::Mmio<0> _mmio_mouse    { _device, { 1 } };

	Device::Irq _irq_keyboard { _device, { 0 } };
	Device::Irq _irq_mouse    { _device, { 1 } };

	Pl050 _pl050 { _mmio_keyboard, _mmio_mouse };

	Event::Connection        _event   { _env };
	Timer::Connection        _timer   { _env };
	Attached_rom_dataspace   _config  { _env, "config" };
	Reconstructible<Verbose> _verbose { _config.xml()  };

	Mouse    _mouse    { _pl050.aux_interface(), _timer, *_verbose };
	Keyboard _keyboard { _pl050.kbd_interface(), false, *_verbose };

	void _handle_irq_common()
	{
		_event.with_batch([&] (Event::Session_client::Batch &batch) {
			while (_mouse   .event_pending()) _mouse   .handle_event(batch);
			while (_keyboard.event_pending()) _keyboard.handle_event(batch);
		});
	}

	void _handle_irq_keyboard() { _irq_keyboard.ack(); _handle_irq_common(); }
	void _handle_irq_mouse()    { _irq_mouse   .ack(); _handle_irq_common(); }

	Signal_handler<Main> _keyboard_irq_handler {
		_env.ep(), *this, &Main::_handle_irq_keyboard };

	Signal_handler<Main> _mouse_irq_handler {
		_env.ep(), *this, &Main::_handle_irq_mouse };

	Led_state _capslock { _env, "capslock" },
	          _numlock  { _env, "numlock"  },
	          _scrlock  { _env, "scrlock"  };

	void _handle_config()
	{
		_config.update();

		Xml_node config = _config.xml();

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

		_irq_keyboard.sigh(_keyboard_irq_handler);
		_irq_mouse   .sigh(_mouse_irq_handler);
	}
};


void Component::construct(Genode::Env &env) { static Ps2::Main main(env); }
