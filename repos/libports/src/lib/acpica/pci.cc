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
#include <io_port_session/connection.h>

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

static void dump_read(char const * const func, ACPI_PCI_ID *pcidev,
                      UINT32 reg, UINT64 value, UINT32 width)
{
	using namespace Genode;

	log(func, ": ", Bdf(pcidev->Bus, pcidev->Device, pcidev->Function), " "
	    "reg=", Hex(reg, Hex::PREFIX, Hex::PAD), " "
	    "width=", width, width < 10 ? " " : "", " -> "
	    "value=", Genode::Hex(value));
}

static void dump_write(char const * const func, ACPI_PCI_ID *pcidev,
                       UINT32 reg, UINT64 value, UINT32 width)
{
	using namespace Genode;

	warning(func, ": ", Bdf(pcidev->Bus, pcidev->Device, pcidev->Function), " "
	        "reg=", Hex(reg, Hex::PREFIX, Hex::PAD), " "
	        "width=", width, width < 10 ? " " : "", " -> "
	        "value=", Genode::Hex(value));
}

static void dump_error(char const * const func, ACPI_PCI_ID *pcidev,
                       UINT32 reg, UINT32 width)
{
	error(func, " unknown device - segment=", pcidev->Segment, " ",
	      "bdf=",   Bdf(pcidev->Bus, pcidev->Device, pcidev->Function), " ",
	      "reg=",   Genode::Hex(reg), " "
	      "width=", Genode::Hex(width));
}


/*************************
 * Acpica PCI OS backend *
 *************************/

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
				Acpica::platform().release_device(client.rpc_cap());
				return AE_ERROR;
			};

			if (reg >= 0x100)
				Genode::warning(__func__, " ", Genode::Hex(reg),
				                " out of supported config space range ",
				                " -> wrong location will be read");

			*value = client.config_read(reg, access_size);

			dump_read(__func__, pcidev, reg, *value, width);

			Acpica::platform().release_device(client.rpc_cap());
			return AE_OK;
		}

		cap = Acpica::platform().next_device(cap);

		Acpica::platform().release_device(client.rpc_cap());
	}

	dump_error(__func__, pcidev, reg, width);

	*value = ~0U;
	return AE_OK;
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
				Acpica::platform().release_device(client.rpc_cap());
				return AE_ERROR;
			};

			client.config_write(reg, value, access_size);

			if (reg >= 0x100)
				Genode::warning(__func__, " ", Genode::Hex(reg),
				                " out of supported config space range ",
				                " -> wrong location will be written");

			dump_write(__func__, pcidev, reg, value, width);

			Acpica::platform().release_device(client.rpc_cap());
			return AE_OK;
		}

		cap = Acpica::platform().next_device(cap);

		Acpica::platform().release_device(client.rpc_cap());
	}

	dump_error(__func__, pcidev, reg, width);

	return AE_OK;
}
