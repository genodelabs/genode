/*
 * \brief  Session interface to control a GPIO pin
 * \author Norman Feske
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PIN_CONTROL_SESSION__PIN_CONTROL_SESSION_H_
#define _INCLUDE__PIN_CONTROL_SESSION__PIN_CONTROL_SESSION_H_

#include <base/rpc_server.h>
#include <session/session.h>

namespace Pin_control {

	using namespace Genode;

	struct Session;
}


struct Pin_control::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Pin_control"; }

	/*
	 * A pin-control session consumes a dataspace capability for the server's
	 * session-object allocation and its session capability.
	 */
	enum { CAP_QUOTA = 2 };

	virtual void state(bool) = 0;

	virtual void yield() = 0;

	GENODE_RPC(Rpc_state, void, state, bool);
	GENODE_RPC(Rpc_yield, void, yield);
	GENODE_RPC_INTERFACE(Rpc_state, Rpc_yield);
};

#endif /* _INCLUDE__PIN_CONTROL_SESSION__PIN_CONTROL_SESSION_H_ */
