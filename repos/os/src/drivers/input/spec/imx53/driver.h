/*
 * \brief  Input-driver
 * \author Stefan Kalkowski
 * \date   2013-03-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__IMX53__DRIVER_H_
#define _DRIVERS__INPUT__SPEC__IMX53__DRIVER_H_

/* Genode includes */
#include <base/env.h>
#include <base/signal.h>
#include <gpio_session/connection.h>

/* local includes */
#include <egalax_ts.h>
#include <mpr121.h>

namespace Input {
	class Tablet_driver;
}


class Input::Tablet_driver
{
	private:

		enum Gpio_irqs {
			GPIO_TOUCH  = 84,
			GPIO_BUTTON = 132,
		};

		Event_queue                              &_ev_queue;
		Gpio::Connection                          _gpio_ts;
		Gpio::Connection                          _gpio_bt;
		Genode::Irq_session_client                _irq_ts;
		Genode::Irq_session_client                _irq_bt;
		Genode::Io_signal_handler<Tablet_driver>  _ts_dispatcher;
		Genode::Io_signal_handler<Tablet_driver>  _bt_dispatcher;
		Touchscreen                               _touchscreen;
		Buttons                                   _buttons;

		void _handle_ts()
		{
			_touchscreen.event(_ev_queue);
			_irq_ts.ack_irq();
		}

		void _handle_bt()
		{
			_buttons.event(_ev_queue);
			_irq_bt.ack_irq();
		}

		Tablet_driver(Genode::Env &env, Event_queue &ev_queue)
		:
			_ev_queue(ev_queue),
			_gpio_ts(env, GPIO_TOUCH),
			_gpio_bt(env, GPIO_BUTTON),
			_irq_ts(_gpio_ts.irq_session(Gpio::Session::LOW_LEVEL)),
			_irq_bt(_gpio_bt.irq_session(Gpio::Session::FALLING_EDGE)),
			_ts_dispatcher(env.ep(), *this, &Tablet_driver::_handle_ts),
			_bt_dispatcher(env.ep(), *this, &Tablet_driver::_handle_bt),
			_touchscreen(env),
			_buttons(env)
		{
			/* GPIO touchscreen handling */
			_gpio_ts.direction(Gpio::Session::OUT);
			_gpio_ts.write(true);
			_gpio_ts.direction(Gpio::Session::IN);

			_irq_ts.sigh(_ts_dispatcher);
			_irq_ts.ack_irq();

			/* GPIO button handling */
			_gpio_bt.direction(Gpio::Session::OUT);
			_gpio_bt.write(true);
			_gpio_bt.direction(Gpio::Session::IN);

			_irq_bt.sigh(_bt_dispatcher);
			_irq_bt.ack_irq();
		}

	public:

		static Tablet_driver* factory(Genode::Env &env, Event_queue &ev_queue);
};

#endif /* _DRIVERS__INPUT__SPEC__IMX53__DRIVER_H_ */
