/*
 * \brief  Connection to event service
 * \author Norman Feske
 * \date   2020-07-02
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__EVENT_SESSION__CONNECTION_H_
#define _INCLUDE__EVENT_SESSION__CONNECTION_H_

#include <event_session/client.h>
#include <base/connection.h>

namespace Event { struct Connection; }


struct Event::Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Env &env, Label const &label = Label())
	:
		Genode::Connection<Event::Session>(env, label, Ram_quota { 18*1024 }, Args()),
		Session_client(env.rm(), cap())
	{ }
};

#endif /* _INCLUDE__EVENT_SESSION__CONNECTION_H_ */
