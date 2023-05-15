/*
 * \brief  Event session interface
 * \author Norman Feske
 * \date   2020-07-02
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__EVENT_SESSION__EVENT_SESSION_H_
#define _INCLUDE__EVENT_SESSION__EVENT_SESSION_H_

#include <dataspace/capability.h>
#include <base/rpc_server.h>
#include <session/session.h>

namespace Event { struct Session; }


struct Event::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Event"; }

	/*
	 * An event session consumes a dataspace capability for the server's
	 * session-object allocation, a dataspace capability for the event
	 * buffer, and its session capability.
	 */
	static constexpr unsigned CAP_QUOTA = 3;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_submit_batch, void, submit_batch, unsigned);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_submit_batch);
};

#endif /* _INCLUDE__EVENT_SESSION__EVENT_SESSION_H_ */
