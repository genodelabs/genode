/*
 * \brief  VirtualBox device models
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* VirtualBox includes */
#include <VBoxDD.h>
#include <VBoxDD2.h>


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
	REGISTER(DevicePcArch);
	REGISTER(DevicePcBios);
	REGISTER(DeviceI8254);
	REGISTER(DeviceI8259);
	REGISTER(DeviceDMA);
	REGISTER(DeviceMC146818);
	REGISTER(DeviceACPI);
	REGISTER(DeviceAPIC);
	REGISTER(DeviceIOAPIC);
	REGISTER(DevicePCI);
	REGISTER(DevicePCIBridge);

	/* devices */
	REGISTER(DevicePS2KeyboardMouse);
	REGISTER(DeviceVga);
	REGISTER(DeviceFloppyController);
	REGISTER(DeviceSerialPort);
	REGISTER(DevicePIIX3IDE);
	REGISTER(DevicePCNet);
	REGISTER(DeviceE1000);
	REGISTER(DeviceVMMDev);
	REGISTER(DeviceOHCI);

	return VINF_SUCCESS;
}

