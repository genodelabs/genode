/*
 * \brief  Input-driver
 * \author Stefan Kalkowski
 * \date   2013-03-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <base/env.h>
#include <base/thread.h>
#include <base/signal.h>
#include <gpio_session/connection.h>

/* local includes */
#include <egalax_ts.h>
#include <mpr121.h>

namespace Input {
	class Tablet_driver;
}


class Input::Tablet_driver : Genode::Thread<8192>
{
	private:

		enum Gpio_irqs {
			GPIO_TOUCH  = 84,
			GPIO_BUTTON = 132,
		};

		Event_queue                      &_ev_queue;
		Gpio::Connection                  _gpio_ts;
		Gpio::Connection                  _gpio_bt;
		Genode::Signal_receiver           _receiver;
		Genode::Signal_context            _ts_rx;
		Genode::Signal_context            _bt_rx;
		Genode::Signal_context_capability _ts_sig_cap;
		Genode::Signal_context_capability _bt_sig_cap;
		Touchscreen                       _touchscreen;
		Buttons                           _buttons;

		Genode::Signal_context_capability _init_ts_gpio()
		{
			Genode::Signal_context_capability ret = _receiver.manage(&_ts_rx);
			_gpio_ts.direction(Gpio::Session::OUT);
			_gpio_ts.write(true);
			_gpio_ts.direction(Gpio::Session::IN);
			_gpio_ts.irq_sigh(_ts_sig_cap);
			_gpio_ts.irq_type(Gpio::Session::LOW_LEVEL);
			_gpio_ts.irq_enable(true);
			return ret;
		}

		Genode::Signal_context_capability _init_bt_gpio()
		{
			Genode::Signal_context_capability ret = _receiver.manage(&_bt_rx);
			_gpio_bt.direction(Gpio::Session::OUT);
			_gpio_bt.write(true);
			_gpio_bt.direction(Gpio::Session::IN);
			_gpio_bt.irq_sigh(_bt_sig_cap);
			_gpio_bt.irq_type(Gpio::Session::FALLING_EDGE);
			_gpio_bt.irq_enable(true);
			return ret;
		}

		Tablet_driver(Event_queue &ev_queue)
		: Thread("touchscreen_signal_handler"),
		  _ev_queue(ev_queue),
		  _gpio_ts(GPIO_TOUCH),
		  _gpio_bt(GPIO_BUTTON),
		  _ts_sig_cap(_init_ts_gpio()),
		  _bt_sig_cap(_init_bt_gpio()) { start(); }

	public:

		static Tablet_driver* factory(Event_queue &ev_queue);

		void entry()
		{
			while (true) {
				Genode::Signal sig = _receiver.wait_for_signal();
				if (sig.context() == &_ts_rx)
					_touchscreen.event(_ev_queue);
				else if (sig.context() == &_bt_rx)
					_buttons.event(_ev_queue);
			}
		}
};


Input::Tablet_driver* Input::Tablet_driver::factory(Event_queue &ev_queue)
{
	static Input::Tablet_driver driver(ev_queue);
	return &driver;
}

#endif /* _DRIVER_H_ */
