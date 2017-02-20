/*
 * \brief  Registry of known guest-physical memory regions
 * \author Norman Feske
 * \date   2013-09-02
 *
 * Contains the mapping of guest-phyiscal to VMM-local addresses.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _GUEST_MEMORY_H_
#define _GUEST_MEMORY_H_

/* Genode includes */
#include <base/lock.h>
#include <base/log.h>
#include <util/flex_iterator.h>
#include <util/list.h>

#include "vmm.h"

/* VirtualBox includes */
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/pdmdev.h>


class Guest_memory
{
	struct Region;

	/*
	 * XXX Use AVL tree instead of a linked list
	 */

	typedef Genode::List<Region> Region_list;
	typedef Genode::Lock         Lock;
	typedef Genode::addr_t       addr_t;

	private:

		struct Region : Region_list::Element
		{
			RTGCPHYS const _GCPhys;    /* guest-physical address */
			RTGCPHYS const _cb;        /* size */
			void   * const _pv;        /* VMM-local address */

			/*
			 * MMIO-specific members
			 */
			PPDMDEVINS      const _pDevIns;
			RTHCPTR         const _pvUser;
			PFNIOMMMIOWRITE const _pfnWriteCallback;
			PFNIOMMMIOREAD  const _pfnReadCallback;
			PFNIOMMMIOFILL  const _pfnFillCallback;
			uint32_t        const _fFlags;

			Region(RTGCPHYS const GCPhys, RTGCPHYS const cb, void *const pv,
			       PPDMDEVINS          pDevIns,
			       RTHCPTR             pvUser,
			       PFNIOMMMIOWRITE     pfnWriteCallback,
			       PFNIOMMMIOREAD      pfnReadCallback,
			       PFNIOMMMIOFILL      pfnFillCallback,
			       uint32_t            fFlags)
			:
				_GCPhys(GCPhys), _cb(cb), _pv(pv),
				_pDevIns          (pDevIns),
				_pvUser           (pvUser),
				_pfnWriteCallback (pfnWriteCallback),
				_pfnReadCallback  (pfnReadCallback),
				_pfnFillCallback  (pfnFillCallback),
				_fFlags           (fFlags)
			{ }

			/**
			 * Return true if region contains specified guest-physical area
			 */
			bool contains(RTGCPHYS GCPhys, size_t size) const
			{
				return (_GCPhys <= GCPhys) && (GCPhys < _GCPhys + _cb) &&
				       (_GCPhys + _cb - GCPhys >= size);
			}

			/**
			 * Return true if region is disjunct to specified guest-physical area
			 */
			bool disjunct(RTGCPHYS GCPhys, size_t size) const
			{
				return (GCPhys + size - 1 < _GCPhys) ||
				       (_GCPhys + _cb - 1 < GCPhys);
			}

			/**
			 * Return guest-physical base address
			 */
			RTGCPHYS GCPhys() const { return _GCPhys; }

			/**
			 * Return VMM-local base address
			 */
			void *pv() const { return _pv; }

			void dump() const
			{
				Genode::log("phys ", Genode::Hex_range<RTGCPHYS>(_GCPhys, _cb),
				            " -> virt ", Genode::Hex_range<Genode::addr_t>((Genode::addr_t)_pv, _cb),
				            " (dev='", _pDevIns && _pDevIns->pReg ? _pDevIns->pReg->szName : 0, "'");
			}

			void *pv_at_offset(addr_t offset)
			{
				if (_pv)
					return (void *)((addr_t)_pv + (addr_t)offset);
				return 0;
			}

			int mmio_write(RTGCPHYS GCPhys, void const *pv, unsigned cb)
			{
				if (!_pfnWriteCallback)
					return VINF_SUCCESS;

				int rc = PDMCritSectEnter(_pDevIns->CTX_SUFF(pCritSectRo),
				                          VINF_IOM_R3_MMIO_READ);
				if (rc != VINF_SUCCESS)
					return rc;

				rc = _pfnWriteCallback(_pDevIns, _pvUser, GCPhys, pv, cb);

				PDMCritSectLeave(_pDevIns->CTX_SUFF(pCritSectRo));

				return rc;
			}

