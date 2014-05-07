/*
 * \brief  Gpio driver for the i.MX53
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-12-09
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <gpio/component.h>
#include <gpio/config.h>

/* local includes */
#include <driver.h>


int main(int, char **)
{
	using namespace Genode;

	printf("--- i.MX53 gpio driver ---\n");

	Imx53_driver &driver = Imx53_driver::factory();
	Gpio::process_config(driver);

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "gpio_ep");
	static Gpio::Root gpio_root(&ep, &sliced_heap, driver);

	/*
	 * Announce service
	 */
	env()->parent()->announce(ep.manage(&gpio_root));

	Genode::sleep_forever();
	return 0;
}

