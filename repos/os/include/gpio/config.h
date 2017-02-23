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

	class Invalid_gpio_number : Genode::Exception {};
	class Invalid_mode        : Genode::Exception {};

	static void process_config(Genode::Xml_node const &config, Gpio::Driver &driver);
}


void Gpio::process_config(Genode::Xml_node const &config, Gpio::Driver &driver)
{
	try {
		Genode::Xml_node gpio_node = config.sub_node("gpio");

		for (;; gpio_node = gpio_node.next("gpio")) {
			unsigned num     = 0;
			char     mode[2] = {0};
			unsigned value   = 0;

			try {
				gpio_node.attribute("num").value(&num);
				if (!driver.gpio_valid(num)) throw Invalid_gpio_number();
				gpio_node.attribute("mode").value(mode, sizeof(mode));
				if (mode[0] == 'O' || mode[0] == 'o') {
					gpio_node.attribute("value").value(&value);
					driver.write(num, value);
					driver.direction(num, false);
				} else if (mode[0] == 'I' || mode[0] == 'i') {
					driver.direction(num, true);
				} else throw Invalid_mode();

				Genode::log("gpio ",  num, " "
				            "mode ",  Genode::Cstring(mode), " "
				            "value=", value);

			} catch(Genode::Xml_node::Nonexistent_attribute) {
				Genode::warning("missing attribute. Ignore node.");
			} catch(Gpio::Invalid_gpio_number) {
				Genode::warning("invalid GPIO number ", num, ", ignore node");
			}
			if (gpio_node.last("gpio")) break;
		}
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) {
		Genode::warning("no GPIO config");
	}
}

#endif /* _INCLUDE__GPIO__CONFIG_H_ */
