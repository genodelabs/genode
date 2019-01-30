/*
 * \brief  Connection to input service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INPUT_SESSION__CONNECTION_H_
#define _INCLUDE__INPUT_SESSION__CONNECTION_H_

#include <input_session/client.h>
#include <base/connection.h>

namespace Input { struct Connection; }

struct Input::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, char const *label = "")
	:
		Genode::Connection<Input::Session>(env,
		                                   session(env.parent(),
		                                   "ram_quota=18K, cap_quota=%u, label=\"%s\"",
		                                   CAP_QUOTA, label)),
		Session_client(env.rm(), cap())
	{ }
};

#endif /* _INCLUDE__INPUT_SESSION__CONNECTION_H_ */
