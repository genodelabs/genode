/*
 * \brief  Genode/Nova specific PGM code to resolve EPT faults
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include "vcpu.h"
#include "PGMInline.h"

enum { VERBOSE_PGM = false };

int Vcpu_handler::map_memory(RTGCPHYS GCPhys, size_t cbWrite,
                             RTGCUINT vbox_fault_reason,
                             Genode::Flexpage_iterator &fli, bool &writeable)
{
	using Genode::Flexpage_iterator;
	using Genode::addr_t;

	PVM      pVM = _current_vm;
	PVMCPU pVCpu = _current_vcpu;

	_ept_fault_addr_type = PGMPAGETYPE_INVALID;

	/* DON'T USE normal printf in this function - corrupts unsaved UTCB !!! */

	PPGMRAMRANGE pRam = pgmPhysGetRangeAtOrAbove(pVM, GCPhys);
	if (!pRam)
		return VERR_PGM_DYNMAP_FAILED;

/* XXX pgmLock/pgmUnlock XXX */

	RTGCPHYS off = GCPhys - pRam->GCPhys;
	if (off >= pRam->cb)
		return VERR_PGM_DYNMAP_FAILED;

	unsigned iPage = off >> PAGE_SHIFT;
	PPGMPAGE pPage = &pRam->aPages[iPage];

	_ept_fault_addr_type = PGM_PAGE_GET_TYPE(pPage);

	if (PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage) ||
	    PGM_PAGE_IS_SPECIAL_ALIAS_MMIO(pPage) ||
	    PGM_PAGE_IS_ZERO(pPage)) {

		if (PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO &&
		    !PGM_PAGE_IS_ZERO(pPage)) {

			Vmm::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " ",
			         PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage), " ",
			         PGM_PAGE_IS_SPECIAL_ALIAS_MMIO(pPage), " ",
			         PGM_PAGE_IS_ZERO(pPage), " "
			         " vbox_fault_reason=", Genode::Hex(vbox_fault_reason));
			Vmm::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			         "host=", Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), " "
			         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
			         "writeable=", writeable, " "
			         "state=", Genode::Hex(PGM_PAGE_GET_STATE(pPage)));
		}
		return VERR_PGM_DYNMAP_FAILED;
	}

	if (!PGM_PAGE_IS_ALLOCATED(pPage))
		Vmm::log("unknown page state ", Genode::Hex(PGM_PAGE_GET_STATE(pPage)),
		         " GCPhys=", Genode::Hex(GCPhys));
	Assert(PGM_PAGE_IS_ALLOCATED(pPage));

	if (PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_RAM   &&
	    PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO2 &&
	    PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_ROM)
	{
		if (VERBOSE_PGM)
			Vmm::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			         "vbox_fault_reason=", Genode::Hex(vbox_fault_reason), " "
			         "host=", Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), " "
			         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
			         "state=", Genode::Hex(PGM_PAGE_GET_STATE(pPage)));
		return VERR_PGM_DYNMAP_FAILED;
	}

	Assert(!PGM_PAGE_IS_ZERO(pPage));

	/* write fault on a ROM region */
	if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_ROM &&
	    vbox_fault_reason & VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE) {
		Vmm::warning(__func__, " - write fault on ROM region!? gp=",
		             Genode::Hex(GCPhys));
		return VERR_PGM_DYNMAP_FAILED;
	}

	/* nothing should be mapped - otherwise we get endless overmap loops */
	Assert(!(vbox_fault_reason & VMX_EXIT_QUALIFICATION_EPT_ENTRY_PRESENT));

	writeable = PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_ROM;

	PPGMPHYSHANDLER handler = pgmHandlerPhysicalLookup(pVM, GCPhys);

	if (VERBOSE_PGM && PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2 &&
	    !handler)
		Vmm::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " ",
		         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
		         "state=", Genode::Hex(PGM_PAGE_GET_STATE(pPage)), " "
		         "- MMIO2 w/o handler");

	if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2 && handler) {
		PFNPGMPHYSHANDLER pfnHandler = PGMPHYSHANDLER_GET_TYPE(pVM, handler)->CTX_SUFF(pfnHandler);
		if (!pfnHandler) {
			Vmm::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)));
			return VERR_PGM_DYNMAP_FAILED;
		}
		void *pvUser = handler->CTX_SUFF(pvUser);
		if (!pvUser) {
			Vmm::log(__LINE__, " GCPhys=", Genode::Hex(GCPhys), " "
			         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)));
			return VERR_PGM_DYNMAP_FAILED;
		}

		PGMACCESSTYPE access_type = (vbox_fault_reason & VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE) ? PGMACCESSTYPE_WRITE : PGMACCESSTYPE_READ;

		VBOXSTRICTRC rcStrict = pfnHandler(pVM, pVCpu, GCPhys, nullptr, nullptr, 0, access_type, PGMACCESSORIGIN_HM, pvUser);
		if (rcStrict != VINF_PGM_HANDLER_DO_DEFAULT) {
			Vmm::log(__LINE__, " nodefault GCPhys=", Genode::Hex(GCPhys), " "
			         "type=", Genode::Hex(PGM_PAGE_GET_TYPE(pPage)), " "
			         "pfnHandler=", pfnHandler);
			return VERR_PGM_DYNMAP_FAILED;
		}
	}

