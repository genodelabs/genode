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
#include <signal_source/signal_source.h>

namespace Genode {

	struct Pd_session;
	struct Parent;
	struct Signal_context;
}


struct Genode::Pd_session : Session
{
	static const char *service_name() { return "PD"; }

	virtual ~Pd_session() { }

	/**
	 * Bind thread to protection domain
	 *
	 * \param thread  capability of thread to bind
	 *
	 * \return        0 on success or negative error code
	 *
	 * After successful bind, the thread will execute inside this
	 * protection domain when started.
	 */
	virtual int bind_thread(Thread_capability thread) = 0;

	/**
	 * Assign parent to protection domain
	 *
	 * \param   parent  capability of parent interface
	 * \return  0 on success, or negative error code
	 */
	virtual int assign_parent(Capability<Parent> parent) = 0;

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


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_bind_thread,   int,  bind_thread,   Thread_capability);
	GENODE_RPC(Rpc_assign_parent, int,  assign_parent, Capability<Parent>);
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


	GENODE_RPC_INTERFACE(Rpc_bind_thread, Rpc_assign_parent, Rpc_assign_pci,
	                     Rpc_alloc_signal_source, Rpc_free_signal_source,
	                     Rpc_alloc_context,  Rpc_free_context, Rpc_submit);
};

#endif /* _INCLUDE__PD_SESSION__PD_SESSION_H_ */