			int mmio_read(RTGCPHYS GCPhys, void *pv, unsigned cb)
			{
				if (!_pfnReadCallback)
					return VINF_IOM_MMIO_UNUSED_FF;

				int rc = PDMCritSectEnter(_pDevIns->CTX_SUFF(pCritSectRo),
				                          VINF_IOM_R3_MMIO_WRITE);

				rc = _pfnReadCallback(_pDevIns, _pvUser, GCPhys, pv, cb);

				PDMCritSectLeave(_pDevIns->CTX_SUFF(pCritSectRo));

				return rc;
			}

			bool simple_mmio_write(RTGCPHYS vm_phys, unsigned cb)
			{
				/* adhere to original IOMMIOWrite check */
				return (cb == 4 && !(vm_phys & 3)) ||
				       ((_fFlags & IOMMMIO_FLAGS_WRITE_MODE) == IOMMMIO_FLAGS_WRITE_PASSTHRU) ||
				       (cb == 8 && !(vm_phys & 7) && IOMMMIO_DOES_WRITE_MODE_ALLOW_QWORD(_fFlags));
			}

			bool simple_mmio_read(RTGCPHYS vm_phys, unsigned cb)
			{
				/* adhere to original IOMMIORead check */
				return (cb == 4 && !(vm_phys & 3)) ||
				       ((_fFlags & IOMMMIO_FLAGS_READ_MODE) == IOMMMIO_FLAGS_READ_PASSTHRU) ||
				       (cb == 8 && !(vm_phys & 7) && (_fFlags & IOMMMIO_FLAGS_READ_MODE) == IOMMMIO_FLAGS_READ_DWORD_QWORD);
			}
		};

		Lock        _lock;
		Region_list _ram_regions;
		Region_list _rom_regions;
		Region_list _mmio_regions;

		static Region *_lookup(RTGCPHYS GCPhys, Region_list &regions, size_t size)
		{
			using Genode::addr_t;

			for (Region *r = regions.first(); r; r = r->next())
				if (r->contains(GCPhys, size))
					return r;

			return 0;
		}

		static bool _overlap(RTGCPHYS GCPhys, size_t size, 
		                     Region_list &regions)
		{
			using Genode::addr_t;

			for (Region *r = regions.first(); r; r = r->next())
			{
				if (r->disjunct(GCPhys, size))
					continue;

				return true;
			}

			return false;
		}

		/**
		 * \return  looked-up region, or 0 if lookup failed
		 */
		Region *_lookup(RTGCPHYS GCPhys, size_t size)
		{
			using Genode::addr_t;

			/*
			 * ROM regions may alias RAM regions. For the lookup, always
			 * consider ROM regions first.
			 */

			if (Region *r = _lookup(GCPhys, _rom_regions, size))
				return r;

			if (Region *r = _lookup(GCPhys, _mmio_regions, size))
				return r;

			if (Region *r = _lookup(GCPhys, _ram_regions, size))
				return r;

			return 0;
		}

	public:

		class Region_conflict { };

		/**
		 * \throw Region_conflict
		 */
		void add_ram_mapping(RTGCPHYS const GCPhys, RTGCPHYS const cb, void * const pv)
		{
			/*
			 * XXX check for overlapping regions
			 */
			_ram_regions.insert(new (vmm_heap())
			                        Region(GCPhys, cb, pv, 0, 0, 0, 0, 0, 0));
		}

		/**
		 * \throw Region_conflict
		 */
		void add_rom_mapping(RTGCPHYS const GCPhys, RTGCPHYS const cb,
		                     void const * const pv, PPDMDEVINS      pDevIns)
		{
			/*
			 * XXX check for overlapping regions
			 */
			_rom_regions.insert(new (vmm_heap())
			                   Region(GCPhys, cb,
			                   (void *)pv, pDevIns, 0, 0, 0, 0, 0));
		}

		/**
		 * \throw Region_conflict
		 */
		void add_mmio_mapping(RTGCPHYS const GCPhys, RTGCPHYS const cb,
		                      PPDMDEVINS      pDevIns,
		                      RTHCPTR         pvUser,
		                      PFNIOMMMIOWRITE pfnWriteCallback,
		                      PFNIOMMMIOREAD  pfnReadCallback,
		                      PFNIOMMMIOFILL  pfnFillCallback,
		                      uint32_t        fFlags)
		{
			/*
			 * XXX check for overlapping regions
			 */
			_mmio_regions.insert(new (vmm_heap())
			                     Region(GCPhys, cb, 0,
			                            pDevIns, pvUser, pfnWriteCallback,
			                            pfnReadCallback, pfnFillCallback, fFlags));
		}


