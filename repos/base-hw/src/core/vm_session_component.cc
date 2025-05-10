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
#include <vm_root.h>
#include <vm_session_component.h>
#include <platform.h>
#include <cpu_thread_component.h>

using namespace Core;


size_t Vm_session_component::Vcpu::_ds_size() {
	return align_addr(sizeof(Board::Vcpu_state), get_page_size_log2()); }


void Vm_session_component::Vcpu::exception_handler(Signal_context_capability handler)
{
	if (!handler.valid()) {
		Genode::warning("invalid signal");
		return;
	}

	if (kobj.constructed()) {
		Genode::warning("Cannot register vcpu handler twice");
		return;
	}

	unsigned const cpu = location.xpos();

	if (!kobj.create(cpu, (void *)ds_addr, Capability_space::capid(handler), id))
		Genode::warning("Cannot instantiate vm kernel object, invalid signal context?");
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

	_vcpus[_vcpu_id_alloc].construct(_ram, _id, _ep);
	Vcpu & vcpu = *_vcpus[_vcpu_id_alloc];

	vcpu.ds.with_error([&] (Ram::Error e) { throw_exception(e); });

	try {
		Local_rm::Attach_attr attr { };
		attr.writeable = true;
		vcpu.ds_addr = _local_rm.attach(vcpu.state(), attr).convert<addr_t>(
			[&] (Local_rm::Attachment &a) {
				a.deallocate = false;
				return _alloc_vcpu_data(addr_t(a.ptr));
			},
			[&] (Local_rm::Error) -> addr_t {
				error("failed to attach VCPU data within core");
				_vcpus[_vcpu_id_alloc].destruct();
				return 0;
			});
	} catch (...) {
		_vcpus[_vcpu_id_alloc].destruct();
		throw;
	}

	vcpu.location = vcpu_location;

	_vcpu_id_alloc ++;
	return vcpu.cap();
}


Core::Vm_root::Create_result Core::Vm_root::_create_session(const char *args)
{
	unsigned priority = 0;
	Arg a = Arg_string::find_arg(args, "priority");
	if (a.valid()) {
		priority = (unsigned)a.ulong_value(0);

		/* clamp priority value to valid range */
		priority = min((unsigned)Cpu_session::PRIORITY_LIMIT - 1, priority);
	}

	return *new (md_alloc())
		Vm_session_component(_registry,
		                     _vmid_alloc,
		                     *ep(),
		                     session_resources_from_args(args),
		                     session_label_from_args(args),
		                     session_diag_from_args(args),
		                     _ram_allocator, _local_rm, priority,
		                     _trace_sources);
}
