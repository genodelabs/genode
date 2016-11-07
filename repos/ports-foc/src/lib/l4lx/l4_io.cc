/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <io_mem_session/connection.h>

#include <env.h>

namespace Fiasco {
#include <l4/io/io.h>
}

using namespace Fiasco;

extern "C" {

	l4io_device_handle_t l4io_get_root_device(void)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	int l4io_iterate_devices(l4io_device_handle_t *devhandle,
	                         l4io_device_t *dev, l4io_resource_handle_t *reshandle)
	{
		Genode::warning(__func__, " not implemented");
		return 1;
	}


	int l4io_lookup_resource(l4io_device_handle_t devhandle,
	                         enum l4io_resource_types_t type,
	                         l4io_resource_handle_t *reshandle,
	                         l4io_resource_t *res)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	long l4io_request_ioport(unsigned portnum, unsigned len)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	long l4io_request_iomem_region(l4_addr_t phys, l4_addr_t virt,
	                               unsigned long size, int flags)
	{
		using namespace Genode;

		Io_mem_connection *iomem = new (env()->heap()) Io_mem_connection(phys, size);
		L4lx::Dataspace *ds =
			L4lx::Env::env()->dataspaces()->insert("iomem", iomem->dataspace());
		if (!L4lx::Env::env()->rm()->attach_at(ds, size, 0, (void*)virt)) {
			error("could not reserve IO mem region at ", Hex(virt));
			L4lx::Env::env()->dataspaces()->remove(ds);
			destroy(env()->heap(), iomem);
			return 1;
		}
		return 0;
	}


	long l4io_search_iomem_region(l4_addr_t phys, l4_addr_t size,
	                              l4_addr_t *rstart, l4_addr_t *rsize)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	long l4io_request_iomem(l4_addr_t phys, unsigned long size, int flags,
	                        l4_addr_t *virt)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	long l4io_release_iomem(l4_addr_t virt, unsigned long size)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	long l4io_request_irq(int irqnum, l4_cap_idx_t irqcap)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	long l4io_release_irq(int irqnum, l4_cap_idx_t irq_cap)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}


	int l4io_has_resource(enum l4io_resource_types_t type,
	                      l4vbus_paddr_t start, l4vbus_paddr_t end)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}

}
