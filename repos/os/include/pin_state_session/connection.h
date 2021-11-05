/*
 * \brief  Connection to pin-state service
 * \author Norman Feske
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PIN_STATE_SESSION__CONNECTION_H_
#define _INCLUDE__PIN_STATE_SESSION__CONNECTION_H_

#include <pin_state_session/pin_state_session.h>
#include <base/connection.h>

namespace Pin_state { struct Connection; }

struct Pin_state::Connection : private Genode::Connection<Session>,
                               private Rpc_client<Session>
{
	enum { RAM_QUOTA = 8*1024UL };

	Connection(Env &env, char const *label = "")
	:
		Genode::Connection<Session>(env,
		                            session(env.parent(),
		                            "ram_quota=%u, cap_quota=%u, label=\"%s\"",
		                            RAM_QUOTA, CAP_QUOTA, label)),
		Rpc_client<Session>(cap())
	{ }

	bool state() const override { return call<Rpc_state>(); }
};

#endif /* _INCLUDE__PIN_STATE_SESSION__CONNECTION_H_ */
