/*
 * \brief  Test GPIO driver with Leds
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

int main(int, char **)
{
	unsigned int _delay = 1000;
	unsigned int _gpio_pin = 16;
	unsigned int _times = 10;

	try {
		Genode::config()->xml_node().attribute("gpio_pin").value(&_gpio_pin);
	} catch (...) { }

	try {
		Genode::config()->xml_node().attribute("delay").value(&_delay);
	} catch (...) { }

	try {
		Genode::config()->xml_node().attribute("times").value(&_times);
	} catch (...) { }

	Genode::log("--- GPIO Led test [GPIO Pin: ", _gpio_pin, ", "
	            "Timer delay: ", _delay, ", Times: ", _times, "] ---");

	Gpio::Connection _led(_gpio_pin);
	Timer::Connection _timer;

	while(_times--)
	{
		Genode::log("Remains blinks: ",_times);
		_led.write(false);
		_timer.msleep(_delay);
		_led.write(true);
		_timer.msleep(_delay);
	}

	Genode::log("Test finished");
	return 0;
}
