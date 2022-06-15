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


/*******************************
 * Accessing PCI via I/O ports *
 *******************************/

enum { REG_ADDR = 0xcf8, REG_DATA = 0xcfc, REG_SIZE = 4 };

static Genode::Io_port_connection &pci_io_port() {
	static Genode::Io_port_connection conn(Acpica::env(), REG_ADDR, REG_SIZE);
	return conn;
}

static unsigned pci_io_cfg_addr(unsigned const bus, unsigned const device,
                                unsigned const function, unsigned const addr)
{
	return (1U << 31) |
	       (bus << 16) |
	       ((device   & 0x1fU) << 11) |
	       ((function & 0x07U) << 8) |
	       (addr & ~3U);
}

static unsigned pci_io_read(unsigned const bus, unsigned const device,
                            unsigned const function, unsigned const addr,
                            unsigned const width)
{
	/* write target address */
	pci_io_port().outl(REG_ADDR, pci_io_cfg_addr(bus, device, function, addr));

	switch (width) {
	case 8:
		return pci_io_port().inb(REG_DATA + (addr & 3));
	case 16:
		return pci_io_port().inw(REG_DATA + (addr & 2));
	case 32:
		return pci_io_port().inl(REG_DATA);
	default:
		return ~0U;
	}
}

static void pci_io_write(unsigned const bus, unsigned const device,
                         unsigned const function, unsigned const addr,
                         unsigned const width, unsigned value)
{
	/* write target address */
	pci_io_port().outl(REG_ADDR, pci_io_cfg_addr(bus, device, function, addr));

	switch (width) {
	case 8:
		pci_io_port().outb(REG_DATA + (addr & 3), value);
		return;
	case 16:
		pci_io_port().outw(REG_DATA + (addr & 2), value);
		return;
	case 32:
		pci_io_port().outl(REG_DATA, value);
		return;
	}
}

/*************************
 * Acpica PCI OS backend *
 *************************/

ACPI_STATUS AcpiOsInitialize (void) { return AE_OK; }

ACPI_STATUS AcpiOsReadPciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                        UINT64 *value, UINT32 width)
{
	if (!Acpica::platform_drv()) {
		try {
			*value = pci_io_read(pcidev->Bus, pcidev->Device, pcidev->Function,
			                     reg, width);
			dump_read(__func__, pcidev, reg, *value, width);
		} catch (...) {
			dump_error(__func__, pcidev, reg, width);
			return AE_ERROR;
		}
		return AE_OK;
	}

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

			*value = client.config_read(reg, access_size);

			dump_read(__func__, pcidev, reg, *value, width);

			Acpica::platform().release_device(client.rpc_cap());
			return AE_OK;
		}

		cap = Acpica::platform().next_device(cap);

		Acpica::platform().release_device(client.rpc_cap());
	}

	dump_error(__func__, pcidev, reg, width);

	return AE_ERROR;
}

ACPI_STATUS AcpiOsWritePciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                         UINT64 value, UINT32 width)
{
	if (!Acpica::platform_drv()) {
		try {
			dump_write(__func__, pcidev, reg, value, width);
			pci_io_write(pcidev->Bus, pcidev->Device, pcidev->Function, reg,
			             width, value);
			return AE_OK;
		} catch (...) {
			dump_error(__func__, pcidev, reg, width);
			return AE_ERROR;
		}
	}

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

			dump_write(__func__, pcidev, reg, value, width);

			Acpica::platform().release_device(client.rpc_cap());
			return AE_OK;
		}

		cap = Acpica::platform().next_device(cap);

		Acpica::platform().release_device(client.rpc_cap());
	}

	dump_error(__func__, pcidev, reg, width);

	return AE_ERROR;
}
