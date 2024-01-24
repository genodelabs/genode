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

using namespace Core;


size_t Vm_session_component::_ds_size() {
	return align_addr(sizeof(Board::Vcpu_state), get_page_size_log2()); }


void Vm_session_component::Vcpu::exception_handler(Signal_context_capability handler)
{
	if (!handler.valid()) {
		warning("invalid signal");
		return;
	}

	if (kobj.constructed()) {
		warning("Cannot register vcpu handler twice");
		return;
	}

	unsigned const cpu = location.xpos();

	if (!kobj.create(cpu, ds_addr, Capability_space::capid(handler), id))
		warning("Cannot instantiate vm kernel object, invalid signal context?");
}


Capability<Vm_session::Native_vcpu> Vm_session_component::create_vcpu(Thread_capability const tcap)
{
	if (_vcpu_id_alloc == Board::VCPU_MAX) return { };

	Affinity::Location vcpu_location;
	_ep.apply(tcap, [&] (Cpu_thread_component *ptr) {
		if (!ptr) return;
		vcpu_location = ptr->platform_thread().affinity();
	});

	if (_vcpus[_vcpu_id_alloc].constructed())
		return { };

	_vcpus[_vcpu_id_alloc].construct(_id, _ep);
	Vcpu & vcpu = *_vcpus[_vcpu_id_alloc];

	try {
		vcpu.ds_cap = _constrained_md_ram_alloc.alloc(_ds_size(), Cache::UNCACHED);
		vcpu.ds_addr = _alloc_vcpu_data(_region_map.attach(vcpu.ds_cap));
	} catch (...) {
		if (vcpu.ds_cap.valid())
			_constrained_md_ram_alloc.free(vcpu.ds_cap);
		_vcpus[_vcpu_id_alloc].destruct();
		throw;
	}

	vcpu.location = vcpu_location;

	_vcpu_id_alloc ++;
	return vcpu.cap();
}
