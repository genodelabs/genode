/*
 * \brief  Genode/Nova specific PGM code to resolve EPT faults
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
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

	/*
	 * If page is not allocated (== zero page) and no MMIO or active page, allocate and map it
	 * immediately. Important do not do this if A20 gate is disabled, A20 gate
	 * is handled by IEM/REM in this case.
	 */
	if (PGM_PAGE_IS_ZERO(pPage)
	    && !PGM_PAGE_IS_ALLOCATED(pPage)
	    && !PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage)
	    && !PGM_PAGE_IS_SPECIAL_ALIAS_MMIO(pPage)
	    && PGM_A20_IS_ENABLED(pVCpu)) {
		pgmLock(pVM);
		pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
		pgmUnlock(pVM);
	}

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
	if (VERBOSE_PGM)
		Vmm::log(Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)),
		         "->", Genode::Hex(GCPhys),
		         " type=", PGM_PAGE_GET_TYPE(pPage),
		         " state=", PGM_PAGE_GET_STATE(pPage),
		         " pde_type=", PGM_PAGE_GET_PDE_TYPE(pPage),
		         PGM_PAGE_GET_PDE_TYPE(pPage) == PGM_PAGE_PDE_TYPE_PDE ? "(is pde)" : "(not pde)",
		         " iPage=", iPage,
		         " range_start=", Genode::Hex(pRam->GCPhys),
		         " range_size=", Genode::Hex(pRam->cb),
		         " pages=", pRam->cb >> PAGE_SHIFT
		);
*/

	/* setup mapping for just a page as standard */
	fli = Flexpage_iterator(PGM_PAGE_GET_HCPHYS(pPage), 4096,
	                        GCPhys & ~0xFFFUL, 4096, GCPhys & ~0xFFFUL);

	if (PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE)
		return VINF_SUCCESS; /* one page mapping */

	Genode::addr_t const superpage_log2 = 21;
	Genode::addr_t const max_pages = pRam->cb >> PAGE_SHIFT;
	Genode::addr_t const superpage_pages = (1UL << superpage_log2) / 4096;
	Genode::addr_t const mask  = (1UL << superpage_log2) - 1;
	Genode::addr_t const super_gcphys = GCPhys & ~mask;

	RTGCPHYS max_off = super_gcphys - pRam->GCPhys;
	if (max_off > pRam->cb)
		return VINF_SUCCESS;

	Genode::addr_t super_hcphys = PGM_PAGE_GET_HCPHYS(pPage) & ~mask;

	unsigned const i_s = max_off >> PAGE_SHIFT;

	if (i_s + superpage_pages > max_pages)
		return VINF_SUCCESS; /* one page mapping */

	if (VERBOSE_PGM)
		Vmm::log(Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), "->",
		         Genode::Hex(GCPhys), " - iPage ", iPage, " [",
		         i_s, ",", i_s + superpage_pages, ")", " "
		         "range_size=", Genode::Hex(pRam->cb));

	/* paranoia sanity checks */
	for (Genode::addr_t i = i_s; i < i_s + superpage_pages; i++) {
		PPGMPAGE page = &pRam->aPages[i];

		Genode::addr_t const gcpage = pRam->GCPhys + (i << PAGE_SHIFT);

		if (!(super_hcphys == (PGM_PAGE_GET_HCPHYS(page) & ~mask)) ||
		    !(super_gcphys == (gcpage & ~mask)) ||
		    !(PGM_PAGE_GET_PDE_TYPE(page) == PGM_PAGE_PDE_TYPE_PDE) ||
		    !(PGM_PAGE_GET_TYPE(page) == PGM_PAGE_GET_TYPE(pPage)) ||
		    !(PGM_PAGE_GET_STATE(page) == PGM_PAGE_GET_STATE(pPage)))
		{
			if (VERBOSE_PGM)
				Vmm::error(Genode::Hex(PGM_PAGE_GET_HCPHYS(pPage)), "->",
				           Genode::Hex(GCPhys), " - iPage ", iPage, " i ", i, " [",
				           i_s, ",", i_s + superpage_pages, ")", " "
				           "range_size=", Genode::Hex(pRam->cb), " "
				           "super_hcphys=", Genode::Hex(super_hcphys), "?=", Genode::Hex((PGM_PAGE_GET_HCPHYS(page) & ~mask)), " "
				           "super_gcphys=", Genode::Hex(super_gcphys), "?=", Genode::Hex((gcpage & ~mask)), " ",
				           (int)(PGM_PAGE_GET_PDE_TYPE(page)), "?=", (int)PGM_PAGE_PDE_TYPE_PDE, " ",
				           (int)(PGM_PAGE_GET_TYPE(page)), "?=", (int)PGM_PAGE_GET_TYPE(pPage), " ",
				           (int)(PGM_PAGE_GET_STATE(page)), "?=", PGM_PAGE_GET_STATE(pPage));
			return VINF_SUCCESS; /* one page mapping */
		}
	}

	/* overwrite one-page mapping with super page mapping */
	fli = Flexpage_iterator(super_hcphys, 1 << superpage_log2,
	                        super_gcphys, 1 << superpage_log2, super_gcphys);

	/* revoke old mappings, e.g. less permissions or small pages */
	Nova::Rights const revoke_rwx(true, true, true);
	Nova::Crd crd = Nova::Mem_crd(super_hcphys >> 12, superpage_log2 - 12, revoke_rwx);
	Nova::revoke(crd, false);

	return VINF_SUCCESS; /* super page mapping */
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
	if (VERBOSE_PGM)
		Vmm::log(__LINE__, " gcphys=", Vmm::Hex(cr3),
		         " host=", Vmm::Hex(PGM_PAGE_GET_HCPHYS(pPage)),
		         " type=", Vmm::Hex(PGM_PAGE_GET_TYPE(pPage)),
		         " state=",Vmm::Hex(PGM_PAGE_GET_STATE(pPage)));
*/

	Genode::uint64_t *pdpte = reinterpret_cast<Genode::uint64_t*>(PGM_PAGE_GET_HCPHYS(pPage) + (cr3 & PAGE_OFFSET_MASK));

	Assert(pdpte != 0);

	return pdpte;
}
