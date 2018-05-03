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
                                   Genode::Allocator &heap, unsigned char bus)
{
	for (int dev = 0; dev < Device_config::MAX_DEVICES; ++dev) {
		for (int fun = 0; fun < Device_config::MAX_FUNCTIONS; ++fun) {

			/* read config space */
			Device_config config(bus, dev, fun, &config_access);

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

				enum {
					PCI_CMD_REG    = 0x4,
					PCI_CMD_MASK   = 0x7 /* IOPORT, MEM, DMA */
				};

				unsigned short cmd = config.read(config_access, PCI_CMD_REG,
			                                     Platform::Device::ACCESS_16BIT);

				if ((cmd & PCI_CMD_MASK) != PCI_CMD_MASK) {
					config.write(config_access, PCI_CMD_REG,
					             cmd | PCI_CMD_MASK,
					             Platform::Device::ACCESS_16BIT);
				}

				Genode::log(config, " - bridge ",
				            Hex(sec_bus, Hex::Prefix::OMIT_PREFIX, Hex::Pad::PAD),
				            ":00.0",
				            ((cmd & PCI_CMD_MASK) != PCI_CMD_MASK) ? " enabled"
				                                                   : "");

				scan_bus(config_access, heap, sec_bus);
			}
		}
	}
}


using Platform::Session_component;
using Genode::Bit_array;

Bit_array<Session_component::MAX_PCI_DEVICES> Session_component::bdf_in_use;
