/*
 * \brief  Connection to pin-control service
 * \author Norman Feske
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PIN_CONTROL_SESSION__CONNECTION_H_
#define _INCLUDE__PIN_CONTROL_SESSION__CONNECTION_H_

#include <pin_control_session/pin_control_session.h>
#include <base/rpc_client.h>
#include <base/connection.h>

namespace Pin_control { struct Connection; }


struct Pin_control::Connection : private Genode::Connection<Session>,
                                 private Rpc_client<Session>
{
	Connection(Env &env, Label const &label = Label())
	:
		Genode::Connection<Session>(env, label, Ram_quota { 8*1024 }, Args()),
		Rpc_client<Session>(cap())
	{ }

	void state(bool enabled) override { call<Rpc_state>(enabled); }

	void yield() override { call<Rpc_yield>(); }
};

#endif /* _INCLUDE__PIN_CONTROL_SESSION__CONNECTION_H_ */