/*
	Vmm::printf("0x%lx->0x%lx type=%u state=0x%u pde=%u iPage=%u range_size=0x%lx pages=%u\n",
	            PGM_PAGE_GET_HCPHYS(pPage), GCPhys, PGM_PAGE_GET_TYPE(pPage),
	            PGM_PAGE_GET_STATE(pPage), PGM_PAGE_GET_PDE_TYPE(pPage),
	            iPage, pRam->cb, pRam->cb >> PAGE_SHIFT);
*/

	if (PGM_PAGE_GET_PDE_TYPE(pPage) == PGM_PAGE_PDE_TYPE_PDE) {
		Genode::addr_t const max_pages = pRam->cb >> PAGE_SHIFT;
		Genode::addr_t const superpage_pages = (1 << 21) / 4096;
		Genode::addr_t mask  = (1 << 21) - 1;
		Genode::addr_t super_gcphys = GCPhys & ~mask;

		RTGCPHYS max_off = super_gcphys - pRam->GCPhys;
		Assert (max_off < pRam->cb);

		Genode::addr_t super_hcphys = PGM_PAGE_GET_HCPHYS(pPage) & ~mask;

		unsigned const i_s = max_off >> PAGE_SHIFT;

		Assert (i_s + superpage_pages <= max_pages);

		if (VERBOSE_PGM)
			Vmm::log(Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), "->",
			         Genode::Hex(GCPhys), " - iPage ", iPage, " [",
			         i_s, ",", i_s + superpage_pages, ")", " "
			         "range_size=", Genode::Hex(pRam->cb));

		/* paranoia sanity checks */
		for (Genode::addr_t i = i_s; i < i_s + superpage_pages; i++) {
			PPGMPAGE page = &pRam->aPages[i];

			Genode::addr_t gcpage = pRam->GCPhys + i << PAGE_SHIFT;

			Assert(super_hcphys == (PGM_PAGE_GET_HCPHYS(page) & ~mask));
			Assert(super_gcphys == (gcpage & ~mask));
			Assert(PGM_PAGE_GET_PDE_TYPE(page) == PGM_PAGE_PDE_TYPE_PDE);
			Assert(PGM_PAGE_GET_TYPE(page) == PGM_PAGE_GET_TYPE(pPage));
			Assert(PGM_PAGE_GET_STATE(page) == PGM_PAGE_GET_STATE(pPage));
		}

		fli = Flexpage_iterator(super_hcphys, 1 << 21,
		                        super_gcphys, 1 << 21, super_gcphys);
	} else
		fli = Flexpage_iterator(PGM_PAGE_GET_HCPHYS(pPage), 4096,
		                        GCPhys & ~0xFFFUL, 4096, GCPhys & ~0xFFFUL);

	return VINF_SUCCESS;
}


Genode::uint64_t * Vcpu_handler::pdpte_map(VM *pVM, RTGCPHYS cr3)
{
	PPGMRAMRANGE pRam = pgmPhysGetRangeAtOrAbove(pVM, cr3);
	Assert (pRam);

/* XXX pgmLock/pgmUnlock XXX */

	RTGCPHYS off = cr3 - pRam->GCPhys;
	Assert (off < pRam->cb);

	unsigned iPage = off >> PAGE_SHIFT;
	PPGMPAGE pPage = &pRam->aPages[iPage];

/*
	Vmm::printf("%u gcphys=0x%lx host=0x%lx type=%lx state=0x%lx\n", __LINE__,
	            cr3, PGM_PAGE_GET_HCPHYS(pPage), PGM_PAGE_GET_TYPE(pPage), PGM_PAGE_GET_STATE(pPage));
*/

	Genode::uint64_t *pdpte = reinterpret_cast<Genode::uint64_t*>(PGM_PAGE_GET_HCPHYS(pPage) + (cr3 & PAGE_OFFSET_MASK));

	Assert(pdpte != 0);

	return pdpte;
}
