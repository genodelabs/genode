/*
 * \brief  PCI specific backend for ACPICA library
 * \author Alexander Boettcher
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
		Genode::print(out, Hex(bus, Hex::OMIT_PREFIX), ":",
		                   Hex(dev, Hex::OMIT_PREFIX), ".",
		                   Hex(fn,  Hex::OMIT_PREFIX), " ");
	}
};


ACPI_STATUS AcpiOsInitialize (void) { return AE_OK; }

ACPI_STATUS AcpiOsReadPciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                        UINT64 *value, UINT32 width)
{
	Platform::Device_capability cap = Acpica::platform().first_device();

	while (cap.valid()) {
		Platform::Device_client client(cap);

		unsigned char bus, dev, fn;
		client.bus_address(&bus, &dev, &fn);

		if (pcidev->Bus == bus && pcidev->Device == dev &&
		    pcidev->Function == fn) {

			Platform::Device_client::Access_size access_size;
			switch (width) {
			case 8:
				access_size = Platform::Device_client::Access_size::ACCESS_8BIT;
				break;
			case 16:
				access_size = Platform::Device_client::Access_size::ACCESS_16BIT;
				break;
			case 32:
				access_size = Platform::Device_client::Access_size::ACCESS_32BIT;
				break;
			default:
				Genode::error(__func__, " : unsupported access size ", width);
				Acpica::platform().release_device(client);
				return AE_ERROR;
			};

			*value = client.config_read(reg, access_size);

			Genode::log(__func__, ": ", Bdf(bus, dev, fn),
			            "reg=",   Genode::Hex(reg), " "
			            "width=", width, " -> "
			            "value=", Genode::Hex(*value));

			Acpica::platform().release_device(client);
			return AE_OK;
		}

		cap = Acpica::platform().next_device(cap);

		Acpica::platform().release_device(client);
	}

	Genode::error(__func__, " unknown device - segment=", pcidev->Segment, " "
	              "bdf=", Bdf(pcidev->Bus, pcidev->Device, pcidev->Function), " "
	              "reg=", Genode::Hex(reg), " "
	              "width=", Genode::Hex(width));

	return AE_ERROR;
}

ACPI_STATUS AcpiOsWritePciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                         UINT64 value, UINT32 width)
{
	Platform::Device_capability cap = Acpica::platform().first_device();

	while (cap.valid()) {
		Platform::Device_client client(cap);

		unsigned char bus, dev, fn;
		client.bus_address(&bus, &dev, &fn);

		if (pcidev->Bus == bus && pcidev->Device == dev &&
		    pcidev->Function == fn) {

			Platform::Device_client::Access_size access_size;
			switch (width) {
			case 8:
				access_size = Platform::Device_client::Access_size::ACCESS_8BIT;
				break;
			case 16:
				access_size = Platform::Device_client::Access_size::ACCESS_16BIT;
				break;
			case 32:
				access_size = Platform::Device_client::Access_size::ACCESS_32BIT;
				break;
			default:
				Genode::error(__func__, " : unsupported access size ", width);
				Acpica::platform().release_device(client);
				return AE_ERROR;
			};

			client.config_write(reg, value, access_size);

			Genode::warning(__func__, ": ", Bdf(bus, dev, fn), " "
			                "reg=",   Genode::Hex(reg), " "
			                "width=", width, " "
			                "value=", Genode::Hex(value));

			Acpica::platform().release_device(client);
			return AE_OK;
		}

		cap = Acpica::platform().next_device(cap);

		Acpica::platform().release_device(client);
	}

	Genode::error(__func__, " unknown device - segment=", pcidev->Segment, " ",
	              "bdf=",   Bdf(pcidev->Bus, pcidev->Device, pcidev->Function), " ",
	              "reg=",   Genode::Hex(reg), " "
	              "width=", Genode::Hex(width));

	return AE_ERROR;
}
