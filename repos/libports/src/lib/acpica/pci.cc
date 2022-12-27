/*
 * \brief  PCI specific backend for ACPICA library
 * \author Alexander Boettcher
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

#include "env.h"

extern "C" {
#include "acpi.h"
#include "acpiosxf.h"
}


/**
 * Utility for the formatted output of a (bus, device, function) triple
 */
struct Bdf
{
	unsigned char const bus, dev, fn;

	Bdf(unsigned char bus, unsigned char dev, unsigned char fn)
	: bus(bus), dev(dev), fn(fn) { }

	void print(Genode::Output &out) const
	{
		using Genode::Hex;
		Genode::print(out, Hex(bus, Hex::OMIT_PREFIX, Hex::PAD), ":",
		                   Hex(dev, Hex::OMIT_PREFIX, Hex::PAD), ".",
		                   Hex(fn,  Hex::OMIT_PREFIX), " ");
	}
};


/*************************
 * Acpica PCI OS backend *
 *************************/

ACPI_STATUS AcpiOsInitialize (void) { return AE_OK; }

ACPI_STATUS AcpiOsReadPciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                        UINT64 *value, UINT32 width)
{
	using namespace Genode;

	Bdf bdf(pcidev->Bus, pcidev->Device, pcidev->Function);

	error(__func__, " ", bdf, " ", Hex(reg), " width=", width);

	*value = ~0U;
	return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                         UINT64 value, UINT32 width)
{
	using namespace Genode;

	Bdf bdf(pcidev->Bus, pcidev->Device, pcidev->Function);

	error(__func__, " ", bdf, " ", Hex(reg), "=", Hex(value), " width=", width);

	return AE_OK;
}
