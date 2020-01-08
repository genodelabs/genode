#ifndef _AHCI__SPEC__X86__PLATFORM_H_
#define _AHCI__SPEC__X86__PLATFORM_H_
/*
 * \brief  Driver for PCI-bus platforms
 * \author Sebastian Sumpf
 * \date   2020-01-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <irq_session/connection.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <util/reconstructible.h>

namespace Ahci {
	struct Data;
	using namespace Genode;
}

struct Ahci::Data
{
	enum Pci_config {
		CLASS_MASS_STORAGE = 0x10000u,
		SUBCLASS_AHCI      = 0x600u,
		CLASS_MASK         = 0xffff00u,
		AHCI_DEVICE        = CLASS_MASS_STORAGE | SUBCLASS_AHCI,
		AHCI_BASE_ID       = 0x5,  /* resource id of ahci base addr <bar 5> */
		PCI_CMD            = 0x4,
	};

	Genode::Env &env;

	Platform::Connection                   pci            { env };
	Platform::Device_capability            pci_device_cap { };
	Constructible<Platform::Device_client> pci_device     { };
	Constructible<Irq_session_client>      irq            { };
	Constructible<Attached_dataspace>      iomem          { };

	Data(Env &env);

	void _config_write(uint8_t op, uint16_t cmd,
	                   Platform::Device::Access_size width)
	{
		size_t donate = 4096;
		retry<Platform::Out_of_ram>(
			[&] () {
				retry<Platform::Out_of_caps>(
					[&] () { pci_device->config_write(op, cmd, width); },
					[&] () { pci.upgrade_caps(2); });
			},
			[&] () {
				pci.upgrade_ram(donate);
				donate *= 2;
			});
	}
};


#endif /* _AHCI__SPEC__X86__PLATFORM_H_ */
