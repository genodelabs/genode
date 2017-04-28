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
#include <drivers/defs/pbxa9.h>
#include <input/component.h>
#include <input/root.h>

/* local includes */
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"
#include "pl050.h"

namespace Ps2 { struct Main; }


struct Ps2::Main
{
	Genode::Env &_env;

	enum {
		PL050_KEYBD_PHYS = 0x10006000,
		PL050_KEYBD_SIZE = 0x1000,
		PL050_MOUSE_PHYS = 0x10007000,
		PL050_MOUSE_SIZE = 0x1000,
		PL050_KEYBD_IRQ = Pbxa9::KMI_0_IRQ,
		PL050_MOUSE_IRQ = Pbxa9::KMI_1_IRQ,
	};

	Pl050                    _pl050 { _env, PL050_KEYBD_PHYS, PL050_KEYBD_SIZE,
	                                  PL050_MOUSE_PHYS, PL050_MOUSE_SIZE };
	Input::Session_component _session { _env, _env.ram() };
	Input::Root_component    _root { _env.ep().rpc_ep(), _session };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Verbose _verbose { _config.xml() };

	Mouse    _mouse    { _pl050.aux_interface(), _session.event_queue(),        _verbose };
	Keyboard _keyboard { _pl050.kbd_interface(), _session.event_queue(), false, _verbose };

	Irq_handler _mouse_irq    { _env, PL050_MOUSE_IRQ, _pl050.aux_interface(), _mouse };
	Irq_handler _keyboard_irq { _env, PL050_KEYBD_IRQ, _pl050.kbd_interface(), _keyboard };

	Main(Genode::Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Ps2::Main main(env); }

