/*
 * \brief  Core-specific instance of the VM session interface
 * \author Stefan Kalkowski
 * \date   2015-06-03
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
#include <vm_session_component.h>
#include <platform.h>

using namespace Genode;

static Board::Vm_page_table_array & dummy_array()
{
	static Board::Vm_page_table_array a;
	return a;
}


void Vm_session_component::_attach(addr_t, addr_t, size_t) { }


void Vm_session_component::_attach_vm_memory(Dataspace_component &,
                                             addr_t const,
                                             Attach_attr const) { }


void Vm_session_component::attach_pic(addr_t) { }


void Vm_session_component::_detach_vm_memory(addr_t, size_t) { }


void * Vm_session_component::_alloc_table()
{
	static Board::Vm_page_table table;
	return (void*) &table;
}


Vm_session_component::Vm_session_component(Rpc_entrypoint  &ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram_alloc,
                                           Region_map &region_map,
                                           unsigned, Trace::Source_registry &)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_constrained_md_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
	_sliced_heap(_constrained_md_ram_alloc, region_map),
	_region_map(region_map),
  _table(*construct_at<Board::Vm_page_table>(_alloc_table())),
  _table_array(dummy_array()) { }


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
}
