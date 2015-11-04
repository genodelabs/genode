/*
 * \brief  Client-side region manager session interface
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \date   2006-07-11
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__CLIENT_H_
#define _INCLUDE__RM_SESSION__CLIENT_H_

#include <rm_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Rm_session_client; }


struct Genode::Rm_session_client : Rpc_client<Rm_session>
{
	/*
	 * Multiple calls to get the dataspace capability on NOVA lead to the
	 * situation that the caller gets each time a new mapping of the same
	 * capability at different indexes. But the client/caller assumes to get
	 * every time the very same index, e.g. in Noux the index is used to look
	 * up data structures attached to the capability. Therefore, we cache the
	 * dataspace capability on the first request.
	 */
	Dataspace_capability _rm_ds_cap;

	explicit Rm_session_client(Rm_session_capability session)
	: Rpc_client<Rm_session>(session) { }

	Local_addr attach(Dataspace_capability ds, size_t size = 0,
	                  off_t offset = 0, bool use_local_addr = false,
	                  Local_addr local_addr = (void *)0,
	                  bool executable = false) override
	{
		return call<Rpc_attach>(ds, size, offset,
		                        use_local_addr, local_addr,
		                        executable);
	}

	void detach(Local_addr local_addr) override {
		call<Rpc_detach>(local_addr); }

	Pager_capability add_client(Thread_capability thread) override {
		return call<Rpc_add_client>(thread); }

	void remove_client(Pager_capability pager) override {
		call<Rpc_remove_client>(pager); }

	void fault_handler(Signal_context_capability handler) override {
		call<Rpc_fault_handler>(handler); }

	State state() override {
		return call<Rpc_state>(); }

	Dataspace_capability dataspace() override
	{
		if (!_rm_ds_cap.valid())
			_rm_ds_cap = call<Rpc_dataspace>();
		return _rm_ds_cap;
	}
};

#endif /* _INCLUDE__RM_SESSION__CLIENT_H_ */
