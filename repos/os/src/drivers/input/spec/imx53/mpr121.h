/*
 * \brief  Freescale MPR121 capacitative button driver
 * \author Stefan Kalkowski
 * \date   2013-03-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__IMX53__MPR121_H_
#define _DRIVERS__INPUT__SPEC__IMX53__MPR121_H_

/* Genode includes */
#include <drivers/defs/imx53.h>
#include <base/attached_io_mem_dataspace.h>
#include <input/event_queue.h>
#include <input/event.h>
#include <input/keycodes.h>

/* local includes */
#include <i2c.h>

namespace Input {
	class Buttons;
}


class Input::Buttons {

	private:

		enum {
			I2C_ADDR = 0x5a,
		};

		enum Events {
			RELEASE = 0,
			BACK    = 1,
			HOME    = 2,
			MENU    = 4,
			POWER   = 8,
		};

		Irq_handler                       _irq_handler;
		Genode::Attached_io_mem_dataspace _i2c_ds;
		I2c::I2c                          _i2c;
		Genode::uint8_t                   _state;

	public:

		Buttons(Genode::Env &env, Timer::Connection &timer)
		:
			_irq_handler(env, Imx53::I2C_2_IRQ),
			_i2c_ds(env, Imx53::I2C_2_BASE, Imx53::I2C_2_SIZE),
			_i2c(timer,
			     (Genode::addr_t)_i2c_ds.local_addr<void>(),
			     _irq_handler),
			_state(0)
		{
			static Genode::uint8_t init_cmd[][2] = {
				{0x41, 0x8 }, {0x42, 0x5 }, {0x43, 0x8 },
				{0x44, 0x5 }, {0x45, 0x8 }, {0x46, 0x5 },
				{0x47, 0x8 }, {0x48, 0x5 }, {0x49, 0x8 },
				{0x4a, 0x5 }, {0x4b, 0x8 }, {0x4c, 0x5 },
				{0x4d, 0x8 }, {0x4e, 0x5 }, {0x4f, 0x8 },
				{0x50, 0x5 }, {0x51, 0x8 }, {0x52, 0x5 },
				{0x53, 0x8 }, {0x54, 0x5 }, {0x55, 0x8 },
				{0x56, 0x5 }, {0x57, 0x8 }, {0x58, 0x5 },
				{0x59, 0x8 }, {0x5a, 0x5 }, {0x2b, 0x1 },
				{0x2c, 0x1 }, {0x2d, 0x0 }, {0x2e, 0x0 },
				{0x2f, 0x1 }, {0x30, 0x1 }, {0x31, 0xff},
				{0x32, 0x2 }, {0x5d, 0x4 }, {0x5c, 0xb },
				{0x7b, 0xb }, {0x7d, 0xc9}, {0x7e, 0x82},
				{0x7f, 0xb4}, {0x5e, 0x84}};

			/* initialize mpr121 touch button device */
			for (unsigned i = 0; i < sizeof(init_cmd)/2; i++)
				_i2c.send(I2C_ADDR, init_cmd[i], 2);
		}

		void event(Event_queue &ev_queue)
		{
			int buttons[] = { BACK, HOME, MENU, POWER };
			int codes[]   = { Input::KEY_BACK, Input::KEY_HOME,
			                  Input::KEY_MENU, Input::KEY_POWER};

			Genode::uint8_t buf =  0;
			_i2c.send(I2C_ADDR, &buf, 1);
			_i2c.recv(I2C_ADDR, &buf, 1);

			for (unsigned i = 0; i < (sizeof(buttons)/sizeof(int)); i++) {
				if ((_state & buttons[i]) == (buf & buttons[i]))
					continue;
				Input::Event::Type event = (buf & buttons[i]) ?
					Input::Event::PRESS : Input::Event::RELEASE;
				ev_queue.add(Input::Event(event, codes[i], 0, 0, 0, 0));
			};
			_state = buf;
		}
};

#endif /* _DRIVERS__INPUT__SPEC__IMX53__MPR121_H_ */
