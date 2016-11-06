/*
 * \brief  Backend allocator for DMA-capable memory
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/object_pool.h>
#include <base/env.h>

/* XXX only because of struct pci_dev */
#include <lx_emul.h>

/* Linux emulation environment includes */
#include <lx_kit/backend_alloc.h>
#include <lx_kit/pci_dev_registry.h>


namespace Lx_kit {

	struct Memory_object_base;
	struct Ram_object;
	struct Dma_object;

	Genode::Object_pool<Memory_object_base> memory_pool;
};


struct Lx_kit::Memory_object_base : Genode::Object_pool<Memory_object_base>::Entry
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


struct Lx_kit::Ram_object : Memory_object_base
{
	Ram_object(Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap) {}

	void free() { Genode::env()->ram_session()->free(ram_cap()); }
};


struct Lx_kit::Dma_object : Memory_object_base
{
	Dma_object(Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap) {}

	void free() { Lx::pci()->free_dma_buffer(ram_cap()); }
};


/*********************************
 ** Lx::Backend_alloc interface **
 *********************************/

Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached)
{
	using namespace Genode;
	using namespace Lx_kit;

	Memory_object_base *o;
	Genode::Ram_dataspace_capability cap;
	if (cached == CACHED) {
		cap = env()->ram_session()->alloc(size);
		o = new (env()->heap())	Ram_object(cap);
	} else {
		Genode::size_t donate = size;
		cap = retry<Platform::Session::Out_of_metadata>(
			[&] () { return Lx::pci()->alloc_dma_buffer(size); },
			[&] () {
				Lx::pci()->upgrade_ram(donate);
				donate = donate * 2 > size ? 4096 : donate * 2;
			});

		o = new (env()->heap()) Dma_object(cap);
	}

	memory_pool.insert(o);
	return cap;
}


void Lx::backend_free(Genode::Ram_dataspace_capability cap)
{
	using namespace Genode;
	using namespace Lx_kit;

	Memory_object_base *object;
	memory_pool.apply(cap, [&] (Memory_object_base *o) {
		object = o;
		if (!object)
			return;

		object->free();
		memory_pool.remove(object);
	});
	destroy(env()->heap(), object);
}


/********************
 ** Pci singletons **
 ********************/

Platform::Connection *Lx::pci()
{
	static Platform::Connection _pci;
	return &_pci;
}


Lx::Pci_dev_registry *Lx::pci_dev_registry()
{
	static Lx::Pci_dev_registry _pci_dev_registry;
	return &_pci_dev_registry;
}
