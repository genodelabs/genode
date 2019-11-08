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

#include <vm_session/client.h>

using namespace Genode;

Vm_session::Vcpu_id
Vm_session_client::create_vcpu(Allocator &, Env &, Vm_handler_base &handler)
{
	Thread * ep = reinterpret_cast<Thread *>(&handler._rpc_ep);
	Vcpu_id vcpu_id { call<Rpc_create_vcpu>(ep->cap()) };
	call<Rpc_exception_handler>(handler._cap, vcpu_id);
	return vcpu_id;
}

void Vm_session_client::run(Vcpu_id const vcpu_id)
{
	call<Rpc_run>(vcpu_id);
}

void Vm_session_client::pause(Vcpu_id const vcpu_id)
{
	call<Rpc_pause>(vcpu_id);
}

Dataspace_capability Vm_session_client::cpu_state(Vcpu_id const vcpu_id)
{
	return call<Rpc_cpu_state>(vcpu_id);
}

Vm_session::~Vm_session()
{ }
