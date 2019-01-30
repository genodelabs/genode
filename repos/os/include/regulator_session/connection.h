/*
 * \brief  Connection to regulator service
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
	 * Constructor
	 *
	 * \param regulator  identifier for the specific regulator
	 * \param label      string identifier of the client
	 */
	Connection(Genode::Env &env, Regulator_id regulator, const char * label = "")
	:
		Genode::Connection<Session>(env,
			session(env.parent(),
			        "ram_quota=8K, cap_quota=%ld, regulator=\"%s\", label=\"%s\"",
			        CAP_QUOTA, regulator_name_by_id(regulator), label)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__REGULATOR_SESSION__CONNECTION_H_ */
