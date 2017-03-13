/*
 * \brief  Test GPIO driver with Leds
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
	Attached_rom_dataspace  config   { env, "config" };
	unsigned                delay    { config.xml().attribute_value("delay", (unsigned)1000) };
	unsigned                gpio_pin { config.xml().attribute_value("gpio_pin", (unsigned)16) };
	unsigned                times    { config.xml().attribute_value("times", (unsigned)10) };
	Gpio::Connection        led      { env, gpio_pin };
	Timer::Connection       timer    { env };

	Main(Env &env);
};


Main::Main(Env &env) : env(env)
{
	log("--- GPIO Led test [GPIO Pin: ", gpio_pin, ", "
	    "Timer delay: ", delay, ", Times: ", times, "] ---");

	while (times--) {
		log("Remaining blinks: ", times);
		led.write(false);
		timer.msleep(delay);
		led.write(true);
		timer.msleep(delay);
	}
	log("Test finished");
}


void Component::construct(Env &env) { static Main main(env); }
