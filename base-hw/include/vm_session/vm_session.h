 /*
 * \brief  VM-session interface
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VM_SESSION__VM_SESSION_H_
#define _INCLUDE__VM_SESSION__VM_SESSION_H_

/* Genode includes */
#include <base/rpc_args.h>
#include <base/signal.h>
#include <session/session.h>
#include <ram_session/ram_session.h>

namespace Genode {

	struct Vm_session : Session
	{
			static const char *service_name() { return "VM"; }

			/**
			 * Destructor
			 */
			virtual ~Vm_session() { }

			/**
			 * Get dataspace of the CPU state of the VM
			 */
			virtual Dataspace_capability cpu_state(void) = 0;

			/**
			 * Register signal handler for exceptions of the Vm
			 */
			virtual void exception_handler(Signal_context_capability handler) = 0;

			/**
			 * (Re-)Start execution of the VM
			 */
			virtual void run(void) {}

			/*********************
			 ** RPC declaration **
			 *********************/

			GENODE_RPC(Rpc_cpu_state, Dataspace_capability, cpu_state);
			GENODE_RPC(Rpc_exception_handler, void, exception_handler,
			           Signal_context_capability);
			GENODE_RPC(Rpc_run, void, run);
			GENODE_RPC_INTERFACE(Rpc_cpu_state, Rpc_exception_handler, Rpc_run);
	};
}

#endif /* _INCLUDE__VM_SESSION__VM_SESSION_H_ */
