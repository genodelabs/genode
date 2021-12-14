/*
 * \brief  platform session component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "pci_session_component.h"
#include "pci_bridge.h"


/* set during ACPI ROM parsing to valid value */
unsigned Platform::Bridge::root_bridge_bdf = INVALID_ROOT_BRIDGE;


static Genode::List<Platform::Bridge> *bridges()
{
	static Genode::List<Platform::Bridge> list;
	return &list;
}


unsigned short Platform::bridge_bdf(unsigned char bus)
{
	for (Platform::Bridge *bridge = bridges()->first(); bridge;
	     bridge = bridge->next())
	{
		if (bridge->part_of(bus))
			return bridge->bdf();
	}
	/* XXX Ideally, this case should never happen */
	return Platform::Bridge::root_bridge_bdf;
}

void Platform::Pci_buses::scan_bus(Config_access &config_access,
                                   Allocator &heap,
                                   Device_bars_pool &devices_bars,
                                   unsigned char bus)
{
	for (unsigned dev = 0; dev < Device_config::MAX_DEVICES; ++dev) {
		for (unsigned fun = 0; fun < Device_config::MAX_FUNCTIONS; ++fun) {

			Pci::Bdf const bdf { .bus = bus, .device = dev, .function = fun };

			/* read config space */
			Device_config config(bdf, &config_access);

			/* remember Device BARs required after power off and/or reset */
			if (config.valid()) {
				Device_config::Device_bars bars = config.save_bars();
				if (!bars.all_invalid())
					new (heap) Registered<Device_config::Device_bars>(devices_bars, bars);
			}

			/*
			 * Switch off PCI bus master DMA for some classes of devices,
			 * which caused trouble.
			 * Some devices are enabled by BIOS/UEFI or/and bootloaders and
			 * aren't switch off when handing over to the kernel and Genode.
			 * By disabling bus master DMA they should stop to issue DMA
			 * operations and IRQs. IRQs are problematic, if it is a shared
			 * IRQ - e.g. Ethernet and graphic card share a GSI IRQ. If the
			 * graphic card driver is started without a Ethernet driver,
			 * the graphic card may ack all IRQs. We may end up in a endless
			 * IRQ/ACK loop, since no Ethernet driver acknowledge/disable IRQ
			 * generation on the Ethernet device.
			 *
			 * Switching off PCI bus master DMA in general is a bad idea,
			 * since some device classes require a explicit handover protocol
			 * between BIOS/UEFI and device, e.g. USB. Violating such protocols
			 * lead to hard hangs on some machines.
			 */
			if (config.class_code() >> 8) {
				uint16_t classcode = config.class_code() >> 16;
				uint16_t subclass  = (config.class_code() >> 8) & 0xff;

				if ((classcode == 0x2 && subclass == 0x00) /* ETHERNET */) {
					config.disable_bus_master_dma(config_access);
				}
			}

			if (!config.valid())
				continue;

			/*
			 * There is at least one device on the current bus, so
			 * we mark it as valid.
			 */
			if (!_valid.get(bus, 1))
				_valid.set(bus, 1);

			/* scan behind bridge */
			if (config.pci_bridge()) {
				/* PCI bridge spec 3.2.5.3, 3.2.5.4 */
				unsigned char sec_bus = config.read(config_access, 0x19,
				                                    Device::ACCESS_8BIT);
				unsigned char sub_bus = config.read(config_access, 0x20,
				                                    Device::ACCESS_8BIT);

				bridges()->insert(new (heap) Bridge(bus, dev, fun, sec_bus,
				                                    sub_bus));

				uint16_t cmd = config.read(config_access,
				                           Device_config::PCI_CMD_REG,
				                           Platform::Device::ACCESS_16BIT);

				const bool enabled = (cmd & Device_config::PCI_CMD_MASK)
				                     == Device_config::PCI_CMD_MASK;

				if (!enabled) {
					config.write(config_access, Device_config::PCI_CMD_REG,
					             cmd | Device_config::PCI_CMD_MASK,
					             Platform::Device::ACCESS_16BIT);
				}

				log(config, " - bridge ",
				    Hex(sec_bus, Hex::Prefix::OMIT_PREFIX, Hex::Pad::PAD),
				    ":00.0", !enabled ? " enabled" : "");

				scan_bus(config_access, heap, devices_bars, sec_bus);
			}
		}
	}
}


using Platform::Session_component;

Genode::Bit_array<Session_component::MAX_PCI_DEVICES> Session_component::bdf_in_use;
