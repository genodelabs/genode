/*
 * \brief  Connection to Terminal service without waiting
 * \author Norman Feske
 * \date   2024-07-05
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL_CONNECTION_H_
#define _TERMINAL_CONNECTION_H_

#include <terminal_session/client.h>
#include <base/connection.h>

namespace Monitor { struct Terminal_connection; }


struct Monitor::Terminal_connection : Genode::Connection<Terminal::Session>, Terminal::Session_client
{
	Terminal_connection(Genode::Env &env, Label const &label = Label())
	:
		Genode::Connection<Terminal::Session>(env, label, Ram_quota { 10*1024 }, Args()),
		Terminal::Session_client(env.rm(), cap())
	{ }
};

#endif /* _TERMINAL_CONNECTION_H_ */
