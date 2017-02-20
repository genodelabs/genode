/*
 * \brief  Client-side log text output session interface
 * \author Norman Feske
 * \date   2006-09-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LOG_SESSION__CLIENT_H_
#define _INCLUDE__LOG_SESSION__CLIENT_H_

#include <log_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Log_session_client; }


struct Genode::Log_session_client : Rpc_client<Log_session>
{
	explicit Log_session_client(Log_session_capability session)
	: Rpc_client<Log_session>(session) { }

	size_t write(String const &string) override {
		return call<Rpc_write>(string); }
};

#endif /* _INCLUDE__LOG_SESSION__CLIENT_H_ */
