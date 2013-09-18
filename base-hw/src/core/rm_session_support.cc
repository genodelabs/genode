/*
 * \brief  RM- and pager implementations specific for base-hw and core
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>

/* core includes */
#include <rm_session_component.h>
#include <platform.h>
#include <platform_pd.h>
#include <platform_thread.h>
#include <tlb.h>

using namespace Genode;


/***************
 ** Rm_client **
 ***************/

void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	/* get software TLB of the thread that we serve */
	Platform_thread * const pt = Kernel::get_thread(badge());
	if (!pt) {
		PERR("failed to get RM client-thread");
		return;
	}
	Tlb * const tlb = pt->tlb();
	if (!tlb) {
		PERR("failed to get PD of RM client-thread");
		return;
	}
	/* update all translation caches */
	tlb->remove_region(virt_base, size);
	Kernel::update_pd(pt->pd_id());
	regain_ram_from_tlb(tlb);
}


/***************
 ** Ipc_pager **
 ***************/

int Ipc_pager::resolve_and_wait_for_fault()
{
	/* check mapping */
	if (!_mapping.valid()) {
		PERR("invalid mapping");
		return -1;
	}
	/* prepare mapping */
	Tlb * const tlb = _pagefault.tlb;
	Page_flags::access_t const flags =
	Page_flags::resolve_and_wait_for_fault(_mapping.writable,
	                                       _mapping.write_combined,
	                                       _mapping.io_mem);

	/* insert mapping into TLB */
	unsigned sl2;
	sl2 = tlb->insert_translation(_mapping.virt_address, _mapping.phys_address,
	                              _mapping.size_log2, flags);
	if (sl2)
	{
		/* try to get some natural aligned RAM */
		void * ram;
		bool ram_ok = platform()->ram_alloc()->alloc_aligned(1<<sl2, &ram,
		                                                     sl2).is_ok();
		if (!ram_ok) {
			PERR("failed to allocate additional RAM for TLB");
			return -1;
		}
		/* try to translate again with extra RAM */
		sl2 = tlb->insert_translation(_mapping.virt_address,
		                              _mapping.phys_address,
		                              _mapping.size_log2, flags, ram);
		if (sl2) {
			PERR("TLB needs to much RAM");
			regain_ram_from_tlb(tlb);
			return -1;
		}
	}
	/* wake up faulter */
	Kernel::resume_faulter(_pagefault.thread_id);

	/* wait for next page fault */
	wait_for_fault();
	return 0;
}

