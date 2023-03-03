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
	Connection(Genode::Env &env, Label const &label = Label())
	:
		Genode::Connection<Input::Session>(env, label, Ram_quota { 18*1024 }, Args()),
		Session_client(env.rm(), cap())
	{ }
};

#endif /* _INCLUDE__INPUT_SESSION__CONNECTION_H_ */
