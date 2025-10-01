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

using namespace Core;


static Core_mem_allocator & cma() {
	return static_cast<Core_mem_allocator&>(platform().core_mem_alloc()); }


Vm_session_component::Attach_result
Vm_session_component::_attach_vm_memory(addr_t vm_addr, addr_t phys_addr,
                                        size_t size, bool exec, bool write,
                                        Cache cache)
{
	using namespace Hw;

	Page_flags pflags { write ? RW : RO,
	                    exec ? EXEC : NO_EXEC,
	                    USER, NO_GLOBAL, RAM, cache };

	Attach_result result = Attach_result::OK;

	_table.insert(vm_addr, phys_addr, size,
	              pflags, _table_array.alloc()).with_error(
		[&] (Page_table_error e) {
			if (e == Page_table_error::INVALID_RANGE) {
				result = Attach_result::INVALID_DS;
				error("Invalid mapping ", Hex(phys_addr), " -> ",
				      Hex(vm_addr), " (", size, ")");
			} else {
				result = Attach_result::OUT_OF_RAM;
				error("Translation table needs to much RAM");
			}
		});
	return result;
}


void Vm_session_component::attach_pic(addr_t vm_addr)
{
	_attach_vm_memory(vm_addr, Board::Cpu_mmio::IRQ_CONTROLLER_VT_CPU_BASE,
	                  Board::Cpu_mmio::IRQ_CONTROLLER_VT_CPU_SIZE, false, true,
	                  CACHED);
}


void Vm_session_component::_detach_vm_memory(addr_t vm_addr, size_t size)
{
	_table.remove(vm_addr, size, _table_array.alloc());
}


void * Vm_session_component::_alloc_table()
{
	/* get some aligned space for the translation table */
	return cma().alloc_aligned(sizeof(Board::Vm_page_table),
	                           Board::Vm_page_table::ALIGNM_LOG2).convert<void *>(
		[&] (Range_allocator::Allocation &a) {
			a.deallocate = false;
			return a.ptr; },

		[&] (Alloc_error) -> void * {
			/* XXX handle individual error conditions */
			error("failed to allocate kernel object");
			throw Insufficient_ram_quota(); }
	);
}


Vm_session_component::Vm_session_component(Registry<Revoke> &registry,
                                           Vmid_allocator &vmid_alloc,
                                           Rpc_entrypoint &ds_ep,
                                           Resources resources,
                                           Label const &label,
                                           Diag diag,
                                           Ram_allocator &ram_alloc,
                                           Local_rm &local_rm,
                                           unsigned,
                                           Trace::Source_registry &)
:
	Session_object(ds_ep, resources, label, diag),
	_elem(registry, *this),
	_ep(ds_ep),
	_ram(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
	_local_rm(local_rm),
	_table(*construct_at<Board::Vm_page_table>(_alloc_table())),
	_table_array(*(new (cma()) Board::Vm_page_table_array([] (void * virt) {
	                           return (addr_t)cma().phys_addr(virt);}))),
	_vmid_alloc(vmid_alloc),
	_memory(ds_ep, *this, _ram, local_rm),
	_id({(unsigned)_vmid_alloc.alloc().convert<unsigned>(
		[&] (addr_t v) { return unsigned(v); },
		[&] (auto &) { error("vmid allocation failed"); return unsigned(0); }),
		cma().phys_addr(&_table)
	})
{ }


Vm_session_component::~Vm_session_component()
{
	for (unsigned i = 0; i < _vcpu_id_alloc; i++) {
		if (!_vcpus[i].constructed())
			continue;

		Vcpu &vcpu = *_vcpus[i];
		if (vcpu.state().valid())
			_local_rm.detach(vcpu.ds_addr);
	}

	/* free guest-to-host page tables */
	destroy(platform().core_mem_alloc(), &_table);
	destroy(platform().core_mem_alloc(), &_table_array);
	_vmid_alloc.free(_id.id);
}
