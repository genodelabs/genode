/*
 * \brief  Connection to input service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INPUT_SESSION__CONNECTION_H_
#define _INCLUDE__INPUT_SESSION__CONNECTION_H_

#include <input_session/client.h>
#include <base/connection.h>

namespace Input { struct Connection; }

struct Input::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Input::Session> _session(Genode::Parent &parent, char const *label) {
		return session(parent, "ram_quota=16K, label=\"%s\"", label); }

	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, char const *label = "")
	:
		Genode::Connection<Input::Session>(env, _session(env.parent(), label)),
		Session_client(env, cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection()
	:
		Genode::Connection<Input::Session>(
			session(*Genode::env()->parent(), "ram_quota=16K")),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__INPUT_SESSION__CONNECTION_H_ */
