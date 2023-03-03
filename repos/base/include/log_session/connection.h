/*
 * \brief  Connection to LOG service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LOG_SESSION__CONNECTION_H_
#define _INCLUDE__LOG_SESSION__CONNECTION_H_

#include <log_session/client.h>
#include <base/connection.h>

namespace Genode { struct Log_connection; }


struct Genode::Log_connection : Connection<Log_session>, Log_session_client
{
	Log_connection(Env &env, Session_label const &label = Session_label())
	:
		Connection<Log_session>(env, label, Ram_quota { RAM_QUOTA }, Args()),
		Log_session_client(cap())
	{ }
};

#endif /* _INCLUDE__LOG_SESSION__CONNECTION_H_ */
