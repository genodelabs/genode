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
	Connection(Env &env, Label const &label = Label())
	:
		Genode::Connection<Session>(env, label, Ram_quota { 8*1024 }, Args()),
		Rpc_client<Session>(cap())
	{ }

	bool state() const override { return call<Rpc_state>(); }
};

#endif /* _INCLUDE__PIN_STATE_SESSION__CONNECTION_H_ */
