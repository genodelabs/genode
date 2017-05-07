/*
 * \brief  Connection to Hello service
 * \author Norman Feske
 * \date   2008-11-10
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__HELLO_SESSION__CONNECTION_H_
#define _INCLUDE__HELLO_SESSION__CONNECTION_H_

#include <hello_session/client.h>
#include <base/connection.h>

namespace Hello { struct Connection; }


struct Hello::Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Env &env)
	:
		/* create session */
		Genode::Connection<Hello::Session>(env, session(env.parent(),
		                                                "ram_quota=6K, cap_quota=4")),

		/* initialize RPC interface */
		Session_client(cap()) { }
};

#endif /* _INCLUDE__HELLO_SESSION__CONNECTION_H_ */
