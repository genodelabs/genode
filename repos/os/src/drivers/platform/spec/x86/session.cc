/*
 * \brief  platform session component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "pci_session_component.h"
#include "pci_bridge.h"


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
	return 0;
}


/**
 * Check if given PCI bus was found on initial scan
 *
 * This tremendously speeds up further scans by other drivers.
 */
bool Platform::bus_valid(int bus) 
{
	struct Valid_buses
	{
		bool valid[Device_config::MAX_BUSES];

		void scan_bus(Config_access &config_access, unsigned char bus = 0)
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
					valid[bus] = true;

					/* scan behind bridge */
					if (config.pci_bridge()) {
						/* PCI bridge spec 3.2.5.3, 3.2.5.4 */
						unsigned char sec_bus = config.read(&config_access,
						                          0x19, Device::ACCESS_8BIT);
						unsigned char sub_bus = config.read(&config_access,
						                          0x20, Device::ACCESS_8BIT);

						bridges()->insert(new (Genode::env()->heap()) Bridge(bus, dev, fun, sec_bus, sub_bus));

						scan_bus(config_access, sec_bus);
					}
				}
			}
		}

		Valid_buses() { Config_access c; scan_bus(c); }
	};

	static Valid_buses buses;

	return buses.valid[bus];
}


using Platform::Session_component;
using Genode::Bit_array;

Bit_array<Session_component::MAX_PCI_DEVICES> Session_component::bdf_in_use;
