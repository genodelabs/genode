/*
 * \brief  Connection to LOG service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LOG_SESSION__CONNECTION_H_
#define _INCLUDE__LOG_SESSION__CONNECTION_H_

#include <log_session/client.h>
#include <base/connection.h>
#include <base/session_label.h>

namespace Genode { struct Log_connection; }


struct Genode::Log_connection : Connection<Log_session>, Log_session_client
{
	enum { RAM_QUOTA = 8*1024UL };

	/**
	 * Constructor
	 */
	Log_connection(Env &env, Session_label label = Session_label())
	:
		Connection<Log_session>(env, session(env.parent(),
		                                     "ram_quota=%ld, label=\"%s\"",
		                                     RAM_QUOTA, label.string())),
		Log_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Log_connection(Session_label label = Session_label()) __attribute__((deprecated))
	:
		Connection<Log_session>(session("ram_quota=%ld, label=\"%s\"",
		                                RAM_QUOTA, label.string())),
		Log_session_client(cap())
	{ }
};

#endif /* _INCLUDE__LOG_SESSION__CONNECTION_H_ */
