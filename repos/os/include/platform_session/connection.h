/*
 * \brief  Connection to platform service
 * \author Stefan Kalkowski
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLATFORM_SESSION__CONNECTION_H_
#define _INCLUDE__PLATFORM_SESSION__CONNECTION_H_

#include <platform_session/client.h>
#include <util/arg_string.h>
#include <base/connection.h>

namespace Platform { class Connection; }


struct Platform::Connection : Genode::Connection<Session>, Client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env)
	: Genode::Connection<Session>(env, session(env.parent(),
	                              "ram_quota=6K, cap_quota=%ld", CAP_QUOTA)),
	  Client(cap()) { }
};

#endif /* _INCLUDE__PLATFORM_SESSION__CONNECTION_H_ */
