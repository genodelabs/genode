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

	sigh_cap = handler;

	unsigned const cpu = location.xpos();

	if (!kobj.create(cpu, (void *)ds_addr, Capability_space::capid(handler), id))
		Genode::warning("Cannot instantiate vm kernel object, invalid signal context?");
}


void Vm_session_component::Vcpu::revoke_signal_context(Signal_context_capability cap)
{
	if (cap != sigh_cap)
		return;

	sigh_cap = Signal_context_capability();
	if (kobj.constructed()) kobj.destruct();
}


void Vm_session_component::attach(Dataspace_capability const cap,
                                  addr_t const guest_phys,
                                  Attach_attr attribute)
{
	Attach_result ret =
		_memory.attach(cap, guest_phys, attribute,
		               [&] (addr_t vm_addr, addr_t phys_addr, size_t size,
		                    bool exec, bool write, Cache cacheable) {
			return _attach_vm_memory(vm_addr, phys_addr, size, exec,
			                         write, cacheable); });

	switch(ret) {
	case Attach_result::OK             : return;
	case Attach_result::INVALID_DS     : throw Invalid_dataspace();
	case Attach_result::OUT_OF_RAM     : throw Out_of_ram();
	case Attach_result::OUT_OF_CAPS    : throw Out_of_caps();
	case Attach_result::REGION_CONFLICT: throw Region_conflict();
	};
}


void Vm_session_component::detach(addr_t guest_phys, size_t size)
{
	_memory.detach(guest_phys, size, [&](addr_t vm_addr, size_t size) {
		_detach_vm_memory(vm_addr, size); });
}


void Vm_session_component::detach_at(addr_t const addr)
{
	_memory.detach_at(addr, [&](addr_t vm_addr, size_t size) {
		_detach_vm_memory(vm_addr, size); });
}


void Vm_session_component::reserve_and_flush(addr_t const addr)
{
	_memory.reserve_and_flush(addr, [&](addr_t vm_addr, size_t size) {
		_detach_vm_memory(vm_addr, size); });
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
	Vcpu &vcpu = *_vcpus[_vcpu_id_alloc];

	vcpu.ds.with_error([&] (Ram::Error e) { throw_exception(e); });

	try {
		Local_rm::Attach_attr attr { };
		attr.writeable = true;
		vcpu.ds_addr = _local_rm.attach(vcpu.state(), attr).convert<addr_t>(
			[&] (Local_rm::Attachment &a) {
				a.deallocate = false;
				return addr_t(a.ptr);
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
