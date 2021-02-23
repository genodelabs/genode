/*
 * \brief  VirtualBox device models
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

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
	REGISTER(DevicePCI);
	REGISTER(DevicePciIch9);
	REGISTER(DevicePcArch);
	REGISTER(DevicePcBios);
	REGISTER(DeviceIOAPIC);
	REGISTER(DevicePS2KeyboardMouse);
	REGISTER(DevicePIIX3IDE);
	REGISTER(DeviceI8254);
	REGISTER(DeviceI8259);
	REGISTER(DeviceHPET);
	REGISTER(DeviceSmc);
	REGISTER(DeviceFlash);
#ifdef VBOX_WITH_EFI
	REGISTER(DeviceEFI);
#endif
	REGISTER(DeviceMC146818);
	REGISTER(DeviceVga);
	REGISTER(DeviceVMMDev);
	REGISTER(DevicePCNet);
	REGISTER(DeviceE1000);
	REGISTER(DeviceICHAC97);
	REGISTER(DeviceHDA);
	REGISTER(DeviceOHCI);
	REGISTER(DeviceACPI);
	REGISTER(DeviceDMA);
	REGISTER(DeviceFloppyController);
	REGISTER(DeviceSerialPort);
	REGISTER(DeviceParallelPort);
	REGISTER(DeviceAHCI);
	REGISTER(DevicePCIBridge);
	REGISTER(DevicePciIch9Bridge);
	REGISTER(DeviceGIMDev);
	REGISTER(DeviceLPC);

	return VINF_SUCCESS;
}
