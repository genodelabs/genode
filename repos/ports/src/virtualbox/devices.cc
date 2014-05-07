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


extern "C" int VBoxDevicesRegister(PPDMDEVREGCB pCallbacks, uint32_t u32Version)
{
	PDBG("VBoxDevicesRegister called");

	int rc = 0;

	/* pcarch */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcArch);
	if (RT_FAILURE(rc))
		return rc;

	/* pcbios */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcBios);
	if (RT_FAILURE(rc))
		return rc;

	/* pci */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCI);
	if (RT_FAILURE(rc))
		return rc;

	/* pckbd */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePS2KeyboardMouse);
	if (RT_FAILURE(rc))
		return rc;

	/* i8254 */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8254);
	if (RT_FAILURE(rc))
		return rc;

	/* i8259 */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8259);
	if (RT_FAILURE(rc))
		return rc;

	/* mc146818 */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceMC146818);
	if (RT_FAILURE(rc))
		return rc;

	/* vga */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVga);
	if (RT_FAILURE(rc))
		return rc;

	/* piix3ide */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePIIX3IDE);
	if (RT_FAILURE(rc))
		return rc;

	/* 8237A DMA */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceDMA);
	if (RT_FAILURE(rc))
		return rc;

 	/* Guest - VMM/Host communication */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVMMDev);
	if (RT_FAILURE(rc))
		return rc;

	/* ACPI missing */

 	/* APIC */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceAPIC);
	if (RT_FAILURE(rc))
		return rc;

	/* i82078 */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceFloppyController);
	if (RT_FAILURE(rc))
		return rc;

	/* Ethernet PCNet controller */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCNet);
	if (RT_FAILURE(rc))
		return rc;

	/* Serial device */
	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSerialPort);
	if (RT_FAILURE(rc))
		return rc;

	rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCIBridge);
	if (RT_FAILURE(rc))
		return rc;

	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceACPI);
	if (RT_FAILURE(rc))
		return rc;

	rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceIOAPIC);
	if (RT_FAILURE(rc))
		return rc;

	return VINF_SUCCESS;
}

