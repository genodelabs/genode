/*
 * \brief  Log text output session interface
 * \author Norman Feske
 * \date   2006-09-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LOG_SESSION__LOG_SESSION_H_
#define _INCLUDE__LOG_SESSION__LOG_SESSION_H_

#include <base/capability.h>
#include <base/stdint.h>
#include <base/rpc_args.h>
#include <session/session.h>

namespace Genode {
	
	struct Log_session;
	struct Log_session_client;
}


struct Genode::Log_session : Session
{
	static const char *service_name() { return "LOG"; }

	typedef Log_session_client Client;

	virtual ~Log_session() { }

	/* the lowest platform-specific maximum IPC payload size (OKL4) */
	enum { MAX_STRING_LEN = 236};

	typedef Rpc_in_buffer<MAX_STRING_LEN> String;

	/**
	 * Output null-terminated string
	 *
	 * \return  number of written characters
	 */
	virtual size_t write(String const &string) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_write, size_t, write, String const &);
	GENODE_RPC_INTERFACE(Rpc_write);
};

#endif /* _INCLUDE__LOG_SESSION__LOG_SESSION_H_ */
