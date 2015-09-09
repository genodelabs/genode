/*
 * \brief  Backend allocator for DMA-capable memory
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__PCI_BACKEND_ALLOC_H_
#define _LX_EMUL__IMPL__INTERNAL__PCI_BACKEND_ALLOC_H_

/* Genode includes */
#include <base/object_pool.h>
#include <base/env.h>

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/slab_backend_alloc.h>

namespace Lx {

	struct Memory_object_base;
	struct Ram_object;
	struct Dma_object;

	extern Genode::Object_pool<Memory_object_base> memory_pool;
};


struct Lx::Memory_object_base : Genode::Object_pool<Memory_object_base>::Entry
{
	Memory_object_base(Genode::Ram_dataspace_capability cap)
	: Genode::Object_pool<Memory_object_base>::Entry(cap) {}
	virtual ~Memory_object_base() {};

	virtual void free() = 0;

	Genode::Ram_dataspace_capability ram_cap()
	{
		using namespace Genode;
		return reinterpret_cap_cast<Ram_dataspace>(cap());
	}
};


struct Lx::Ram_object : Memory_object_base
{
	Ram_object(Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap) {}

	void free() { Genode::env()->ram_session()->free(ram_cap()); }
};


struct Lx::Dma_object : Memory_object_base
{
	Dma_object(Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap) {}

	void free() { Lx::pci()->free_dma_buffer(ram_cap()); }
};


Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached)
{
	using namespace Genode;

	Memory_object_base *o;
	Genode::Ram_dataspace_capability cap;
	if (cached == CACHED) {
		cap = env()->ram_session()->alloc(size);
		o = new (env()->heap())	Ram_object(cap);
	} else {
		/* transfer quota to pci driver, otherwise it will give us a exception */
		char buf[32];
		Genode::snprintf(buf, sizeof(buf), "ram_quota=%ld", size);
		Genode::env()->parent()->upgrade(Lx::pci()->cap(), buf);

		cap = Lx::pci()->alloc_dma_buffer(size);
		o = new (env()->heap()) Dma_object(cap);
	}

	memory_pool.insert(o);
	return cap;
}


void Lx::backend_free(Genode::Ram_dataspace_capability cap)
{
	using namespace Genode;

	Memory_object_base *o = memory_pool.lookup_and_lock(cap);
	if (!o)
		return;

	o->free();
	memory_pool.remove_locked(o);
	destroy(env()->heap(), o);
}


#endif /* _LX_EMUL__IMPL__INTERNAL__PCI_BACKEND_ALLOC_H_ */

