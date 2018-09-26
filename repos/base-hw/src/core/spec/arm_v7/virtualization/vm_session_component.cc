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

static Core_mem_allocator & cma() {
	return static_cast<Core_mem_allocator&>(platform().core_mem_alloc()); }


void Vm_session_component::_exception_handler(Signal_context_capability handler, Vcpu_id)
{
	if (!create(_ds_addr, Capability_space::capid(handler),
	            cma().phys_addr(&_table)))
		Genode::warning("Cannot instantiate vm kernel object, invalid signal context?");
}


void Vm_session_component::_attach(addr_t phys_addr, addr_t vm_addr, size_t size)
{
	using namespace Hw;

	Page_flags pflags { RW, NO_EXEC, USER, NO_GLOBAL, RAM, CACHED };

	try {
		_table.insert_translation(vm_addr, phys_addr, size, pflags,
		                          _table_array.alloc());
		return;
	} catch(Hw::Out_of_tables &) {
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

		/* unsupported - deny otherwise arbitrary physical memory can be mapped to a VM */
		if (dsc->managed())
			throw Invalid_dataspace();

		_attach(dsc->phys_addr(), vm_addr, dsc->size());
	});
}


void Vm_session_component::attach_pic(addr_t vm_addr)
{
	_attach(Board::IRQ_CONTROLLER_VT_CPU_BASE, vm_addr,
	        Board::IRQ_CONTROLLER_VT_CPU_SIZE);
}


void Vm_session_component::detach(addr_t vm_addr, size_t size) {
	_table.remove_translation(vm_addr, size, _table_array.alloc()); }



void * Vm_session_component::_alloc_table()
{
	void * table;
	/* get some aligned space for the translation table */
	if (!cma().alloc_aligned(sizeof(Table), (void**)&table,
	                         Table::ALIGNM_LOG2).ok()) {
		error("failed to allocate kernel object");
		throw Insufficient_ram_quota();
	}
	return table;
}


Vm_session_component::Vm_session_component(Rpc_entrypoint &ds_ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram_alloc,
                                           Region_map &region_map)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ds_ep(&ds_ep),
	_constrained_md_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
	_region_map(region_map),
	_table(*construct_at<Table>(_alloc_table())),
	_table_array(*(new (cma()) Array([this] (void * virt) {
		return (addr_t)cma().phys_addr(virt);})))
{
	_ds_cap = _constrained_md_ram_alloc.alloc(_ds_size(), Genode::Cache_attribute::UNCACHED);

	try {
		_ds_addr = region_map.attach(_ds_cap);
	} catch (...) {
		_constrained_md_ram_alloc.free(_ds_cap);
		throw;
	}
}


Vm_session_component::~Vm_session_component()
{
	/* free region in allocator */
	if (_ds_cap.valid()) {
		_region_map.detach(_ds_addr);
		_constrained_md_ram_alloc.free(_ds_cap);
	}

	/* free guest-to-host page tables */
	destroy(platform().core_mem_alloc(), &_table);
	destroy(platform().core_mem_alloc(), &_table_array);
}
