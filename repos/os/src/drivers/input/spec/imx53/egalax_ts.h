/*
 * \brief  EETI eGalaxy touchscreen driver
 * \author Stefan Kalkowski
 * \date   2013-03-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__IMX53__EGALAX_TS_H_
#define _DRIVERS__INPUT__SPEC__IMX53__EGALAX_TS_H_

/* Genode includes */
#include <drivers/board_base.h>
#include <base/attached_io_mem_dataspace.h>
#include <input/event_queue.h>
#include <input/event.h>
#include <input/keycodes.h>

/* local includes */
#include <i2c.h>

namespace Input {
	class Touchscreen;
}


class Input::Touchscreen {

	private:

		enum I2c_addresses { I2C_ADDR = 0x4    };
		enum Finger_state  { PRESSED, RELEASED };

		Irq_handler                       _irq_handler;
		Genode::Attached_io_mem_dataspace _i2c_ds;
		I2c::I2c                          _i2c;
		Genode::uint8_t                   _buf[10];
		Finger_state                      _state;

	public:

		Touchscreen(Genode::Env &env, Timer::Connection &timer)
		:
			_irq_handler(env, Genode::Board_base::I2C_3_IRQ),
			_i2c_ds(env,
			        Genode::Board_base::I2C_3_BASE,
			        Genode::Board_base::I2C_3_SIZE),
			_i2c(timer,
			     (Genode::addr_t)_i2c_ds.local_addr<void>(),
			     _irq_handler),
			     _state(RELEASED)
		{
			/* ask for touchscreen firmware version */
			Genode::uint8_t cmd[10] = { 0x03, 0x03, 0xa, 0x01, 0x41 };
			_i2c.send(I2C_ADDR, cmd, sizeof(cmd));
		}

		void event(Event_queue &ev_queue)
		{
			_i2c.recv(I2C_ADDR, _buf, sizeof(_buf));

			/* ignore all events except of multitouch*/
			if (_buf[0] != 4)
				return;

			int x = (_buf[3] << 8) | _buf[2];
			int y = (_buf[5] << 8) | _buf[4];

			Genode::uint8_t state = _buf[1];
			bool            valid = state & (1 << 7);
			int             id    = (state >> 2) & 0xf;
			int             down  = state & 1;

			if (!valid || id > 5)
				return; /* invalid point */

			x = 102400 / (3276700 / x);
			y = 76800 / (3276700 / y);

			/* motion event */
			ev_queue.add(Input::Event(Input::Event::MOTION, 0, x, y, 0, 0));

			/* button event */
			if ((down  && (_state == RELEASED)) ||
				(!down && (_state == PRESSED))) {
				ev_queue.add(Input::Event(down ? Input::Event::PRESS
				                          : Input::Event::RELEASE,
				                          Input::BTN_LEFT, 0, 0, 0, 0));
				_state = down ? PRESSED : RELEASED;
			}
		}
};

#endif /* _DRIVERS__INPUT__SPEC__IMX53__EGALAX_TS_H_ */
