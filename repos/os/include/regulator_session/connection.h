/*
 * \brief  Connection to regulator service
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__CONNECTION_H_
#define _INCLUDE__REGULATOR_SESSION__CONNECTION_H_

#include <regulator_session/client.h>
#include <regulator/consts.h>
#include <base/connection.h>

namespace Regulator { struct Connection; }


struct Regulator::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Regulator::Session> _session(Genode::Parent &parent,
	                                        char const *label,
	                                        Regulator_id regulator)
	{
		return session("ram_quota=8K, regulator=\"%s\", label=\"%s\"",
		               regulator_name_by_id(regulator), label);
	}

	/**
	 * Constructor
	 *
	 * \param regulator  identifier for the specific regulator
	 * \param label      string identifier of the client
	 */
	Connection(Genode::Env &env, Regulator_id regulator, const char * label = "")
	:
		Genode::Connection<Session>(env, _session(env.parent(), label, regulator)),
		Session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \param regulator  identifier for the specific regulator
	 * \param label      string identifier of the client
	 */
	Connection(Regulator_id regulator, const char * label = "")
	:
		Genode::Connection<Session>(_session(*Genode::env()->parent(), label, regulator)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__REGULATOR_SESSION__CONNECTION_H_ */
