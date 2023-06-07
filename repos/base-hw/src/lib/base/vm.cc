/*
 * \brief  Client-side VM session interface (base-hw generic)
 * \author Alexander Boettcher
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator.h>
#include <base/attached_dataspace.h>
#include <base/env.h>
#include <base/registry.h>
#include <base/sleep.h>
#include <base/internal/capability_space.h>
#include <kernel/interface.h>

#include <vm_session/connection.h>
#include <vm_session/handler.h>

#include <hw_native_vcpu/hw_native_vcpu.h>

using namespace Genode;

using Exit_config = Vm_connection::Exit_config;
using Call_with_state = Vm_connection::Call_with_state;


/****************************
 ** hw vCPU implementation **
 ****************************/

struct Hw_vcpu : Rpc_client<Vm_session::Native_vcpu>, Noncopyable
{
	private:

		Attached_dataspace      _state;
		Native_capability       _kernel_vcpu { };
		void                   *_ep_handler    { nullptr };

		Capability<Native_vcpu> _create_vcpu(Vm_connection &, Vcpu_handler_base &);

		Vcpu_state & _local_state()
		{
			return *_state.local_addr<Vcpu_state>();
		}

	public:

		const Hw_vcpu& operator=(const Hw_vcpu &) = delete;
		Hw_vcpu(const Hw_vcpu&) = delete;

		Hw_vcpu(Env &, Vm_connection &, Vcpu_handler_base &);


		void with_state(Call_with_state &);
};


Hw_vcpu::Hw_vcpu(Env &env, Vm_connection &vm, Vcpu_handler_base &handler)
:
	Rpc_client<Native_vcpu>(_create_vcpu(vm, handler)),
	_state(env.rm(), vm.with_upgrade([&] () { return call<Rpc_state>(); }))
{
	_ep_handler = reinterpret_cast<Thread *>(&handler.rpc_ep());
	call<Rpc_exception_handler>(handler.signal_cap());
	_kernel_vcpu = call<Rpc_native_vcpu>();
}


void Hw_vcpu::with_state(Call_with_state &cw)
{
	if (Thread::myself() != _ep_handler) {
		error("vCPU state requested outside of vcpu_handler EP");
		sleep_forever();
	}
	Kernel::pause_vm(Capability_space::capid(_kernel_vcpu));

	if (cw.call_with_state(_local_state()))
		Kernel::run_vm(Capability_space::capid(_kernel_vcpu));
}


Capability<Vm_session::Native_vcpu> Hw_vcpu::_create_vcpu(Vm_connection     &vm,
                                                          Vcpu_handler_base &handler)
{
	Thread &tep { *reinterpret_cast<Thread *>(&handler.rpc_ep()) };

	return vm.with_upgrade([&] () {
		return vm.call<Vm_session::Rpc_create_vcpu>(tep.cap()); });
}


/**************
 ** vCPU API **
 **************/

void Vm_connection::Vcpu::_with_state(Call_with_state &cw) { static_cast<Hw_vcpu &>(_native_vcpu).with_state(cw); }


Vm_connection::Vcpu::Vcpu(Vm_connection &vm, Allocator &alloc,
                          Vcpu_handler_base &handler, Exit_config const &)
:
	_native_vcpu(*new (alloc) Hw_vcpu(vm._env, vm, handler))
{ }
