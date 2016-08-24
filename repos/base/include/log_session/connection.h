/*
 * \brief  Connection to LOG service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LOG_SESSION__CONNECTION_H_
#define _INCLUDE__LOG_SESSION__CONNECTION_H_

#include <log_session/client.h>
#include <base/connection.h>
#include <base/session_label.h>

namespace Genode { struct Log_connection; }


struct Genode::Log_connection : Connection<Log_session>, Log_session_client
{
	/**
	 * Constructor
	 */
	Log_connection(Env &env, Session_label label = Session_label())
	:
		Connection<Log_session>(env, session(env.parent(),
		                                     "ram_quota=8K, label=\"%s\"",
		                                     label.string())),
		Log_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Log_connection(Session_label label = Session_label())
	:
		Connection<Log_session>(session("ram_quota=8K, label=\"%s\"",
		                                label.string())),
		Log_session_client(cap())
	{ }
};

#endif /* _INCLUDE__LOG_SESSION__CONNECTION_H_ */
