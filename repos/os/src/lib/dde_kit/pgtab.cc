/*
 * \brief   Virtual page-table facility
 * \author  Christian Helmuth
 * \date    2008-08-15
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/stdint.h>
#include <base/env.h>
#include <base/sync_allocator.h>
#include <base/allocator_avl.h>
#include <base/printf.h>

extern "C" {
#include <dde_kit/pgtab.h>
}

using namespace Genode;

class Mem_region
{
	private:

		addr_t _base;         /* region base address */
		size_t _size;         /* region size */
		addr_t _mapped_base;  /* base address in other address space */

	public:

		Mem_region() : _base(0), _size(0), _mapped_base(0) { }

		Mem_region(addr_t base, size_t size, addr_t mapped_base)
		: _base(base), _size(size), _mapped_base(mapped_base) { }


		/***************
		 ** Accessors **
		 ***************/

		addr_t base() const        { return _base; }
		size_t size() const        { return _size; }
		addr_t mapped_base() const { return _mapped_base; }
};

typedef Allocator_avl_tpl<Mem_region> Mem_region_allocator;

class Mem_map : public Synchronized_range_allocator<Mem_region_allocator>
{
	public:

		Mem_map()
		: Synchronized_range_allocator<Mem_region_allocator>(env()->heap()) {
			add_range(0, ~0); }


		/***********************************
		 ** Wrapped Allocator_avl methods **
		 ***********************************/

		/**
		 * Assign custom meta data to block at specified address
		 */
		void metadata(void *addr, Mem_region region)
		{
			Lock::Guard lock_guard(*lock());

			raw()->metadata(addr, region);
		}

		/**
		 * Return meta data that was attached to block at specified address
		 */
		Mem_region * metadata(void *addr)
		{
			Lock::Guard lock_guard(*lock());

			return raw()->metadata(addr);
		}
};


static Mem_map * phys_to_virt_map()
{
	static Mem_map _phys_to_virt_map;
	return &_phys_to_virt_map;
}


static Mem_map * virt_to_phys_map()
{
	static Mem_map _virt_to_phys_map;
	return &_virt_to_phys_map;
}


extern "C" void dde_kit_pgtab_set_region(void *virt, dde_kit_addr_t phys, unsigned pages)
{
	dde_kit_pgtab_set_region_with_size(virt, phys, pages << DDE_KIT_PAGE_SHIFT);
}


extern "C" void dde_kit_pgtab_set_region_with_size(void *virt, dde_kit_addr_t phys,
                                                   dde_kit_size_t size)
{
	Mem_map   *map;
	Mem_region region;

	/* add region to virtual memory map */
	map    = virt_to_phys_map();
	region = Mem_region(reinterpret_cast<addr_t>(virt), size, phys);

	if (map->alloc_addr(size, reinterpret_cast<addr_t>(virt)).is_ok())
		map->metadata(virt, region);
	else
		PWRN("virt->phys mapping for [%lx,%lx) failed",
		     reinterpret_cast<addr_t>(virt), reinterpret_cast<addr_t>(virt) + size);

	/* add region to physical memory map for reverse lookup */
	map    = phys_to_virt_map();
	region = Mem_region(phys, size, reinterpret_cast<addr_t>(virt));

	if (map->alloc_addr(size, phys).is_ok())
		map->metadata(reinterpret_cast<void *>(phys), region);
	else
		PWRN("phys->virt mapping for [%lx,%lx) failed", phys, phys + size);
}


extern "C" void dde_kit_pgtab_clear_region(void *virt)
{
	Mem_region *region =virt_to_phys_map()->metadata(virt);

	if (!region) {
		PWRN("no virt->phys mapping @ %p", virt);
		return;
	}

	void *phys = reinterpret_cast<void *>(region->mapped_base());

	/* remove region from both maps */
	virt_to_phys_map()->free(virt);
	phys_to_virt_map()->free(phys);
}


extern "C" dde_kit_addr_t dde_kit_pgtab_get_physaddr(void *virt)
{
	addr_t      phys;
	Mem_region *region = virt_to_phys_map()->metadata(virt);

	if (!region) {
		PWRN("no virt->phys mapping @ %p", virt);
		return 0;
	}

	phys = reinterpret_cast<addr_t>(virt) - region->base() + region->mapped_base();

	return phys;
}


extern "C" dde_kit_addr_t dde_kit_pgtab_get_virtaddr(dde_kit_addr_t phys)
{
	addr_t      virt;
	Mem_region *region = phys_to_virt_map()->metadata(reinterpret_cast<void *>(phys));

	if (!region) {
		PWRN("no phys->virt mapping @ %p", reinterpret_cast<void *>(phys));
		return 0;
	}

	virt = phys - region->base() + region->mapped_base();

	return virt;
}


extern "C" dde_kit_size_t dde_kit_pgtab_get_size(void *virt)
{
	Mem_region *region =virt_to_phys_map()->metadata(virt);

	if (!region) {
		PWRN("no virt->phys mapping @ %p", virt);
		return 0;
	}

	return region->size();
}
