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


static bool cpu_name(char const * name)
{
	unsigned cpuid = 0, edx = 0, ebx = 0, ecx = 0;
	asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx), "=b"(ebx), "=c"(ecx));

	return ebx == *reinterpret_cast<unsigned const *>(name + 0) &&
	       edx == *reinterpret_cast<unsigned const *>(name + 4) &&
	       ecx == *reinterpret_cast<unsigned const *>(name + 8);
}


/*************************
 * Acpica PCI OS backend *
 *************************/

ACPI_STATUS AcpiOsInitialize (void) { return AE_OK; }

ACPI_STATUS AcpiOsReadPciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                        UINT64 *value, UINT32 width)
{
	using namespace Genode;

	Bdf bdf(pcidev->Bus, pcidev->Device, pcidev->Function);

	bool const intel   = cpu_name("GenuineIntel");
	bool const emulate = intel &&
	                     !pcidev->Bus && !pcidev->Device && !pcidev->Function;

	/*
	 * ACPI quirk for 12th Gen Framework laptop and Thinkpad X1 Nano Gen2
	 *
	 * XXX emulate some of the register accesses to the Intel root bridge to
	 *     avoid bogus calculation of physical addresses. The value seems to
	 *     be close to the pci config start address as provided by mcfg table
	 *     for those machines.
	 */
	if (emulate) {
		if (reg == 0x60 && width == 32) {
			*value = 0xe0000001;
			warning(bdf, " emulate read ", Hex(reg), " -> ", Hex(*value));
			return AE_OK;
		}
	}

	/* during startup suppress errors */
	if (!(AcpiDbgLevel & ACPI_LV_INIT))
		error(__func__, " ", bdf, " ", Hex(reg), " width=", width);

	*value = ~0U;
	return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                         UINT64 value, UINT32 width)
{
	using namespace Genode;

	Bdf bdf(pcidev->Bus, pcidev->Device, pcidev->Function);

	/* during startup suppress errors */
	if (!(AcpiDbgLevel & ACPI_LV_INIT))
		error(__func__, " ", bdf, " ", Hex(reg), "=", Hex(value), " width=", width);

	return AE_OK;
}
