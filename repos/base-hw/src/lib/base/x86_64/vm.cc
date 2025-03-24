/*
 * \brief  Client-side VM session interface (x86_64 specific)
 * \author Alexander Boettcher
 * \author Benjamin Lamowski
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator.h>
#include <base/attached_dataspace.h>
#include <base/env.h>
#include <base/registry.h>
#include <base/signal.h>
#include <base/sleep.h>
#include <base/internal/capability_space.h>

#include <kernel/interface.h>

#include <spec/x86/cpu/vcpu_state.h>

#include <vm_session/connection.h>
#include <vm_session/handler.h>

#include <hw_native_vcpu/hw_native_vcpu.h>

using namespace Genode;

using Exit_config = Vm_connection::Exit_config;


/****************************
 ** hw vCPU implementation **
 ****************************/

struct Hw_vcpu : Rpc_client<Vm_session::Native_vcpu>, Noncopyable
{
	private:
		enum Initial_exitcode : unsigned { EXIT_STARTUP = 0xfe };

		Attached_dataspace      _state;
		Native_capability       _kernel_vcpu { };
		Vcpu_handler_base &     _vcpu_handler;
		unsigned                _id { 0 };
		void                   *_ep_handler    { nullptr };

		Vcpu_state & _local_state() { return *_state.local_addr<Vcpu_state>(); }
		Capability<Native_vcpu> _create_vcpu(Vm_connection &,
		                                     Vcpu_handler_base &);

	public:

		void run();
		const Hw_vcpu& operator=(const Hw_vcpu &) = delete;
		Hw_vcpu(const Hw_vcpu&) = delete;

		Hw_vcpu(Env &, Vm_connection &, Vcpu_handler_base &);

		void with_state(auto const &fn);

};


Hw_vcpu::Hw_vcpu(Env &env, Vm_connection &vm, Vcpu_handler_base &handler)
:
	Rpc_client<Native_vcpu>(_create_vcpu(vm, handler)),
	_state(env.rm(), vm.with_upgrade([&] { return call<Rpc_state>(); })),
	_vcpu_handler(handler)
{
	static unsigned counter = 0;
	call<Rpc_exception_handler>(handler.signal_cap());
	_kernel_vcpu = call<Rpc_native_vcpu>();
	_id = counter++;
	_ep_handler = reinterpret_cast<Thread *>(&handler.rpc_ep());

	/*
	 * Set the startup exit for the initial signal to the VMM's Vcpu_handler
	 */
        _local_state().exit_reason = EXIT_STARTUP;
}


void Hw_vcpu::run()
{
	Kernel::run_vm(Capability_space::capid(_kernel_vcpu));
}


void Hw_vcpu::with_state(auto const &fn)
{
	if (Thread::myself() != _ep_handler) {
		error("vCPU state requested outside of vcpu_handler EP");
		sleep_forever();
	}
	Kernel::pause_vm(Capability_space::capid(_kernel_vcpu));

	if (fn(_local_state()))
		run();
}


Capability<Vm_session::Native_vcpu> Hw_vcpu::_create_vcpu(Vm_connection     &vm,
                                                          Vcpu_handler_base &handler)
{
	Thread &tep { *reinterpret_cast<Thread *>(&handler.rpc_ep()) };

	return vm.create_vcpu(tep.cap());
}


/**************
 ** vCPU API **
 **************/

void Vm_connection::Vcpu::_with_state(With_state::Ft const &fn)
{
	static_cast<Hw_vcpu &>(_native_vcpu).with_state(fn);
}


Vm_connection::Vcpu::Vcpu(Vm_connection &vm, Allocator &alloc,
                          Vcpu_handler_base &handler, Exit_config const &)
:
	_native_vcpu(*new (alloc) Hw_vcpu(vm._env, vm, handler))
{
	/*
	 * Send the initial startup signal to the vCPU handler.
	 */
	Signal_transmitter(handler.signal_cap()).submit();
}
