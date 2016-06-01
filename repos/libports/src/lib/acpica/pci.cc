/*
 * \brief  PCI specific backend for ACPICA library
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/env.h>
#include <parent/parent.h>
#include <platform_session/client.h>

extern "C" {
#include "acpi.h"
#include "acpiosxf.h"
}


static Platform::Client & platform()
{
	static bool connected = false;

	typedef Genode::Capability<Platform::Session> Platform_session_capability;
	Platform_session_capability platform_cap;

	if (!connected) {
		Genode::Parent::Service_name announce_for_acpica("Acpi");
		Genode::Native_capability cap = Genode::env()->parent()->session(announce_for_acpica, "ram_quota=20K");

		platform_cap = Genode::reinterpret_cap_cast<Platform::Session>(cap);
		connected = true;
	}

	static Platform::Client conn(platform_cap);
	return conn;
}

ACPI_STATUS AcpiOsInitialize (void)
{
	/* acpi_drv uses IOMEM concurrently to us - wait until it is done */
	PINF("wait for platform drv");
	try {
		platform();
	} catch (...) {
		PERR("did not get Platform connection");
		Genode::Lock lock(Genode::Lock::LOCKED);
		lock.lock();
	}
	PINF("wait for platform drv - done");
	return AE_OK;
}

ACPI_STATUS AcpiOsReadPciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                        UINT64 *value, UINT32 width)
{
	Platform::Device_capability cap = platform().first_device();

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
				PERR("%s : unsupported access size %u", __func__, width);
				platform().release_device(client);
				return AE_ERROR;
			};

			*value = client.config_read(reg, access_size);

			PINF("%s: %x:%x.%x reg=0x%x width=%u -> value=0x%llx",
			     __func__, bus, dev, fn, reg, width, *value);

			platform().release_device(client);
			return AE_OK;
		}

		cap = platform().next_device(cap);

		platform().release_device(client);
	}

	PERR("%s unknown device - segment=%u bdf=%x:%x.%x reg=0x%x width=0x%x",
	     __func__, pcidev->Segment, pcidev->Bus, pcidev->Device,
	     pcidev->Function, reg, width);

	return AE_ERROR;
}

ACPI_STATUS AcpiOsWritePciConfiguration (ACPI_PCI_ID *pcidev, UINT32 reg,
                                         UINT64 value, UINT32 width)
{
	Platform::Device_capability cap = platform().first_device();

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
				PERR("%s : unsupported access size %u", __func__, width);
				platform().release_device(client);
				return AE_ERROR;
			};

			client.config_write(reg, value, access_size);

			PWRN("%s: %x:%x.%x reg=0x%x width=%u value=0x%llx",
			     __func__, bus, dev, fn, reg, width, value);

			platform().release_device(client);
			return AE_OK;
		}

		cap = platform().next_device(cap);

		platform().release_device(client);
	}

	PERR("%s unknown device - segment=%u bdf=%x:%x.%x reg=0x%x width=0x%x",
	     __func__, pcidev->Segment, pcidev->Bus, pcidev->Device,
	     pcidev->Function, reg, width);

	return AE_ERROR;
}
