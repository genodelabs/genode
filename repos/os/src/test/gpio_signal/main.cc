/*
 * \brief  Test GPIO driver
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \author Martin Stein
 * \date   2015-07-26
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <gpio_session/connection.h>
#include <irq_session/client.h>
#include <timer_session/connection.h>

using namespace Genode;


struct Main
{
	Env                    &env;
	Attached_rom_dataspace  config       { env, "config" };
	Signal_receiver         sig_rec      { };
	Signal_context          sig_ctx      { };
	Timer::Connection       timer        { env };
	unsigned                gpio_pin     { config.xml().attribute_value("gpio_pin", (unsigned)16) };
	unsigned                gpio_pin_in  { config.xml().attribute_value("gpio_pin_in", (unsigned)17) };
	unsigned                gpio_pin_out { config.xml().attribute_value("gpio_pin_out", (unsigned)18) };
	bool                    state        { config.xml().attribute_value("state", (unsigned)0) > 0 };

	Main(Env &env);
};


Main::Main(Env &env) : env(env)
{
	log("--- GPIO Signals test [LED "
	    "pin: ",           gpio_pin, ", "
	    "Input pin: ",     gpio_pin_in, ", "
	    "Output pin: ",    gpio_pin_out, ", "
	    "Initial state: ", state, "] ---");

	Gpio::Connection led(env, gpio_pin);
	Gpio::Connection signal_input(env, gpio_pin_in);
	Gpio::Connection signal_output(env, gpio_pin_out);

	/*
	 * Set pins direction.
	 * This values can be set by configuration
	 */
	signal_input.direction(Gpio::Session::IN);
	signal_output.direction(Gpio::Session::OUT);

	/*
	 * Power on the signal output
	 */
	signal_output.write(true);


	/*
	 * Initialize the pin IRQ signal
	 */
	Irq_session_client irq(signal_input.irq_session(Gpio::Session::HIGH_LEVEL));
	irq.sigh(sig_rec.manage(&sig_ctx));
	irq.ack_irq();

	while(true)
	{
		state = !state;
		led.write(state);

		/*
		 * Wait for a GIO signal on _signal_input
		 */
		sig_rec.wait_for_signal();

		/*
		 * Small delay between push button actions
		 */
		timer.msleep(100);

		/*
		 * Switch the LED state
		 */
		if(!state) {
			log("Led going OFF");
		} else {
			log("Led going ON"); }

		irq.ack_irq();
	}
}


void Component::construct(Env &env) { static Main main(env); }
