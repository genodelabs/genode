/*
 * \brief  Client-side VM session interface
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VM_SESSION__CLIENT_H_
#define _INCLUDE__VM_SESSION__CLIENT_H_

/* Genode includes */
#include <vm_session/capability.h>
#include <vm_session/handler.h>
#include <base/rpc_client.h>

namespace Genode { struct Vm_session_client; class Allocator; class Vm_state; }

/**
 * Client-side VM session interface
 */
struct Genode::Vm_session_client : Rpc_client<Vm_session>
{
	/**
	 * Constructor
	 */
	explicit Vm_session_client(Vm_session_capability session)
	: Rpc_client<Vm_session>(session) { }


	/**************************
	 ** Vm_session interface **
	 **************************/

	Dataspace_capability cpu_state(Vcpu_id);

	void run(Vcpu_id);
	void pause(Vcpu_id);

	void attach(Dataspace_capability ds, addr_t vm_addr,
	            Attach_attr attr = { .offset = 0,
	                                 .size = 0,
	                                 .executable = true,
	                                 .writeable = true } ) override
	{
		call<Rpc_attach>(ds, vm_addr, attr);
	}

	void detach(addr_t vm_addr, size_t size) override {
		call<Rpc_detach>(vm_addr, size); }

	void attach_pic(addr_t vm_addr) override {
		call<Rpc_attach_pic>(vm_addr); }

	Vcpu_id create_vcpu(Allocator &, Env &, Vm_handler_base &);
};

#endif /* _INCLUDE__VM_SESSION__CLIENT_H_ */
