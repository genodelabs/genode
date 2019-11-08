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
#include <base/ram_allocator.h>
#include <cpu_session/cpu_session.h>
#include <session/session.h>

namespace Genode { struct Vm_session; }

struct Genode::Vm_session : Session
{
	static const char *service_name() { return "VM"; }

	struct Vcpu_id
	{
		enum { INVALID = ~0U };
		unsigned id { INVALID };
	};

	struct Attach_attr
	{
		addr_t offset;
		addr_t size;
		bool executable;
		bool writeable;
	};

	enum { CAP_QUOTA = 3 };

	class Invalid_dataspace : Exception { };
	class Region_conflict   : Exception { };

	/**
	 * Destructor
	 */
	virtual ~Vm_session();

	/**
	 * Attach dataspace to the guest-physical memory address space
	 *
	 * \param ds       dataspace to be attached
	 * \param vm_addr  address in guest-physical memory address space
	 */
	virtual void attach(Dataspace_capability ds, addr_t, Attach_attr) = 0;

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

	GENODE_RPC(Rpc_cpu_state, Dataspace_capability, _cpu_state, Vcpu_id);
	GENODE_RPC(Rpc_exception_handler, void, _exception_handler,
	           Signal_context_capability, Vcpu_id);
	GENODE_RPC(Rpc_run, void, _run, Vcpu_id);
	GENODE_RPC(Rpc_pause, void, _pause, Vcpu_id);
	GENODE_RPC_THROW(Rpc_attach, void, attach,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps, Region_conflict,
	                                  Invalid_dataspace),
	                 Dataspace_capability, addr_t, Attach_attr);
	GENODE_RPC(Rpc_detach, void, detach, addr_t, size_t);
	GENODE_RPC(Rpc_attach_pic, void, attach_pic, addr_t);
	GENODE_RPC_THROW(Rpc_create_vcpu, Vcpu_id, _create_vcpu,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 Thread_capability);

	GENODE_RPC_INTERFACE(Rpc_cpu_state, Rpc_exception_handler,
	                     Rpc_run, Rpc_pause, Rpc_attach, Rpc_detach,
	                     Rpc_attach_pic, Rpc_create_vcpu);
};

#endif /* _INCLUDE__VM_SESSION__VM_SESSION_H_ */
