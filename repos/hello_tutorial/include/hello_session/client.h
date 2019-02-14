/*
 * \brief  Client-side interface of the Hello service
 * \author Björn Döbel
 * \date   2008-03-20
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__HELLO_SESSION_H__CLIENT_H_
#define _INCLUDE__HELLO_SESSION_H__CLIENT_H_

#include <hello_session/hello_session.h>
#include <base/rpc_client.h>
#include <base/log.h>

namespace Hello { struct Session_client; }


struct Hello::Session_client : Genode::Rpc_client<Session>
{
	Session_client(Genode::Capability<Session> cap)
	: Genode::Rpc_client<Session>(cap) { }

	void say_hello() override
	{
		Genode::log("issue RPC for saying hello");
		call<Rpc_say_hello>();
		Genode::log("returned from 'say_hello' RPC call");
	}

	int add(int a, int b) override
	{
		return call<Rpc_add>(a, b);
	}
};

#endif /* _INCLUDE__HELLO_SESSION_H__CLIENT_H_ */
