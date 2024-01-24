/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2015-02-17
 */

/*
 * Copyright (C) 2015-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>

/* base internal includes */
#include <base/internal/unmanaged_singleton.h>

/* core includes */
#include <kernel/core_interface.h>
#include <vm_session_component.h>
#include <platform.h>
#include <cpu_thread_component.h>
#include <core_env.h>

using namespace Core;


static Core_mem_allocator & cma() {
	return static_cast<Core_mem_allocator&>(platform().core_mem_alloc()); }


void Vm_session_component::_attach(addr_t phys_addr, addr_t vm_addr, size_t size)
{
	using namespace Hw;

	Page_flags pflags { RW, EXEC, USER, NO_GLOBAL, RAM, CACHED };

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


void Vm_session_component::_attach_vm_memory(Dataspace_component &dsc,
                                             addr_t const vm_addr,
                                             Attach_attr const attribute)
{
	_attach(dsc.phys_addr() + attribute.offset, vm_addr, attribute.size);
}


void Vm_session_component::attach_pic(addr_t )
{ }


void Vm_session_component::_detach_vm_memory(addr_t vm_addr, size_t size)
{
	_table.remove_translation(vm_addr, size, _table_array.alloc());
}


void * Vm_session_component::_alloc_table()
{
	/* get some aligned space for the translation table */
	return cma().alloc_aligned(sizeof(Board::Vm_page_table),
	                           Board::Vm_page_table::ALIGNM_LOG2).convert<void *>(
		[&] (void *table_ptr) {
			return table_ptr; },

		[&] (Range_allocator::Alloc_error) -> void * {
			/* XXX handle individual error conditions */
			error("failed to allocate kernel object");
			throw Insufficient_ram_quota(); }
	);
}


using Vmid_allocator = Genode::Bit_allocator<256>;

static Vmid_allocator &alloc()
{
	static Vmid_allocator * allocator = nullptr;
	if (!allocator) {
		allocator = unmanaged_singleton<Vmid_allocator>();

		/* reserve VM ID 0 for the hypervisor */
		addr_t id = allocator->alloc();
		assert (id == 0);
	}
	return *allocator;
}


Genode::addr_t Vm_session_component::_alloc_vm_data(Genode::addr_t ds_addr)
{
	void * vm_data_ptr = cma()
	                     .alloc_aligned(sizeof(Board::Vm_data), 12)
                             .convert<void *>(
	                                      [&](void *ptr) { return ptr; },
	                                      [&](Range_allocator::Alloc_error) -> void * {
	                                              /* XXX handle individual error conditions */
	                                              error("failed to allocate kernel object");
	                                              throw Insufficient_ram_quota();
	                                          }
	);

	Genode::Vm_data* vm_data = (Genode::Vm_data *) vm_data_ptr;
	vm_data->vcpu_state = (Genode::Vcpu_state *) ds_addr;
	vm_data->vmcb_phys_addr = (addr_t)cma().phys_addr(vm_data->vmcb);
	return (Genode::addr_t) vm_data_ptr;
}


Vm_session_component::Vm_session_component(Rpc_entrypoint &ds_ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram_alloc,
                                           Region_map &region_map,
                                           unsigned,
                                           Trace::Source_registry &)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ds_ep),
	_constrained_md_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
	_sliced_heap(_constrained_md_ram_alloc, region_map),
	_region_map(region_map),
	_table(*construct_at<Board::Vm_page_table>(_alloc_table())),
	_table_array(*(new (cma()) Board::Vm_page_table_array([] (void * virt) {
	                           return (addr_t)cma().phys_addr(virt);}))),
	_id({(unsigned)alloc().alloc(), cma().phys_addr(&_table)})
{
	/* configure managed VM area */
	_map.add_range(0UL, ~0UL);
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
	for (unsigned i = 0; i < _vcpu_id_alloc; i++) {
		if (!_vcpus[i].constructed())
			continue;

		Vcpu & vcpu = *_vcpus[i];
		if (vcpu.ds_cap.valid()) {
			_region_map.detach(vcpu.ds_addr);
			_constrained_md_ram_alloc.free(vcpu.ds_cap);
		}
	}

	/* free guest-to-host page tables */
	destroy(platform().core_mem_alloc(), &_table);
	destroy(platform().core_mem_alloc(), &_table_array);
	alloc().free(_id.id);
}
