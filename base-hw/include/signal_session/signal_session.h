/*
 * \brief  Signal session interface
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__SIGNAL_SESSION_H_
#define _INCLUDE__SIGNAL_SESSION__SIGNAL_SESSION_H_

/* Genode includes */
#include <base/capability.h>
#include <base/exception.h>
#include <session/session.h>

namespace Genode
{
	class Signal_receiver;
	class Signal_context;

	/*
	 * The 'dst' of this cap is used to communicate the ID of the
	 * corresponding signal-receiver kernel-object or 0 if the cap is invalid.
	 */
	typedef Capability<Signal_receiver> Signal_receiver_capability;

	/*
	 * The 'dst' of this cap is used to communicate the ID of the
	 * corresponding signal-context kernel-object or 0 if the cap is invalid.
	 */
	typedef Capability<Signal_context> Signal_context_capability;

	/**
	 * Signal session interface
	 */
	struct Signal_session : Session
	{
		class Out_of_metadata : public Exception { };

		/**
		 * String that can be used to refer to this service
		 */
		static const char * service_name() { return "SIGNAL"; }

		/**
		 * Destructor
		 */
		virtual ~Signal_session() { }

		/**
		 * Create and manage a new signal receiver
		 *
		 * \return  a cap that acts as reference to the created object
		 *
		 * \throw Out_of_metadata
		 * \throw Exception
		 */
		virtual Signal_receiver_capability alloc_receiver() = 0;

		/**
		 * Create and manage a new signal context
		 *
		 * \param r        names the signal receiver that shall provide
		 *                 the new context
		 * \param imprint  every signal that occures on the new context gets
		 *                 signed with this value
		 *
		 * \return  a cap that acts as reference to the created object
		 *
		 * \throw Out_of_metadata
		 * \throw Exception
		 */
		virtual Signal_context_capability
		alloc_context(Signal_receiver_capability r,
		              unsigned const imprint) = 0;

		/**
		 * Free a signal context
		 *
		 * \param cap  capability of signal-context to release
		 *
		 * \throw Exception
		 */
		virtual void free_context(Signal_context_capability cap) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC_THROW(Rpc_alloc_receiver, Signal_receiver_capability,
		                 alloc_receiver, GENODE_TYPE_LIST(Out_of_metadata,
		                 Exception));
		GENODE_RPC_THROW(Rpc_alloc_context, Signal_context_capability,
		                 alloc_context, GENODE_TYPE_LIST(Out_of_metadata,
		                 Exception), Signal_receiver_capability, unsigned);
		GENODE_RPC_THROW(Rpc_free_context, void, free_context,
		                 GENODE_TYPE_LIST(Exception),
		                 Signal_context_capability);

		GENODE_RPC_INTERFACE(Rpc_alloc_receiver, Rpc_alloc_context,
		                     Rpc_free_context);
	};
}

#endif /* _INCLUDE__SIGNAL_SESSION__SIGNAL_SESSION_H_ */

