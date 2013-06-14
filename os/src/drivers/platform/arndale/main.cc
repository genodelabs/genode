/*
 * \brief  Driver for Arndale specific platform devices (clocks, power, etc.)
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <regulator/component.h>
#include <regulator/consts.h>

#include <cmu.h>


struct Driver_factory : Regulator::Driver_factory
{
	Cmu _cmu;

	Regulator::Driver &create(Regulator::Regulator_id id) {
		switch (id) {
		case Regulator::CLK_CPU:
			return _cmu;
		default:
			throw Root::Invalid_args(); /* invalid regulator */
		};
	}

	void destroy(Regulator::Driver &driver) { }

};


int main(int, char **)
{
	using namespace Genode;

	PINF("--- Arndale platform driver ---\n");

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, 4096, "arndale_plat_ep");
	static ::Driver_factory driver_factory;
	static Regulator::Root reg_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&reg_root));

	sleep_forever();
	return 0;
}
