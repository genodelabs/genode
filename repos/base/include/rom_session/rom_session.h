/*
 * \brief  ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 *
 * A ROM session corresponds to a ROM module. The module name is specified as
 * an argument on session creation.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ROM_SESSION__ROM_SESSION_H_
#define _INCLUDE__ROM_SESSION__ROM_SESSION_H_

#include <dataspace/capability.h>
#include <session/session.h>
#include <base/signal.h>

namespace Genode {

	struct Rom_dataspace;
	struct Rom_session;
	struct Rom_session_client;

	typedef Capability<Rom_dataspace> Rom_dataspace_capability;
}


struct Genode::Rom_dataspace : Dataspace { };


struct Genode::Rom_session : Session
{
	static const char *service_name() { return "ROM"; }

	typedef Rom_session_client Client;

	virtual ~Rom_session() { }

	/**
	 * Request dataspace containing the ROM session data
	 *
	 * \return  capability to ROM dataspace
	 *
	 * The capability may be invalid.
	 *
	 * Consecutive calls of this method are not guaranteed to return the
	 * same dataspace as dynamic ROM sessions may update the ROM data
	 * during the lifetime of the session. When calling the method, the
	 * server may destroy the old dataspace and replace it with a new one
	 * containing the updated data. Hence, prior calling this method, the
	 * client should make sure to detach the previously requested dataspace
	 * from its local address space.
	 */
	virtual Rom_dataspace_capability dataspace() = 0;

	/**
	 * Update ROM dataspace content
	 *
	 * This method is an optimization for use cases where ROM dataspaces
	 * are updated at a high rate. In such cases, requesting a new
	 * dataspace for each update induces a large overhead because
	 * memory mappings must be revoked and updated (e.g., handling the
	 * page faults referring to the dataspace). If the updated content
	 * fits in the existing dataspace, those costly operations can be
	 * omitted.
	 *
	 * When this method is called, the server may replace the dataspace
	 * content with new data.
	 *
	 * \return true if the existing dataspace contains up-to-date content,
	 *         or false if a new dataspace must be requested via the
	 *         'dataspace' method
	 */
	virtual bool update() { return false; }

	/**
	 * Register signal handler to be notified of ROM data changes
	 *
	 * The ROM session interface allows for the implementation of ROM
	 * services that dynamically update the data exported as ROM dataspace
	 * during the lifetime of the session. This is useful in scenarios
	 * where this data is generated rather than originating from a static
	 * file, for example to update a program's configuration at runtime.
	 *
	 * By installing a signal handler using the 'sigh()' method, the
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
	GENODE_RPC(Rpc_update, bool, update);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_update, Rpc_sigh);
};

#endif /* _INCLUDE__ROM_SESSION__ROM_SESSION_H_ */
