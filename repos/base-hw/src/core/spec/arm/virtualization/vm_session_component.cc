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
#include <cpu_thread_component.h>
#include <core_env.h>

using namespace Genode;

static Core_mem_allocator & cma() {
	return static_cast<Core_mem_allocator&>(platform().core_mem_alloc()); }


void Vm_session_component::_attach(addr_t phys_addr, addr_t vm_addr, size_t size)
{
	using namespace Hw;

	Page_flags pflags { RW, NO_EXEC, USER, NO_GLOBAL, RAM, CACHED };

	try {
		_table.insert_translation(vm_addr, phys_addr, size, pflags,
		                          _table_array.alloc());
		Kernel::Vm::invalidate_tlb_vm();
		return;
	} catch(Hw::Out_of_tables &) {
		Genode::error("Translation table needs to much RAM");
	} catch(...) {
		Genode::error("Invalid mapping ", Genode::Hex(phys_addr), " -> ",
		              Genode::Hex(vm_addr), " (", size, ")");
	}
}


void Vm_session_component::_attach_vm_memory(Dataspace_component &dsc,
                                             addr_t const vm_addr,
                                             Attach_attr const attribute)
{
	_attach(dsc.phys_addr() + attribute.offset, vm_addr, attribute.size);
}


void Vm_session_component::attach_pic(addr_t vm_addr)
{
	_attach(Board::Cpu_mmio::IRQ_CONTROLLER_VT_CPU_BASE, vm_addr,
	        Board::Cpu_mmio::IRQ_CONTROLLER_VT_CPU_SIZE);
}


void Vm_session_component::_detach_vm_memory(addr_t vm_addr, size_t size)
{
    _table.remove_translation(vm_addr, size, _table_array.alloc());
    Kernel::Vm::invalidate_tlb_vm();
}


void * Vm_session_component::_alloc_table()
{
	void * table;
	/* get some aligned space for the translation table */
	if (!cma().alloc_aligned(sizeof(Board::Vm_page_table), (void**)&table,
	                         Board::Vm_page_table::ALIGNM_LOG2).ok()) {
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
                                           Region_map &region_map,
                                           unsigned,
                                           Trace::Source_registry &)
: Ram_quota_guard(resources.ram_quota),
  Cap_quota_guard(resources.cap_quota),
  _ep(ds_ep),
  _constrained_md_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
  _sliced_heap(_constrained_md_ram_alloc, region_map),
  _region_map(region_map),
  _table(*construct_at<Board::Vm_page_table>(_alloc_table())),
  _table_array(*(new (cma()) Board::Vm_page_table_array([this] (void * virt) {
	return (addr_t)cma().phys_addr(virt);})))
{
	/* configure managed VM area */
	_map.add_range(0, 0UL - 0x1000);
	_map.add_range(0UL - 0x1000, 0x1000);
}


Vm_session_component::~Vm_session_component()
{
	/* detach all regions */
	while (true) {
		addr_t out_addr = 0;

		if (!_map.any_block_addr(&out_addr))
			break;

		detach(out_addr);
	}

	/* free region in allocator */
	for (unsigned i = 0; i < _id_alloc; i++) {
		Vcpu & vcpu = _vcpus[i];
		if (vcpu.ds_cap.valid()) {
			_region_map.detach(vcpu.ds_addr);
			_constrained_md_ram_alloc.free(vcpu.ds_cap);
		}
	}

	/* free guest-to-host page tables */
	destroy(platform().core_mem_alloc(), &_table);
	destroy(platform().core_mem_alloc(), &_table_array);
}
