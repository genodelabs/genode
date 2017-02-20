/*
 * \brief  IPC server
 * \author Norman Feske
 * \date   2016-03-16
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__IPC_SERVER_H_
#define _INCLUDE__BASE__INTERNAL__IPC_SERVER_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/ipc.h>

namespace Genode {

	struct Ipc_server;

	/**
	 * Send reply to caller
	 */
	void ipc_reply(Native_capability caller, Rpc_exception_code,
	               Msgbuf_base &snd_msg);

	typedef Native_capability Reply_capability;

	struct Rpc_request
	{
		Reply_capability caller;
		unsigned long    badge = ~0;

		Rpc_request() { }

		Rpc_request(Reply_capability caller, unsigned long badge)
		: caller(caller), badge(badge) { }
	};

	/**
	 * Send result of previous RPC request and wait for new one
	 */
	Rpc_request ipc_reply_wait(Reply_capability const &caller,
	                           Rpc_exception_code      reply_exc,
	                           Msgbuf_base            &reply_msg,
	                           Msgbuf_base            &request_msg);
}


struct Genode::Ipc_server : Native_capability
{
	Ipc_server();
	~Ipc_server();
};

#endif /* _INCLUDE__BASE__INTERNAL__IPC_SERVER_H_ */