		bool remove_mmio_mapping(RTGCPHYS const GCPhys, RTGCPHYS const size)
		{
			Region *r = _lookup(GCPhys, _mmio_regions, size);
			if (!r)
				return false;

			_mmio_regions.remove(r);
			delete r;
			return true;
		}


		void dump() const
		{
			Genode::log("guest-physical to VMM-local RAM mappings:");
			for (Region const *r = _ram_regions.first(); r; r = r->next())
				r->dump();

			Genode::log("guest-physical to VMM-local ROM mappings:");
			for (Region const *r = _rom_regions.first(); r; r = r->next())
				r->dump();

			Genode::log("guest-physical MMIO regions:");
			for (Region const *r = _mmio_regions.first(); r; r = r->next())
				r->dump();
		}

		/**
		 * \return  looked-up VMM-local address, or 0 if lookup failed
		 */
		void *lookup(RTGCPHYS GCPhys, size_t size)
		{
			Region *r = _lookup(GCPhys, size);
			if (!r)
				return 0;

			return r->pv_at_offset(GCPhys - r->GCPhys());
		}

		/**
		 * \return looked-up VMM-local address if Guest address is RAM
		 */
		void *lookup_ram(RTGCPHYS const GCPhys, size_t size,
		                 Genode::Flexpage_iterator &it)
		{
			if (_overlap(GCPhys, size, _rom_regions))
				return 0;

			if (_overlap(GCPhys, size, _mmio_regions))
				return 0;

			if (!_overlap(GCPhys, size, _ram_regions))
				return 0;

			Region *r = _lookup(GCPhys, _ram_regions, size);
			if (!r)
				return 0;

			void * vmm_local = lookup_ram(GCPhys & ~(size * 2UL - 1), size * 2UL, it);
			if (vmm_local)
				return vmm_local;

			it = Genode::Flexpage_iterator((addr_t)r->pv_at_offset(GCPhys - r->GCPhys()), size, GCPhys, size, GCPhys);

			return r->pv_at_offset(GCPhys - r->GCPhys());
		}

		/**
		 * \return VirtualBox return code
		 */
		int mmio_write(RTGCPHYS vm_phys, uint32_t u32Value, size_t size)
		{
			Region *r = _lookup(vm_phys, size);

			if (!r) {
				Genode::error("Guest_memory::mmio_write: lookup failed - "
				              "GCPhys=", Genode::Hex(vm_phys), " u32Value=",
				              u32Value, " size=", size);
				return VERR_IOM_MMIO_RANGE_NOT_FOUND;
			}

			/* use VERR_IOM_NOT_MMIO_RANGE_OWNER to request complicated write */
			if (!r->simple_mmio_write(vm_phys, size))
				return VERR_IOM_NOT_MMIO_RANGE_OWNER;

			int rc = r->mmio_write(vm_phys, &u32Value, size);
			/* check that VERR_IOM_NOT_MMIO_RANGE_OWNER is unused, see above */
			Assert(rc != VERR_IOM_NOT_MMIO_RANGE_OWNER);
			return rc;
		}

		/**
		 * \return VirtualBox return code
		 */
		int mmio_read(RTGCPHYS vm_phys, uint32_t *u32Value, size_t size)
		{
			Region *r = _lookup(vm_phys, size);

			if (!r) {
				Genode::error("Guest_memory::mmio_read: lookup failed - "
				              "GCPhys=", Genode::Hex(vm_phys), " u32Value=",
				              u32Value, " size=", size);
				return VERR_IOM_MMIO_RANGE_NOT_FOUND;
			}

			/* use VERR_IOM_NOT_MMIO_RANGE_OWNER to request complicated read */
			if (!r->simple_mmio_read(vm_phys, size))
				return VERR_IOM_NOT_MMIO_RANGE_OWNER;

			int rc = r->mmio_read(vm_phys, u32Value, size);
			/* check that VERR_IOM_NOT_MMIO_RANGE_OWNER is unused, see above */
			Assert(rc != VERR_IOM_NOT_MMIO_RANGE_OWNER);
			return rc;
		}
};


/**
 * Return pointer to singleton instance
 */
Guest_memory *guest_memory();


#endif /* _GUEST_MEMORY_H_ */
