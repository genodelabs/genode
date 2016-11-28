/*
 * \brief  Service and session interface
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 */

 /*
  * Copyright (C) 2009-2016 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU General Public License version 2.
  */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <util/xml_generator.h>

/* local includes */
#include <acpi.h>


namespace Acpi {
	using namespace Genode;

	struct Main;
}

struct Acpi::Main
{
	Genode::Env  &env;
	Genode::Heap  heap { env.ram(), env.rm() };

	Main(Env &env) : env(env)
	{
		try {
			Acpi::generate_report(env, heap);
		} catch (Genode::Xml_generator::Buffer_exceeded) {
			Genode::error("ACPI report too large - failure");
			throw;
		} catch (...) {
			Genode::error("Unknown exception occured - failure");
			throw;
		}
	}
};


void Component::construct(Genode::Env &env) { static Acpi::Main main(env); }
