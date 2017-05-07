/*
 * \brief  Driver for Arndale specific platform devices (clocks, power, etc.)
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/heap.h>
#include <base/component.h>
#include <regulator/component.h>
#include <regulator/consts.h>

#include <cmu.h>
#include <pmu.h>


struct Driver_factory : Regulator::Driver_factory
{
	Cmu _cmu;
	Pmu _pmu;

	Driver_factory(Genode::Env &env) : _cmu(env), _pmu(env) { }

	Regulator::Driver &create(Regulator::Regulator_id id) {
		switch (id) {
		case Regulator::CLK_CPU:
		case Regulator::CLK_SATA:
		case Regulator::CLK_USB30:
		case Regulator::CLK_USB20:
		case Regulator::CLK_MMC0:
		case Regulator::CLK_HDMI:
			return _cmu;
		case Regulator::PWR_SATA:
		case Regulator::PWR_USB30:
		case Regulator::PWR_USB20:
		case Regulator::PWR_HDMI:
			return _pmu;
		default:
			throw Genode::Service_denied(); /* invalid regulator */
		};
	}

	void destroy(Regulator::Driver &driver) { }
};


struct Main
{
	Genode::Env &    env;
	Genode::Heap     heap { env.ram(), env.rm() };
	::Driver_factory factory { env };
	Regulator::Root  root { env, heap, factory };

	Main(Genode::Env & env) : env(env) {
		env.parent().announce(env.ep().manage(root)); }
};


void Component::construct(Genode::Env &env)
{
	Genode::log("--- Arndale platform driver ---");

	static Main main(env);
}
