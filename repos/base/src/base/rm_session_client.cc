/*
 * \brief  Client-side region manager session interface
 * \author Norman Feske
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <rm_session/client.h>

using namespace Genode;

Rm_session_client::Rm_session_client(Rm_session_capability session)
: Rpc_client<Rm_session>(session) { }

Rm_session::Local_addr
Rm_session_client::attach(Dataspace_capability ds, size_t size, off_t offset,
                          bool use_local_addr, Local_addr local_addr,
                          bool executable)
{
	return call<Rpc_attach>(ds, size, offset, use_local_addr, local_addr,
	                        executable);
}

void Rm_session_client::detach(Local_addr local_addr) {
	call<Rpc_detach>(local_addr); }

Pager_capability Rm_session_client::add_client(Thread_capability thread)
{
	return call<Rpc_add_client>(thread);
}

void Rm_session_client::remove_client(Pager_capability pager) {
	call<Rpc_remove_client>(pager); }

void Rm_session_client::fault_handler(Signal_context_capability cap) {
	call<Rpc_fault_handler>(cap); }

	Rm_session::State Rm_session_client::state() { return call<Rpc_state>(); }

Dataspace_capability Rm_session_client::dataspace() { return call<Rpc_dataspace>(); }
