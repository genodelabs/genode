/*
 * \brief  Driver for Odroid-x2 specific platform devices (clocks, power, etc.)
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopez Leon <humberto@uclv.cu>
 * \author Reinier Millo Sanchez <rmillo@uclv.cu>
 * \date   2015-04-30
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
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
#include <pmu.h>

struct Driver_factory: Regulator::Driver_factory
{

	Cmu _cmu;

	Regulator::Driver &create(Regulator::Regulator_id id)
	{
		switch (id) {
		case Regulator::CLK_CPU:
			return _cmu;
		default:
			throw Root::Invalid_args(); /* invalid regulator */
		};
	}

	void destroy(Regulator::Driver &driver) {
	}

};

int main(int, char **)
{
	using namespace Genode;

	PINF("--- Odroid-x2 platform driver ---\n");

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, 4096, "odroid_x2_plat_ep");
	static ::Driver_factory driver_factory;
	static Regulator::Root reg_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&reg_root));

	sleep_forever();
	return 0;
}
