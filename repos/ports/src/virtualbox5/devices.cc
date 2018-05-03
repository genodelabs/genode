/*
 * \brief  VirtualBox device models
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*
 * VirtualBox defines a 'Log' macro, which would interfere with 'Genode::Log'
 * if we didn't include the header here
 */
#include <base/log.h>

/* VirtualBox includes */
#include <VBoxDD.h>
#include <VBoxDD2.h>

#include "vmm.h"

#define REGISTER(device)                                       \
	do {                                                       \
		rc = pCallbacks->pfnRegister(pCallbacks, &g_##device); \
		if (RT_FAILURE(rc))                                    \
			return rc;                                         \
	} while (0)


extern "C" int VBoxDevicesRegister(PPDMDEVREGCB pCallbacks, uint32_t u32Version)
{
	int rc = 0;

	/* platform */
	REGISTER(DevicePCI);
	REGISTER(DevicePciIch9);
	REGISTER(DevicePcArch);
	REGISTER(DevicePcBios);
	REGISTER(DeviceIOAPIC);
#ifndef VBOX_WITH_NEW_APIC
	REGISTER(DeviceAPIC);
#endif
	REGISTER(DevicePS2KeyboardMouse);
	REGISTER(DevicePIIX3IDE);
	REGISTER(DeviceI8254);
	REGISTER(DeviceI8259);
	REGISTER(DeviceHPET);
	REGISTER(DeviceSmc);
	REGISTER(DeviceMC146818);
	REGISTER(DeviceVga);
	REGISTER(DeviceVMMDev);
	REGISTER(DevicePCNet);
#ifdef VBOX_WITH_E1000
	REGISTER(DeviceE1000);
#endif
	REGISTER(DeviceICHAC97);
	REGISTER(DeviceICH6_HDA);
	REGISTER(DeviceOHCI);
	REGISTER(DeviceACPI);
	REGISTER(DeviceDMA);
	REGISTER(DeviceFloppyController);
	REGISTER(DeviceSerialPort);
#ifdef VBOX_WITH_AHCI
	REGISTER(DeviceAHCI);
#endif
	REGISTER(DevicePCIBridge);
	REGISTER(DevicePciIch9Bridge);
	REGISTER(DeviceLPC);

	REGISTER(DeviceXHCI);

	return VINF_SUCCESS;
}
