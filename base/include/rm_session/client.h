/*
 * \brief  Client-side region manager session interface
 * \author Christian Helmuth
 * \date   2006-07-11
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__CLIENT_H_
#define _INCLUDE__RM_SESSION__CLIENT_H_

#include <rm_session/capability.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Rm_session_client : Rpc_client<Rm_session>
	{
		explicit Rm_session_client(Rm_session_capability session)
		: Rpc_client<Rm_session>(session) { }

		Local_addr attach(Dataspace_capability ds, size_t size, off_t offset,
		                  bool use_local_addr, Local_addr local_addr,
		                  bool executable = false)
		{
			return call<Rpc_attach>(ds, size, offset,
			                        use_local_addr, local_addr,
			                        executable);
		}

		void detach(Local_addr local_addr) {
			call<Rpc_detach>(local_addr); }

		Pager_capability add_client(Thread_capability thread) {
			return call<Rpc_add_client>(thread); }

		void fault_handler(Signal_context_capability handler) {
			call<Rpc_fault_handler>(handler); }

		State state() {
			return call<Rpc_state>(); }

		Dataspace_capability dataspace() {
			return call<Rpc_dataspace>(); }
	};
}

#endif /* _INCLUDE__RM_SESSION__CLIENT_H_ */
