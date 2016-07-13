/*
 * \brief  Test GPIO driver
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-26
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/signal.h>
#include <gpio_session/connection.h>
#include <irq_session/client.h>
#include <util/mmio.h>
#include <timer_session/connection.h>

#include <os/config.h>

using namespace Genode;

int main(int, char **)
{
	Signal_receiver   sig_rec;
	Signal_context    sig_ctx;
	Timer::Connection _timer;

	unsigned _gpio_pin     = 16;
	unsigned _gpio_pin_in  = 17;
	unsigned _gpio_pin_out = 18;
	unsigned _tmp          = 0;
	bool _state            = false;

	try {
		Genode::config()->xml_node().attribute("gpio_pin").value(&_gpio_pin);
	} catch (...) { }

	try {
		Genode::config()->xml_node().attribute("gpio_pin_in").value(&_gpio_pin_in);
	} catch (...) { }

	try {
		Genode::config()->xml_node().attribute("gpio_pin_out").value(&_gpio_pin_out);
	} catch (...) { }

	try {
		Genode::config()->xml_node().attribute("state").value(&_tmp);
	} catch (...) { }
	_state = _tmp>0;

	log("--- GPIO Signals test [LED "
	    "pin: ",           _gpio_pin, ", "
	    "Input pin: ",     _gpio_pin_in, ", "
	    "Output pin: ",    _gpio_pin_out, ", "
	    "Initial state: ", _state, "] ---");

	Gpio::Connection _led(_gpio_pin);
	Gpio::Connection _signal_input(_gpio_pin_in);
	Gpio::Connection _signal_output(_gpio_pin_out);

	/*
	 * Set pins direction.
	 * This values can be set by configuration
	 */
	_signal_input.direction(Gpio::Session::IN);
	_signal_output.direction(Gpio::Session::OUT);


	/*
	 * Power on the signal output
	 */
	_signal_output.write(true);


	/*
	 * Initialize the pin IRQ signal
	 */
	Genode::Irq_session_client irq(_signal_input.irq_session(Gpio::Session::HIGH_LEVEL));
	irq.sigh(sig_rec.manage(&sig_ctx));
	irq.ack_irq();

	while(1)
	{
		_state = !_state;
		_led.write(_state);

		/*
		 * Wait for a GIO signal on _signal_input
		 */
		sig_rec.wait_for_signal();

		/*
		 * Small delay between push button actions
		 */
		_timer.msleep(100);

		/*
		 * Switch the LED state
		 */
		if(!_state)
		{
			Genode::log("Led going OFF");
		}else
		{
			Genode::log("Led going ON");
		}

		irq.ack_irq();
	}

	return 0;
}
