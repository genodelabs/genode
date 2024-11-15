/*
 * \brief  Monitored VM session
 * \author Norman Feske
 * \date   2023-07-04
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITORED_VM_H_
#define _MONITORED_VM_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <vm_session/connection.h>

/* local includes */
#include <monitored_thread.h>

namespace Monitor { struct Monitored_vm_session; }


struct Monitor::Monitored_vm_session : Monitored_rpc_object<Vm_session, Monitored_vm_session>
{
	using Monitored_rpc_object::Monitored_rpc_object;


	/**************************
	 ** Vm_session interface **
	 **************************/

	void attach(Dataspace_capability ds, addr_t at, Attach_attr attr) override
	{
		_real.call<Rpc_attach>(ds, at, attr);
	}

	void detach(addr_t vm_addr, size_t size) override
	{
		_real.call<Rpc_detach>(vm_addr, size);
	}

	void attach_pic(addr_t vm_addr) override
	{
		_real.call<Rpc_attach_pic>(vm_addr);
	}

	Capability<Native_vcpu> create_vcpu(Thread_capability thread_cap) override
	{
		Capability<Native_vcpu> result { };

		Monitored_thread::with_thread(_ep, thread_cap,
			[&] (Monitored_thread &monitored_thread) {
				result = _real.call<Rpc_create_vcpu>(monitored_thread._real);
			},
			[&] /* VM session not intercepted */ {
				result = _real.call<Rpc_create_vcpu>(thread_cap);
			});

		return result;
	}
};

#endif /* _MONITORED_VM_H_ */
