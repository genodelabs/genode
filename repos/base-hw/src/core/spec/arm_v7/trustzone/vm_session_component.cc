/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>

/* core includes */
#include <vm_session_component.h>
#include <platform.h>

using namespace Core;


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


static unsigned id_alloc = 0;


Vm_session_component::Vm_session_component(Registry<Revoke> &registry,
                                           Vmid_allocator &vmids,
                                           Rpc_entrypoint &ep,
                                           Resources resources,
                                           Label const &label,
                                           Diag diag,
                                           Ram_allocator &ram_alloc,
                                           Local_rm &local_rm,
                                           unsigned, Trace::Source_registry &)
:
	Session_object(ep, resources, label, diag),
	_elem(registry, *this),
	_ep(ep),
	_ram(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
	_sliced_heap(_ram, local_rm),
	_local_rm(local_rm),
	_table(*construct_at<Board::Vm_page_table>(_alloc_table())),
	_table_array(dummy_array()),
	_vmid_alloc(vmids),
	_id({id_alloc++, nullptr})
{
	if (_id.id) {
		error("Only one TrustZone VM available!");
		throw Service_denied();
	}
}


Vm_session_component::~Vm_session_component()
{
	/* detach all regions */
	while (true) {
		addr_t out_addr = 0;

		if (!_map.any_block_addr(&out_addr))
			break;

		detach_at(out_addr);
	}

	/* free region in allocator */
	for (unsigned i = 0; i < _vcpu_id_alloc; i++) {
		if (!_vcpus[i].constructed())
			continue;

		Vcpu &vcpu = *_vcpus[i];
		if (vcpu.state().valid())
			_local_rm.detach(vcpu.ds_addr);
	}

	id_alloc--;
}
