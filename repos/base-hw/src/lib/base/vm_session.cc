/*
 * \brief  Client-side VM session interface
 * \author Alexander Boettcher
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator.h>
#include <base/env.h>
#include <base/registry.h>
#include <base/internal/capability_space.h>
#include <kernel/interface.h>
#include <vm_session/client.h>

using namespace Genode;

struct Vcpu;

static Genode::Registry<Genode::Registered<Vcpu>> vcpus;


struct Vcpu
{
	Vm_session_client::Vcpu_id          const id;
	Capability<Vm_session::Native_vcpu> const cap;

	Vcpu(Vm_session::Vcpu_id id, Capability<Vm_session::Native_vcpu> cap)
	: id(id), cap(cap) { }

	virtual ~Vcpu() { }
};


Vm_session::Vcpu_id
Vm_session_client::create_vcpu(Allocator & alloc, Env &, Vm_handler_base & handler)
{
	Vcpu_id const id =
		call<Rpc_create_vcpu>(reinterpret_cast<Thread *>(&handler._rpc_ep)->cap());
	call<Rpc_exception_handler>(handler._cap, id);
	Vcpu * vcpu = new (alloc) Registered<Vcpu> (vcpus, id, call<Rpc_native_vcpu>(id));
	return vcpu->id;
}


void Vm_session_client::run(Vcpu_id const vcpu_id)
{
	vcpus.for_each([&] (Vcpu & vcpu) {
		if (vcpu.id.id != vcpu_id.id) { return; }
		Kernel::run_vm(Capability_space::capid(vcpu.cap));
	});
}


void Vm_session_client::pause(Vcpu_id const vcpu_id)
{
	vcpus.for_each([&] (Vcpu & vcpu) {
		if (vcpu.id.id != vcpu_id.id) { return; }
		Kernel::pause_vm(Capability_space::capid(vcpu.cap));
	});
}


Dataspace_capability Vm_session_client::cpu_state(Vcpu_id const vcpu_id)
{
	return call<Rpc_cpu_state>(vcpu_id);
}


Vm_session::~Vm_session()
{ }
