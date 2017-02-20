/*
 * \brief  Registry of VMM-local memory regions
 * \author Norman Feske
 * \date   2013-09-02
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VMM_MEMORY_H_
#define _VMM_MEMORY_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <util/list.h>

/* Genode/Virtualbox includes */
#include "mem_region.h"
#include "vmm.h"

/* VirtualBox includes */
#include <VBox/vmm/pgm.h>


class Vmm_memory
{
	typedef Genode::size_t           size_t;
	typedef Genode::Lock             Lock;
	typedef Genode::List<Mem_region> Mem_region_list;

	private:

		Lock             _lock;
		Mem_region_list  _regions;
		Genode::Env     &_env;

		/**
		 * Backing store
		 */

		Mem_region *_lookup_unsynchronized(PPDMDEVINS pDevIns, unsigned iRegion)
		{
			for (Mem_region *r = _regions.first(); r; r = r->next())
				if (r->pDevIns == pDevIns && r->iRegion == iRegion)
					return r;
			return 0;
		}

		Mem_region *_lookup_unsynchronized(RTGCPHYS vm_phys, size_t size)
		{
			for (Mem_region *r = _regions.first(); r; r = r->next())
				if (r->vm_phys && r->vm_phys <= vm_phys
				    && vm_phys - r->vm_phys < r->size()
				    && r->size() - (vm_phys - r->vm_phys) >= size)
					return r;

			return 0;
		}

	public:

		Vmm_memory(Genode::Env &env) : _env(env) { }

		/**
		 * \throw  Ram_session::Alloc_failed
		 * \throw  Region_map::Attach_failed
		 */
		void *alloc(size_t cb, PPDMDEVINS pDevIns, unsigned iRegion)
		{
			Lock::Guard guard(_lock);

			try {
				Mem_region *r = new (vmm_heap()) Mem_region(_env, cb,
				                                            pDevIns, iRegion);
				_regions.insert(r);

				return r->local_addr<void>();

			} catch (Genode::Ram_session::Alloc_failed) {
				Genode::error("Vmm_memory::alloc(", Genode::Hex(cb), "): "
				              "RAM allocation failed");
				throw;
			} catch (Genode::Region_map::Attach_failed) {
				Genode::error("Vmm_memory::alloc(", Genode::Hex(cb), "): "
				              "RM attach failed");
				throw;
			}

			return 0;
		}

		void *alloc_rom(size_t cb, PPDMDEVINS pDevIns)
		{
			return alloc(cb, pDevIns, ~0U);
		}

		void *alloc_ram(size_t cb)
		{
			return alloc(cb, 0, ~0U);
		}

		bool add_handler(RTGCPHYS vm_phys, size_t size,
		                 PFNPGMR3PHYSHANDLER pfnHandlerR3, void *pvUserR3,
		                 PGMPHYSHANDLERTYPE enmType = PGMPHYSHANDLERTYPE_PHYSICAL_ALL)
		{
			Lock::Guard guard(_lock);

			Mem_region *r = _lookup_unsynchronized(vm_phys, size);

			if (!r) return false;

			r->enmType      = enmType;
			r->pfnHandlerR3 = pfnHandlerR3;
			r->pvUserR3     = pvUserR3;

			return true;
		}

		void * lookup(RTGCPHYS vm_phys, size_t size,
		              PFNPGMR3PHYSHANDLER *ppfnHandlerR3 = 0,
		              void **ppvUserR3 = 0,
		              PGMPHYSHANDLERTYPE *enmType = 0)
		{
			Lock::Guard guard(_lock);

			Mem_region *r = _lookup_unsynchronized(vm_phys, size);

			if (!r) return 0;

			if (enmType)       *enmType       = r->enmType;
			if (ppfnHandlerR3) *ppfnHandlerR3 = r->pfnHandlerR3;
			if (ppvUserR3)     *ppvUserR3     = r->pvUserR3;

			return reinterpret_cast<void *>(r->local_addr<uint8_t>() +
			                                (vm_phys - r->vm_phys));
		}

		bool lookup_range(RTGCPHYS &vm_phys, size_t &size)
		{
			Lock::Guard guard(_lock);

			Mem_region *r = _lookup_unsynchronized(vm_phys, size);

			if (!r)
				return false;

			vm_phys = r->vm_phys;
			size    = r->size();

			return true;
		}

		size_t map_to_vm(PPDMDEVINS pDevIns, RTGCPHYS GCPhys,
		                 unsigned iRegion = ~0U)
		{
			Lock::Guard guard(_lock);

			Mem_region *r = _lookup_unsynchronized(pDevIns, iRegion);

			if (r) r->vm_phys = GCPhys;

			return r ? r->size() : 0;
		}

		bool unmap_from_vm(RTGCPHYS GCPhys, size_t size, bool invalidate = false)
		{
			Lock::Guard guard(_lock);

			Mem_region *r = _lookup_unsynchronized(GCPhys, size);
			if (!r) return false;

			bool result = revoke_from_vm(r);

			if (invalidate)
				r->vm_phys = 0ULL;

			return result;
		}

		/**
		 * Platform specific implemented.
		 */
		bool revoke_from_vm(Mem_region *r);

		/**
		 * Revoke all memory (RAM or ROM) from VM
		 */
		void revoke_all()
		{
			Lock::Guard guard(_lock);

			for (Mem_region *r = _regions.first(); r; r = r->next())
			{
				bool ok = revoke_from_vm(r);
				Assert(ok);
			}
		}
};


/**
 * Return pointer to singleton instance
 */
Vmm_memory *vmm_memory();


#endif /* _VMM_MEMORY_H_ */
