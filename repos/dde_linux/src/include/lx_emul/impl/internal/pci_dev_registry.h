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

#ifndef _LX_EMUL__IMPL__INTERNAL__PCI_DEV_REGISTRY_H_
#define _LX_EMUL__IMPL__INTERNAL__PCI_DEV_REGISTRY_H_

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/pci_dev.h>

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

		List<Pci_dev> _devs;

	public:

		void insert(Pci_dev *pci_dev)
		{
			PDBG("insert pci_dev %p", pci_dev);
			_devs.insert(pci_dev);
		}

		Genode::Io_mem_dataspace_capability io_mem(resource_size_t         phys,
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

			PERR("Device using i/o memory of address %zx is unknown", phys);
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

			PWRN("I/O port(%u) read failed", port);
			return (T)~0;
		}

		template <typename T>
		void io_write(unsigned port, T value)
		{
			/* try I/O access on all PCI devices, return on first success */
			for (Pci_dev *d = _devs.first(); d; d = d->next())
				if (d->io_port().out<T>(port, value))
					return;

			PWRN("I/O port(%u) write failed", port);
		}
};

#endif /* _LX_EMUL__IMPL__INTERNAL__PCI_DEV_REGISTRY_H_ */
