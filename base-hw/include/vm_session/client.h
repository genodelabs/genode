/*
 * \brief  Client-side vm session interface
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VM_SESSION__CLIENT_H_
#define _INCLUDE__VM_SESSION__CLIENT_H_

#include <vm_session/capability.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Vm_session_client : Rpc_client<Vm_session>
	{
		explicit Vm_session_client(Vm_session_capability session)
		: Rpc_client<Vm_session>(session) { }

		Dataspace_capability cpu_state() {
			return call<Rpc_cpu_state>(); }

		void exception_handler(Signal_context_capability handler) {
			call<Rpc_exception_handler>(handler); }

		void run() {
			call<Rpc_run>(); }
	};
}

#endif /* _INCLUDE__VM_SESSION__CLIENT_H_ */
