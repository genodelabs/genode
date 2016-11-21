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
#include <os/attached_rom_dataspace.h>

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
	REGISTER(DevicePCI);
	REGISTER(DevicePCIBridge);

	/* devices */
	REGISTER(DevicePS2KeyboardMouse);
	REGISTER(DeviceVga);
	REGISTER(DeviceFloppyController);
	REGISTER(DeviceSerialPort);
	REGISTER(DevicePIIX3IDE);
	REGISTER(DeviceAHCI);
	REGISTER(DevicePCNet);
	REGISTER(DeviceE1000);
	REGISTER(DeviceVMMDev);
	REGISTER(DeviceOHCI);
	REGISTER(DeviceICHAC97);
	REGISTER(DeviceHDA);

	return VINF_SUCCESS;
}

/*
 * The virtual PCI model delivers IRQs to the PIC by default and to the IOAPIC
 * only if the guest operating system selected the IOAPIC with the '_PIC' ACPI
 * method and if it called the '_PRT' ACPI method afterwards. When running a
 * guest operating system which uses the IOAPIC, but does not call these ACPI
 * methods (for example Genode/NOVA), IRQ delivery to the IOAPIC can be
 * enforced with the 'force_ioapic' configuration option.
 *
 * References:
 *   - 'pciSetIrqInternal()' in DevPCI.cpp
 *   - '_PIC' and '_PRT' ACPI methods in vbox.dsl
 */

static bool read_force_ioapic_from_config()
{
	try {
		Genode::Attached_rom_dataspace config("config");
		return config.xml().attribute_value("force_ioapic", false);
	} catch (Genode::Rom_connection::Rom_connection_failed) {
		return false;
	}
}

bool force_ioapic()
{
	/* read only once from config ROM */
	static bool force = read_force_ioapic_from_config();
	return force;
}
