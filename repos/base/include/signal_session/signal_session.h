/*
 * \brief  Signal session interface
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__SIGNAL_SESSION_H_
#define _INCLUDE__SIGNAL_SESSION__SIGNAL_SESSION_H_

#include <base/capability.h>
#include <base/exception.h>
#include <signal_session/source.h>
#include <session/session.h>

namespace Genode {

	class Signal_context;
	class Signal_receiver;

	typedef Capability<Signal_receiver> Signal_receiver_capability;
	typedef Capability<Signal_context>  Signal_context_capability;
	typedef Capability<Signal_source>   Signal_source_capability;

	struct Signal_session;
}


struct Genode::Signal_session : Session
{
	static const char *service_name() { return "SIGNAL"; }

	virtual ~Signal_session() { }

	class Out_of_metadata : public Exception { };

	/**
	 * Request capability for the signal-source interface
	 */
	virtual Signal_source_capability signal_source() = 0;

	/**
	 * Allocate signal context
	 *
	 * \param imprint  opaque value that gets delivered with signals
	 *                 originating from the allocated signal-context
	 *                 capability
	 * \return new signal-context capability
	 * \throw  Out_of_metadata
	 */
	virtual Signal_context_capability alloc_context(long imprint) = 0;

	/**
	 * Free signal-context
	 *
	 * \param cap  capability of signal-context to release
	 */
	virtual void free_context(Signal_context_capability cap) = 0;

	/**
	 * Submit signals to the specified signal context
	 *
	 * \param context  signal destination
	 * \param cnt      number of signals to submit at once
	 *
	 * Note that the 'context' argument does not necessarily belong to
	 * the signal session. Normally, it is a capability obtained from
	 * a potentially untrusted source. Because we cannot trust this
	 * capability, signals are not submitted by invoking 'cap' directly
	 * but by using it as argument to our trusted signal-session
	 * interface. Otherwise, a potential signal receiver could supply
	 * a capability with a blocking interface to compromise the
	 * nonblocking behaviour of the signal submission.
	 */
	virtual void submit(Signal_context_capability context,
	                    unsigned cnt = 1) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_signal_source, Signal_source_capability, signal_source);
	GENODE_RPC_THROW(Rpc_alloc_context, Signal_context_capability, alloc_context,
	                 GENODE_TYPE_LIST(Out_of_metadata), long);
	GENODE_RPC(Rpc_free_context, void, free_context, Signal_context_capability);
	GENODE_RPC(Rpc_submit, void, submit, Signal_context_capability, unsigned);

	GENODE_RPC_INTERFACE(Rpc_submit, Rpc_signal_source, Rpc_alloc_context,
	                     Rpc_free_context);
};

#endif /* _INCLUDE__CAP_SESSION__CAP_SESSION_H_ */
