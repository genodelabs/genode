/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2015-02-17
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>

/* core includes */
#include <kernel/core_interface.h>
#include <vm_session_component.h>
#include <platform.h>
#include <core_env.h>

using namespace Genode;


void Vm_session_component::exception_handler(Signal_context_capability handler)
{
	Core_mem_allocator * cma =
		static_cast<Core_mem_allocator*>(platform()->core_mem_alloc());

	if (!create((void*)_ds.core_local_addr(), Capability_space::capid(handler),
	            cma->phys_addr(_table)))
		Genode::warning("Cannot instantiate vm kernel object, invalid signal context?");
}


void Vm_session_component::_attach(addr_t phys_addr, addr_t vm_addr, size_t size)
{
	Page_flags pflags { RW, NO_EXEC, USER, NO_GLOBAL, RAM, CACHED };

	try {
		_table->insert_translation(vm_addr, phys_addr, size, pflags, _tt_alloc);
		return;
	} catch(Allocator::Out_of_memory) {
		Genode::error("Translation table needs to much RAM");
	} catch(...) {
		Genode::error("Invalid mapping ", Genode::Hex(phys_addr), " -> ",
		              Genode::Hex(vm_addr), " (", size, ")");
	}
}


void Vm_session_component::attach(Dataspace_capability ds_cap, addr_t vm_addr)
{
	/* check dataspace validity */
	_ds_ep->apply(ds_cap, [&] (Dataspace_component *dsc) {
		if (!dsc) throw Invalid_dataspace();

		_attach(dsc->phys_addr(), vm_addr, dsc->size());
	});
}


void Vm_session_component::attach_pic(addr_t vm_addr)
{
	_attach(Board::IRQ_CONTROLLER_VT_CPU_BASE, vm_addr,
	        Board::IRQ_CONTROLLER_VT_CPU_SIZE);
}


void Vm_session_component::detach(addr_t vm_addr, size_t size) {
	_table->remove_translation(vm_addr, size, _tt_alloc); }


Vm_session_component::Vm_session_component(Rpc_entrypoint  *ds_ep,
                                           size_t           ram_quota)
: _ds_ep(ds_ep), _ds(_ds_size(), _alloc_ds(ram_quota), UNCACHED, true, 0),
  _ds_cap(static_cap_cast<Dataspace>(_ds_ep->manage(&_ds)))
{
	_ds.assign_core_local_addr(core_env()->rm_session()->attach(_ds_cap));

	Core_mem_allocator * cma =
		static_cast<Core_mem_allocator*>(platform()->core_mem_alloc());
	void *tt;

	/* get some aligned space for the translation table */
	if (!cma->alloc_aligned(sizeof(Translation_table), (void**)&tt,
	                        Translation_table::ALIGNM_LOG2).ok()) {
		error("failed to allocate kernel object");
		throw Root::Quota_exceeded();
	}

	_table    = construct_at<Translation_table>(tt);
	_tt_alloc = (new (cma) Table_allocator(cma))->alloc();
}


Vm_session_component::~Vm_session_component()
{
	/* dissolve VM dataspace from service entry point */
	_ds_ep->dissolve(&_ds);

	/* free region in allocator */
	core_env()->rm_session()->detach(_ds.core_local_addr());
	platform()->ram_alloc()->free((void*)_ds.phys_addr());

	/* free guest-to-host page tables */
	destroy(platform()->core_mem_alloc(), _table);
	destroy(platform()->core_mem_alloc(), Table_allocator::base(_tt_alloc));
}
