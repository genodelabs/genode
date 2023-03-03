/*
 * \brief  Connection to a VM service
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2012-10-02
 *
 * The VM connection is the API for VM and vCPU handling and implemented
 * for each platform specifically.
 */

/*
 * Copyright (C) 2012-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VM_SESSION__CONNECTION_H_
#define _INCLUDE__VM_SESSION__CONNECTION_H_

#include <base/connection.h>
#include <base/rpc_client.h>
#include <vm_session/vm_session.h>
#include <cpu_session/cpu_session.h>
#include <util/retry.h>
#include <util/noncopyable.h>

namespace Genode {
	struct Vm_connection;
	struct Vcpu_handler_base;
	struct Vcpu_state;
	struct Allocator;
}


struct Genode::Vm_connection : Connection<Vm_session>, Rpc_client<Vm_session>
{
	/*
	 * VM-Exit state-transfer configuration
	 *
	 * Per default all Vcpu_state is transfered on each exit reason. The exit
	 * config enables omission of some state transfers on specific exit
	 * reasons.
	 */
	struct Exit_config
	{
		/* for example OMIT_FPU_ON_IRQ */
	};

	/**
	 * Virtual CPU
	 *
	 * A vCPU can be created only from a Vm_connection and is, thus, bounded to
	 * a VM until destroyed.
	 */
	struct Vcpu : Genode::Noncopyable
	{
		Native_vcpu &_native_vcpu;

		Vcpu(Vm_connection &, Allocator &, Vcpu_handler_base &, Exit_config const &);

		void run();
		void pause();
		Vcpu_state & state();
	};

	friend class Vcpu;

	/**
	 * Constructor
	 *
	 * \param priority  designated priority of the VM
	 * \param affinity  which physical CPU the VM should run on top of
	 */
	Vm_connection(Env &env, Label const &label = Label(),
	              long priority = Cpu_session::DEFAULT_PRIORITY,
	              unsigned long affinity = 0)
	:
		Connection<Vm_session>(env, label, Ram_quota { 16*1024 }, Affinity(),
		                       Args("priority=", Hex(priority), ", "
		                            "affinity=", Hex(affinity))),
		Rpc_client<Vm_session>(cap())
	{ }

	template <typename FUNC>
	auto with_upgrade(FUNC func) -> decltype(func())
	{
		return Genode::retry<Genode::Out_of_ram>(
			[&] () {
				return Genode::retry<Genode::Out_of_caps>(
					[&] () { return func(); },
					[&] () { this->upgrade_caps(2); });
			},
			[&] () { this->upgrade_ram(4096); }
		);
	}


	/**************************
	 ** Vm_session interface **
	 **************************/

	void attach(Dataspace_capability ds, addr_t vm_addr, Attach_attr attr) override
	{
		with_upgrade([&] () {
			call<Rpc_attach>(ds, vm_addr, attr); });
	}

	void detach(addr_t vm_addr, size_t size) override {
		call<Rpc_detach>(vm_addr, size); }

	void attach_pic(addr_t vm_addr) override {
		call<Rpc_attach_pic>(vm_addr); }
};

#endif /* _INCLUDE__VM_SESSION__CONNECTION_H_ */
