/*
 * \brief  Session interface to obtain a GPIO pin state
 * \author Norman Feske
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PIN_STATE_SESSION__PIN_STATE_SESSION_H_
#define _INCLUDE__PIN_STATE_SESSION__PIN_STATE_SESSION_H_

#include <base/rpc_server.h>
#include <session/session.h>

namespace Pin_state {

	using namespace Genode;

	struct Session;
}


struct Pin_state::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Pin_state"; }

	/*
	 * A pin-state session consumes a dataspace capability for the server's
	 * session-object allocation and its session capability.
	 */
	static constexpr unsigned CAP_QUOTA = 2;

	virtual bool state() const = 0;

	GENODE_RPC(Rpc_state, bool, state);
	GENODE_RPC_INTERFACE(Rpc_state);
};

#endif /* _INCLUDE__PIN_STATE_SESSION__PIN_STATE_SESSION_H_ */
