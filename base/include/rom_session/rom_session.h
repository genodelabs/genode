/*
 * \brief  ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 *
 * A ROM session corresponds to an open file. The file name is specified as an
 * argument on session creation.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ROM_SESSION__ROM_SESSION_H_
#define _INCLUDE__ROM_SESSION__ROM_SESSION_H_

#include <dataspace/capability.h>
#include <session/session.h>
#include <base/signal.h>

namespace Genode {

	struct Rom_dataspace : Dataspace { };

	typedef Capability<Rom_dataspace> Rom_dataspace_capability;

	struct Rom_session : Session
	{
		static const char *service_name() { return "ROM"; }

		virtual ~Rom_session() { }

		/**
		 * Request dataspace containing the ROM session data
		 *
		 * \return  capability to ROM dataspace
		 *
		 * The capability may be invalid.
		 *
		 * Consecutive calls of this functions are not guaranteed to return the
		 * same dataspace as dynamic ROM sessions may update the ROM data
		 * during the lifetime of the session. When calling the function, the
		 * server may destroy the old dataspace and replace it with a new one
		 * containing the updated data. Hence, prior calling this function, the
		 * client should make sure to detach the previously requested dataspace
		 * from its local address space.
		 */
		virtual Rom_dataspace_capability dataspace() = 0;

		/**
		 * Register signal handler to be notified of ROM data changes
		 *
		 * The ROM session interface allows for the implementation of ROM
		 * services that dynamically update the data exported as ROM dataspace
		 * during the lifetime of the session. This is useful in scenarios
		 * where this data is generated rather than originating from a static
		 * file, for example to update a program's configuration at runtime.
		 *
		 * By installing a signal handler using the 'sigh()' function, the
		 * client will receive a notification each time the data changes at the
		 * server. From the client's perspective, the original data contained
		 * in the currently used dataspace remains unchanged until the client
		 * calls 'dataspace()' the next time.
		 */
		virtual void sigh(Signal_context_capability sigh) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_dataspace, Rom_dataspace_capability, dataspace);
		GENODE_RPC(Rpc_sigh, void, sigh, Signal_context_capability);

		GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_sigh);
	};
}

#endif /* _INCLUDE__ROM_SESSION__ROM_SESSION_H_ */
