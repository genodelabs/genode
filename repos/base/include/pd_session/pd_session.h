/*
 * \brief  Protection domain (PD) session interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-06-27
 *
 * A pd session represents the protection domain of a program.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PD_SESSION__PD_SESSION_H_
#define _INCLUDE__PD_SESSION__PD_SESSION_H_

#include <base/exception.h>
#include <thread/capability.h>
#include <session/session.h>
#include <region_map/region_map.h>

namespace Genode {

	struct Pd_session;
	struct Pd_session_client;
	struct Parent;
	struct Signal_context;
}


struct Genode::Pd_session : Session
{
	static const char *service_name() { return "PD"; }

	typedef Pd_session_client Client;

	virtual ~Pd_session() { }

	/**
	 * Assign parent to protection domain
	 *
	 * \param   parent  capability of parent interface
	 */
	virtual void assign_parent(Capability<Parent> parent) = 0;

	/**
	 * Assign PCI device to PD
	 *
	 * The specified address has to refer to the locally mapped PCI
	 * configuration space of the device.
	 *
	 * This function is solely used on the NOVA kernel.
	 */
	virtual bool assign_pci(addr_t pci_config_memory_address, uint16_t bdf) = 0;


	/********************************
	 ** Support for the signal API **
	 ********************************/

	typedef Capability<Signal_source> Signal_source_capability;

	class Out_of_metadata       : public Exception { };
	class Invalid_signal_source : public Exception { };

	/**
	 * Create a new signal source
	 *
	 * \return  a cap that acts as reference to the created source
	 *
	 * The signal source provides an interface to wait for incoming signals.
	 *
	 * \throw Out_of_metadata
	 */
	virtual Signal_source_capability alloc_signal_source() = 0;

	/**
	 * Free a signal source
	 *
	 * \param cap  capability of the signal source to destroy
	 */
	virtual void free_signal_source(Signal_source_capability cap) = 0;

	/**
	 * Allocate signal context
	 *
	 * \param source  signal source that shall provide the new context
	 *
	 *
	 * \param imprint  opaque value that gets delivered with signals
	 *                 originating from the allocated signal-context capability
	 * \return new signal-context capability
	 *
	 * \throw Out_of_metadata
	 * \throw Invalid_signal_source
	 */
	virtual Capability<Signal_context>
	alloc_context(Signal_source_capability source, unsigned long imprint) = 0;

	/**
	 * Free signal-context
	 *
	 * \param cap  capability of signal-context to release
	 */
	virtual void free_context(Capability<Signal_context> cap) = 0;

	/**
	 * Submit signals to the specified signal context
	 *
	 * \param context  signal destination
	 * \param cnt      number of signals to submit at once
	 *
	 * The 'context' argument does not necessarily belong to this PD session.
	 * Normally, it is a capability obtained from a potentially untrusted
	 * component. Because we cannot trust this capability, signals are not
	 * submitted by invoking 'cap' directly but by using it as argument to our
	 * trusted PD-session interface. Otherwise, a potential signal receiver
	 * could supply a capability with a blocking interface to compromise the
	 * nonblocking behaviour of the signal submission.
	 */
	virtual void submit(Capability<Signal_context> context, unsigned cnt = 1) = 0;


	/***********************************
	 ** Support for the RPC framework **
	 ***********************************/

	/**
	 * Allocate new RPC-object capability
	 *
	 * \param ep  entry point that will use this capability
	 *
	 * \throw Out_of_metadata  if meta-data backing store is exhausted
	 *
	 * \return new RPC capability
	 */
	virtual Native_capability alloc_rpc_cap(Native_capability ep) = 0;

	/**
	 * Free RPC-object capability
	 *
	 * \param cap  capability to free
	 */
	virtual void free_rpc_cap(Native_capability cap) = 0;


	/**************************************
	 ** Virtual address-space management **
	 **************************************/

	enum { LINKER_AREA_SIZE = 160*1024*1024UL };

	/**
	 * Return region map of the PD's virtual address space
	 */
	virtual Capability<Region_map> address_space() = 0;

	/**
	 * Return region map of the PD's stack area
	 */
	virtual Capability<Region_map> stack_area() = 0;

	/**
	 * Return region map of the PD's linker area
	 */
	virtual Capability<Region_map> linker_area() = 0;


	/*****************************************
	 ** Access to kernel-specific interface **
	 *****************************************/

	/**
	 * Common base class of kernel-specific PD interfaces
	 */
	struct Native_pd { };

	/**
	 * Return capability to kernel-specific PD operations
	 */
	virtual Capability<Native_pd> native_pd() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_assign_parent, void, assign_parent, Capability<Parent>);
	GENODE_RPC(Rpc_assign_pci,    bool, assign_pci,    addr_t, uint16_t);

	GENODE_RPC_THROW(Rpc_alloc_signal_source, Signal_source_capability,
	                 alloc_signal_source, GENODE_TYPE_LIST(Out_of_metadata));
	GENODE_RPC(Rpc_free_signal_source, void, free_signal_source, Signal_source_capability);
	GENODE_RPC_THROW(Rpc_alloc_context, Capability<Signal_context>, alloc_context,
	                 GENODE_TYPE_LIST(Out_of_metadata, Invalid_signal_source),
	                 Signal_source_capability, unsigned long);
	GENODE_RPC(Rpc_free_context, void, free_context,
	           Capability<Signal_context>);
	GENODE_RPC(Rpc_submit, void, submit, Capability<Signal_context>, unsigned);

	GENODE_RPC_THROW(Rpc_alloc_rpc_cap, Native_capability, alloc_rpc_cap,
	                 GENODE_TYPE_LIST(Out_of_metadata), Native_capability);
	GENODE_RPC(Rpc_free_rpc_cap, void, free_rpc_cap, Native_capability);

	GENODE_RPC(Rpc_address_space, Capability<Region_map>, address_space);
	GENODE_RPC(Rpc_stack_area,    Capability<Region_map>, stack_area);
	GENODE_RPC(Rpc_linker_area,   Capability<Region_map>, linker_area);

	GENODE_RPC(Rpc_native_pd, Capability<Native_pd>, native_pd);

	GENODE_RPC_INTERFACE(Rpc_assign_parent, Rpc_assign_pci,
	                     Rpc_alloc_signal_source, Rpc_free_signal_source,
	                     Rpc_alloc_context, Rpc_free_context, Rpc_submit,
	                     Rpc_alloc_rpc_cap, Rpc_free_rpc_cap, Rpc_address_space,
	                     Rpc_stack_area, Rpc_linker_area, Rpc_native_pd);
};

#endif /* _INCLUDE__PD_SESSION__PD_SESSION_H_ */
