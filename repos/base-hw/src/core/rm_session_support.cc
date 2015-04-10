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
	Locked_ptr<Address_space> locked_address_space(_address_space);

	if (locked_address_space.is_valid())
		locked_address_space->flush(virt_base, size);
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
	                          _mapping.cacheable,
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
		PERR("Invalid mapping %p -> %p (%lx)", (void*)_mapping.phys_address,
			 (void*)_mapping.virt_address, 1UL << _mapping.size_log2);
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
		/* receive fault */
		Signal s = Signal_receiver::wait_for_signal();
		Pager_object * po = static_cast<Pager_object *>(s.context());

		/*
		 * Synchronize access and ensure that the object is still managed
		 *
		 * FIXME: The implicit lookup of the oject isn't needed.
		 */
		unsigned const pon = po->cap().local_name();
		Object_pool<Pager_object>::Guard pog(_ep->lookup_and_lock(pon));
		if (!pog) {
			PWRN("failed to lookup pager object");
			continue;
		}
		/* let pager object go to fault state */
		pog->fault_occured(s);

		/* fetch fault data */
		Platform_thread * const pt = (Platform_thread *)pog->badge();
		if (!pt) {
			PWRN("failed to get platform thread of faulter");
			continue;
		}
		typedef Kernel::Thread_reg_id Reg_id;
		static addr_t const read_regs[] = {
			Reg_id::FAULT_TLB, Reg_id::IP, Reg_id::FAULT_ADDR,
			Reg_id::FAULT_WRITES, Reg_id::FAULT_SIGNAL };
		enum { READS = sizeof(read_regs)/sizeof(read_regs[0]) };
		void * const utcb = Thread_base::myself()->utcb()->base();
		memcpy(utcb, read_regs, sizeof(read_regs));
		addr_t * const values = (addr_t *)&_fault;
		if (Kernel::access_thread_regs(pt->kernel_thread(), READS, 0, values)) {
			PWRN("failed to read fault data");
			continue;
		}
		/* try to resolve fault directly via local region managers */
		if (pog->pager(*this)) { continue; }

		/* apply mapping that was determined by the local region managers */
		if (apply_mapping()) {
			PWRN("failed to apply mapping");
			continue;
		}
		/* let pager object go back to no-fault state */
		pog->fault_resolved();
	}
}
