/*
 * \brief  RM- and pager implementations specific for base-hw and core
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>

/* core includes */
#include <rm_session_component.h>
#include <platform.h>
#include <platform_thread.h>
#include <assert.h>
#include <software_tlb.h>

using namespace Genode;


/***************
 ** Rm_client **
 ***************/


void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	/* get software TLB of the thread that we serve */
	Platform_thread * const pt = Kernel::get_thread(badge());
	assert(pt);
	Software_tlb * const tlb = pt->software_tlb();
	assert(tlb);

	/* update all translation caches */
	tlb->remove_region(virt_base, size);
	Kernel::update_pd(pt->pd_id());

	/* try to regain administrative memory that has been freed by unmap */
	size_t s;
	void * base;
	while (tlb->regain_memory(base, s)) platform()->ram_alloc()->free(base, s);
}


/***************
 ** Ipc_pager **
 ***************/

void Ipc_pager::resolve_and_wait_for_fault()
{
	/* valid mapping? */
	assert(_mapping.valid());

	/* do we need extra space to resolve pagefault? */
	Software_tlb * const tlb = _pagefault.software_tlb;
	enum Mapping_attributes { X = 1, K = 0, G = 0 };
	unsigned sl2 = tlb->insert_translation(_mapping.virt_address,
	               _mapping.phys_address, _mapping.size_log2,
	               _mapping.writable, X, K, G);
	if (sl2)
	{
		/* try to get some natural aligned space */
		void * space;
		assert(platform()->ram_alloc()->alloc_aligned(1<<sl2, &space, sl2));

		/* try to translate again with extra space */
		sl2 = tlb->insert_translation(_mapping.virt_address,
		                              _mapping.phys_address,
		                              _mapping.size_log2,
		                              _mapping.writable, X, K, G, space);
		assert(!sl2);
	}
	/* try to wake up faulter */
	assert(!Kernel::resume_thread(_pagefault.thread_id));

	/* wait for next page fault */
	wait_for_fault();
}

