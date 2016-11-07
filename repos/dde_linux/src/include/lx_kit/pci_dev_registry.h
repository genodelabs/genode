/*
 * \brief  Registry of PCI devices
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__PCI_DEV_REGISTRY_H_
#define _LX_KIT__PCI_DEV_REGISTRY_H_

/* Linux emulation environment includes */
#include <lx_kit/internal/pci_dev.h>

namespace Lx {
	
	class Pci_dev_registry;

	/**
	 * Return singleton 'Pci_dev_registry' object
	 *
	 * Implementation must be provided by the driver.
	 */
	Pci_dev_registry *pci_dev_registry();
}


class Lx::Pci_dev_registry
{
	private:

		Lx_kit::List<Pci_dev> _devs;

	public:

		void insert(Pci_dev *pci_dev)
		{
			_devs.insert(pci_dev);
		}

		Pci_dev* first() { return _devs.first(); }

		Genode::Io_mem_dataspace_capability io_mem(Genode::addr_t          phys,
		                                           Genode::Cache_attribute cache_attribute,
		                                           Genode::size_t          size,
		                                           Genode::addr_t         &offset)
		{
			enum { PCI_ROM_RESOURCE = 6 };

			for (Pci_dev *d = _devs.first(); d; d = d->next()) {

				unsigned bar = 0;
				for (; bar < PCI_ROM_RESOURCE; bar++) {
					if ((pci_resource_flags(d, bar) & IORESOURCE_MEM) &&
					    (pci_resource_start(d, bar) <= phys) &&
					    (pci_resource_end(d, bar) >= (phys+size-1)))
						break;
				}
				if (bar >= PCI_ROM_RESOURCE)
					continue;

				/* offset from the beginning of the PCI resource */
				offset = phys - pci_resource_start(d, bar);

				Genode::Io_mem_session_capability io_mem_cap =
					d->io_mem(bar, cache_attribute);

				return Genode::Io_mem_session_client(io_mem_cap).dataspace();
			}

			Genode::error("device using I/O memory of address ",
			              Genode::Hex(phys), " is unknown");
			return Genode::Io_mem_dataspace_capability();
		}

		template <typename T>
		T io_read(unsigned port)
		{
			/* try I/O access on all PCI devices */
			for (Pci_dev *d = _devs.first(); d; d = d->next()) {
				T value = 0;
				if (d->io_port().in<T>(port, &value))
					return value;
			}

			Genode::warning("I/O port(", port, ") read failed");
			return (T)~0;
		}

		template <typename T>
		void io_write(unsigned port, T value)
		{
			/* try I/O access on all PCI devices, return on first success */
			for (Pci_dev *d = _devs.first(); d; d = d->next())
				if (d->io_port().out<T>(port, value))
					return;

			Genode::warning("I/O port(", port, ") write failed");
		}
};

#endif /* _LX_KIT__PCI_DEV_REGISTRY_H_ */
