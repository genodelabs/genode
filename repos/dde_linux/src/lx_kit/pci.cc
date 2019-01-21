/*
 * \brief  Backend allocator for DMA-capable memory
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
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
	Genode::Ram_allocator &_ram;

	Ram_object(Genode::Ram_allocator &ram,
	           Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap), _ram(ram) {}

	void free() { _ram.free(ram_cap()); }
};


struct Lx_kit::Dma_object : Memory_object_base
{
	Platform::Connection &_pci;

	Dma_object(Platform::Connection &pci,
	           Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap), _pci(pci) {}

	void free() { _pci.free_dma_buffer(ram_cap()); }
};


/********************
 ** Pci singletons **
 ********************/

static Genode::Constructible<Platform::Connection> _global_pci;
static Genode::Allocator     *_global_md_alloc;
static Genode::Ram_allocator *_global_ram;


void Lx::pci_init(Genode::Env &env, Genode::Ram_allocator &ram,
                  Genode::Allocator &md_alloc)
{
	_global_pci.construct(env);
	_global_ram      = &ram;
	_global_md_alloc = &md_alloc;

	Lx::pci_dev_registry(&env);
}


Platform::Connection *Lx::pci()
{
	return &*_global_pci;
}


Lx::Pci_dev_registry *Lx::pci_dev_registry(Genode::Env *env)
{
	static Lx::Pci_dev_registry _pci_dev_registry(*env);
	return &_pci_dev_registry;
}


/*********************************
 ** Lx::Backend_alloc interface **
 *********************************/

Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached)
{
	using namespace Genode;
	using namespace Lx_kit;

	Memory_object_base *obj;
	Genode::Ram_dataspace_capability cap;
	if (cached == CACHED) {
		cap = _global_ram->alloc(size);
		obj = new (_global_md_alloc) Ram_object(*_global_ram, cap);
	} else {
		Genode::size_t donate = size;
		cap = retry<Genode::Out_of_ram>(
			[&] () {
				return retry<Genode::Out_of_caps>(
					[&] () { return _global_pci->alloc_dma_buffer(size); },
					[&] () { _global_pci->upgrade_caps(2); });
			},
			[&] () {
				_global_pci->upgrade_ram(donate);
				donate = donate * 2 > size ? 4096 : donate * 2;
			});

		obj = new (_global_md_alloc) Dma_object(*_global_pci, cap);
	}

	memory_pool.insert(obj);
	return cap;
}


void Lx::backend_free(Genode::Ram_dataspace_capability cap)
{
	using namespace Genode;
	using namespace Lx_kit;

	Memory_object_base *object;
	memory_pool.apply(cap, [&] (Memory_object_base *obj) {
		object = obj;
		if (!object) { return; }

		object->free();
		memory_pool.remove(object);
	});
	destroy(_global_md_alloc, object);
}
