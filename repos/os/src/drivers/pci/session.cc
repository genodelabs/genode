/*
 * \brief  PCI-session component
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

/**
 * Check if given PCI bus was found on initial scan
 *
 * This tremendously speeds up further scans by other drivers.
 */
bool Pci::bus_valid(int bus) 
{
	struct Valid_buses
	{
		bool valid[Device_config::MAX_BUSES];

		void scan_bus(Config_access &config_access, int bus = 0)
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
					if (config.is_pci_bridge()) {
						int sub_bus = config.read(&config_access,
						                          0x19, Device::ACCESS_8BIT);
						scan_bus(config_access, sub_bus);
					}
				}
			}
		}

		Valid_buses() { Config_access c; scan_bus(c); }
	};

	static Valid_buses buses;

	return buses.valid[bus];
}
