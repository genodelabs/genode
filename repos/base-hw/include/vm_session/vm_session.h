 /*
 * \brief  VM-session interface
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

			class Invalid_dataspace : Exception { };

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

			/**
			 * Stop execution of the VM
			 */
			virtual void pause(void) {}

			/**
			 * Attach dataspace to the guest-physical memory address space
			 *
			 * \param ds       dataspace to be attached
			 * \param vm_addr  address in guest-physical memory address space
			 */
			virtual void attach(Dataspace_capability ds, addr_t vm_addr) = 0;

			/**
			 * Invalidate region of the guest-physical memory address space
			 *
			 * \param vm_addr  address in guest-physical memory address space
			 * \param size     size of the region to invalidate
			 */
			virtual void detach(addr_t vm_addr, size_t size) = 0;

			/**
			 * Attach cpu-local interrupt-controller's interface to
			 * guest-physical memory address space.
			 *
			 * \param vm_addr  address in guest-physical memory address space
			 *
			 * Note: this is currently only support for ARM interrupt-controller
			 *       hardware virtualization
			 */
			virtual void attach_pic(addr_t vm_addr) = 0;


			/*********************
			 ** RPC declaration **
			 *********************/

			GENODE_RPC(Rpc_cpu_state, Dataspace_capability, cpu_state);
			GENODE_RPC(Rpc_exception_handler, void, exception_handler,
			           Signal_context_capability);
			GENODE_RPC(Rpc_run, void, run);
			GENODE_RPC(Rpc_pause, void, pause);
			GENODE_RPC_THROW(Rpc_attach, void, attach,
			                 GENODE_TYPE_LIST(Invalid_dataspace),
		                     Dataspace_capability, addr_t);
			GENODE_RPC(Rpc_detach, void, detach, addr_t, size_t);
			GENODE_RPC(Rpc_attach_pic, void, attach_pic, addr_t);
			GENODE_RPC_INTERFACE(Rpc_cpu_state, Rpc_exception_handler,
			                     Rpc_run, Rpc_pause, Rpc_attach, Rpc_detach,
			                     Rpc_attach_pic);
	};
}

#endif /* _INCLUDE__VM_SESSION__VM_SESSION_H_ */
