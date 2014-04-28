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
#include <tlb.h>

using namespace Genode;


/**************************************
 ** Helpers for processor broadcasts **
 **************************************/

struct Update_pd_data { unsigned const pd_id; };

void update_pd(void * const data)
{
	auto const d = reinterpret_cast<Update_pd_data *>(data);
	Kernel::update_pd(d->pd_id);
}


/***************
 ** Rm_client **
 ***************/

void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	/* remove mapping from the translation table of the thread that we serve */
	Platform_thread * const pt = (Platform_thread *)badge();
	if (!pt) {
		PWRN("failed to get platform thread of RM client");
		return;
	}
	Tlb * const tlb = pt->tlb();
	if (!tlb) {
		PWRN("failed to get page table of RM client");
		return;
	}
	tlb->remove_region(virt_base, size);

	/* update translation caches of all processors */
	Kernel::update_pd(pt->pd()->id());

	/* try to get back released memory from the translation table */
	regain_ram_from_tlb(tlb);
}


/***************************
 ** Pager_activation_base **
 ***************************/

int Pager_activation_base::apply_mapping()
{
	/* prepare mapping */
	Tlb * const tlb = (Tlb *)_fault.tlb;
	Page_flags const flags =
	Page_flags::apply_mapping(_mapping.writable,
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
			PWRN("failed to allocate additional RAM for TLB");
			return -1;
		}
		/* try to translate again with extra RAM */
		sl2 = tlb->insert_translation(_mapping.virt_address,
		                              _mapping.phys_address,
		                              _mapping.size_log2, flags, ram);
		if (sl2) {
			PWRN("TLB needs to much RAM");
			regain_ram_from_tlb(tlb);
			return -1;
		}
	}
	return 0;
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
