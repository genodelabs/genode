/*
 * \brief  Access to GPIO driver configuration
 * \author Stefan Kalkowski
 * \date   2013-05-06
 *
 * Taken from the OMAP4 GPIO driver written by Ivan Loskutov.
 *
 * Configure GPIO
 * Example:
 * <config>
 *     <gpio num="123" mode="I"/>
 *     <gpio num="124" mode="O" value="0"/>
 * </config>
 *
 * num - GPIO pin number
 * mode - input(I) or output(O)
 * value - output level (1 or 0), only for output mode
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPIO__CONFIG_H_
#define _INCLUDE__GPIO__CONFIG_H_

/* Genode includes */
#include <base/exception.h>
#include <gpio/driver.h>
#include <util/xml_node.h>

namespace Gpio {

	class Invalid_mode : Genode::Exception {};

	static void process_config(Genode::Xml_node const &config, Gpio::Driver &driver);
}


void Gpio::process_config(Genode::Xml_node const &config, Gpio::Driver &driver)
{
	if (!config.has_sub_node("gpio"))
		Genode::warning("no GPIO config");

	config.for_each_sub_node("gpio", [&] (Genode::Xml_node const &gpio_node) {

		unsigned const num = gpio_node.attribute_value("num", 0U);
		if (!driver.gpio_valid(num)) {
			Genode::warning("invalid GPIO number ", num, ", ignore node");
			return;
		}

		typedef Genode::String<2> Mode;
		Mode const mode = gpio_node.attribute_value("mode", Mode());

		unsigned value = 0;

		if (mode == "O" || mode == "o") {
			value = gpio_node.attribute_value("value", value);
			driver.write(num, value);
			driver.direction(num, false);
		}
		else if (mode == "I" || mode == "i") {
			driver.direction(num, true);
		}
		else {
			Genode::error("gpio ", num, " has invalid mode, must be I or O");
			throw Invalid_mode();
		}

		Genode::log("gpio ", num, " mode ", mode, " value=", value);
	});
}

#endif /* _INCLUDE__GPIO__CONFIG_H_ */
