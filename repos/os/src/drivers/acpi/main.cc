/*
 * \brief  Service and session interface
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 */

 /*
  * Copyright (C) 2009-2015 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU General Public License version 2.
  */

#include <base/printf.h>
#include <base/sleep.h>

#include <os/reporter.h>

#include "acpi.h"

int main(int argc, char **argv)
{
	using namespace Genode;

	try {
		Acpi::generate_report();
	} catch (Genode::Xml_generator::Buffer_exceeded) {
		PERR("ACPI report too large - failure");
		throw;
	} catch (...) {
		PERR("Unknown exception occured - failure");
		throw;
	}

	Genode::sleep_forever();
	return 0;
}
