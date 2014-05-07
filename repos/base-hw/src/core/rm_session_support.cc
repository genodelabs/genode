/*
 * \brief  RM- and pager implementations specific for base-hw and core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/pager.h>

/* core includes */
#include <rm_session_component.h>
#include <platform.h>
#include <platform_pd.h>
#include <platform_thread.h>
#include <translation_table.h>

using namespace Genode;


/***************
 ** Rm_client **
 ***************/

void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	/* remove mapping from the translation table of the thread that we serve */
	Platform_thread * const pt = (Platform_thread *)badge();
	if (!pt || !pt->pd()) return;

	Lock::Guard guard(*pt->pd()->lock());

	Translation_table * const tt = pt->pd()->translation_table();
	if (!tt) {
		PWRN("failed to get translation table of RM client");
		return;
	}
	tt->remove_translation(virt_base, size,pt->pd()->page_slab());

	/* update translation caches of all processors */
	Kernel::update_pd(pt->pd()->id());
}


/***************************
 ** Pager_activation_base **
 ***************************/

int Pager_activation_base::apply_mapping()
{
	/* prepare mapping */
	Platform_pd * const pd = (Platform_pd*)_fault.pd;

	Lock::Guard guard(*pd->lock());

	Translation_table * const tt = pd->translation_table();
	Page_slab * page_slab = pd->page_slab();

	Page_flags const flags =
	Page_flags::apply_mapping(_mapping.writable,
	                          _mapping.write_combined,
	                          _mapping.io_mem);

	/* insert mapping into translation table */
	try {
		for (unsigned retry = 0; retry < 2; retry++) {
			try {
				tt->insert_translation(_mapping.virt_address, _mapping.phys_address,
									   1 << _mapping.size_log2, flags, page_slab);
				return 0;
			} catch(Page_slab::Out_of_slabs) {
				page_slab->alloc_slab_block();
			}
		}
	} catch(Allocator::Out_of_memory) {
		PERR("Translation table needs to much RAM");
	} catch(...) {
		PERR("Invalid mapping %p -> %p (%zx)", (void*)_mapping.phys_address,
			 (void*)_mapping.virt_address, 1 << _mapping.size_log2);
	}
	return -1;
}


void Pager_activation_base::entry()
{
	/* get ready to receive faults */
	_cap = Native_capability(thread_get_my_native_id(), 0);
	_cap_valid.unlock();
	while (1)
	{
		/* await fault */
		Pager_object * o;
		while (1) {
			Signal s = Signal_receiver::wait_for_signal();
			o = dynamic_cast<Pager_object *>(s.context());
			if (o) {
				o->fault_occured(s);
				break;
			}
			PWRN("unknown pager object");
		}
		/* fetch fault data */
		Platform_thread * const pt = (Platform_thread *)o->badge();
		if (!pt) {
			PWRN("failed to get platform thread of faulter");
			continue;
		}
		unsigned const thread_id = pt->id();
		typedef Kernel::Thread_reg_id Reg_id;
		static addr_t const read_regs[] = {
			Reg_id::FAULT_TLB, Reg_id::IP, Reg_id::FAULT_ADDR,
			Reg_id::FAULT_WRITES, Reg_id::FAULT_SIGNAL };
		enum { READS = sizeof(read_regs)/sizeof(read_regs[0]) };
		void * const utcb = Thread_base::myself()->utcb()->base();
		memcpy(utcb, read_regs, sizeof(read_regs));
		addr_t * const values = (addr_t *)&_fault;
		if (Kernel::access_thread_regs(thread_id, READS, 0, values)) {
			PWRN("failed to read fault data");
			continue;
		}
		/* handle fault */
		if (o->pager(*this)) { continue; }
		if (apply_mapping()) {
			PWRN("failed to apply mapping");
			continue;
		}
		o->fault_resolved();
	}
}
