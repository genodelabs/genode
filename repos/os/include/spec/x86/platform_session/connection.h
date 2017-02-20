/*
 * \brief  Connection to Platform service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <base/connection.h>
#include <platform_session/client.h>

namespace Platform { struct Connection; }


struct Platform::Connection : Genode::Connection<Session>, Client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env)
	:
		Genode::Connection<Session>(env, session("ram_quota=16K")),
		Client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection() __attribute__((deprecated))
	:
		Genode::Connection<Session>(session("ram_quota=16K")),
		Client(cap())
	{ }
};
