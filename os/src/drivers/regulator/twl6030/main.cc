/*
 * \brief  Driver for Twl6030 Voltage Regulator
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2013-02-18
 */

/*
 * Copyright (C) 2013 Ksys Labs LLC
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <os/config.h>
#include <cap_session/connection.h>
#include <os/attached_io_mem_dataspace.h>

#include <i2c_session/connection.h>

#include "../regulator_component.h"
#include "../regulator_driver.h"

#include "driver.h"

int main(int argc, char **argv)
{
	using namespace Genode;

	PINF("--- Twl6030 driver started ---\n");

	static I2C::Connection i2c;
	Regulator::Twl6030_driver_factory driver_factory(i2c);

	enum { STACK_SIZE = 0x2000 };
	static Cap_connection cap;

	static Rpc_entrypoint ep(&cap, STACK_SIZE, "twl6030_ep");

	static Regulator::Root regulator_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&regulator_root));

	sleep_forever();
	return 0;
}
