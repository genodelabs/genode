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
#include <input/component.h>
#include <input/root.h>
#include <platform_session/connection.h>

/* local includes */
#include "i8042.h"
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"
#include "verbose.h"

namespace Ps2 { struct Main; }


struct Ps2::Main
{
	Genode::Env &_env;

	Input::Session_component _session { _env, _env.ram() };
	Input::Root_component    _root { _env.ep().rpc_ep(), _session };

	Platform::Connection _platform { _env };

	Platform::Device_capability _ps2_device_cap()
	{
		return Genode::retry<Platform::Session::Out_of_metadata>(
			[&] () { return _platform.device("PS2"); },
			[&] () { _platform.upgrade_ram(4096); });
	}

	Platform::Device_client _device_ps2 { _ps2_device_cap() };

	enum { REG_IOPORT_DATA = 0, REG_IOPORT_STATUS };

	I8042 _i8042 { _device_ps2.io_port(REG_IOPORT_DATA),
	               _device_ps2.io_port(REG_IOPORT_STATUS) };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Verbose _verbose { _config.xml() };

	Keyboard _keyboard { _i8042.kbd_interface(), _session.event_queue(),
	                     _i8042.kbd_xlate(), _verbose };

	Mouse _mouse { _i8042.aux_interface(), _session.event_queue(), _verbose };

	Irq_handler _keyboard_irq { _env.ep(), _keyboard, _device_ps2.irq(0) };
	Irq_handler _mouse_irq    { _env.ep(), _mouse,    _device_ps2.irq(1) };

	Main(Genode::Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct( Genode::Env &env) { static Ps2::Main ps2(env); }

