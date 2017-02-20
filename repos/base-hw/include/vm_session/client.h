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
#include <base/rpc_client.h>

namespace Genode
{
	/**
	 * Client-side VM session interface
	 */
	struct Vm_session_client : Rpc_client<Vm_session>
	{
		/**
		 * Constructor
		 */
		explicit Vm_session_client(Vm_session_capability session)
		: Rpc_client<Vm_session>(session) { }


		/**************************
		 ** Vm_session interface **
		 **************************/

		Dataspace_capability cpu_state() {
			return call<Rpc_cpu_state>(); }

		void exception_handler(Signal_context_capability handler) {
			call<Rpc_exception_handler>(handler); }

		void run()   { call<Rpc_run>();   }
		void pause() { call<Rpc_pause>(); }

		void attach(Dataspace_capability ds,addr_t vm_addr) {
			call<Rpc_attach>(ds, vm_addr); }

		void detach(addr_t vm_addr, size_t size) {
			call<Rpc_detach>(vm_addr, size); }

		void attach_pic(addr_t vm_addr) {
			call<Rpc_attach_pic>(vm_addr); }
	};
}

#endif /* _INCLUDE__VM_SESSION__CLIENT_H_ */
