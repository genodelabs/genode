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
#include <kernel/core_interface.h>
#include <vm_session_component.h>
#include <platform.h>
#include <cpu_thread_component.h>
#include <core_env.h>

using namespace Genode;


size_t Vm_session_component::_ds_size() {
	return align_addr(sizeof(Board::Vm_state), get_page_size_log2()); }


addr_t Vm_session_component::_alloc_ds()
{
	addr_t addr;
	if (platform().ram_alloc().alloc_aligned(_ds_size(), (void**)&addr,
		                                     get_page_size_log2()).error())
		throw Insufficient_ram_quota();
	return addr;
}


void Vm_session_component::_run(Vcpu_id) { }


void Vm_session_component::_pause(Vcpu_id) { }


Capability<Vm_session::Native_vcpu> Vm_session_component::_native_vcpu(Vcpu_id id)
{
	if (!_valid_id(id)) { return Capability<Vm_session::Native_vcpu>(); }
	return reinterpret_cap_cast<Vm_session::Native_vcpu>(_vcpus[id.id].kobj.cap());
}


void Vm_session_component::_exception_handler(Signal_context_capability handler,
                                              Vcpu_id id)
{
	if (!_valid_id(id)) {
		Genode::warning("invalid vcpu id ", id.id);
		return;
	}

	Vcpu & vcpu = _vcpus[id.id];
	if (vcpu.kobj.constructed()) {
		Genode::warning("Cannot register vcpu handler twice");
		return;
	}

	unsigned const cpu = vcpu.location.valid() ? vcpu.location.xpos() : 0;

	if (!vcpu.kobj.create(cpu, vcpu.ds_addr, Capability_space::capid(handler), _id))
		Genode::warning("Cannot instantiate vm kernel object, ",
		                "invalid signal context?");
}


Vm_session::Vcpu_id Vm_session_component::_create_vcpu(Thread_capability tcap)
{
	using namespace Genode;

	if (_vcpu_id_alloc == Board::VCPU_MAX) return Vcpu_id{Vcpu_id::INVALID};

	Affinity::Location vcpu_location;
	auto lambda = [&] (Cpu_thread_component *ptr) {
		if (!ptr) return;
		vcpu_location = ptr->platform_thread().affinity();
	};
	_ep.apply(tcap, lambda);

	Vcpu & vcpu = _vcpus[_vcpu_id_alloc];
	vcpu.ds_cap = _constrained_md_ram_alloc.alloc(_ds_size(),
	                                              Cache_attribute::UNCACHED);
	try {
		vcpu.ds_addr = _region_map.attach(vcpu.ds_cap);
	} catch (...) {
		_constrained_md_ram_alloc.free(vcpu.ds_cap);
		throw;
	}

	vcpu.location = vcpu_location;
	return Vcpu_id { _vcpu_id_alloc++ };
}


Genode::Dataspace_capability
Vm_session_component::_cpu_state(Vm_session::Vcpu_id id)
{
	return (_valid_id(id)) ? _vcpus[id.id].ds_cap
	                       : Genode::Ram_dataspace_capability();
}
