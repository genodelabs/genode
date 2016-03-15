/*
 * \brief  IPC server
 * \author Norman Feske
 * \date   2016-03-16
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__IPC_SERVER_H_
#define _INCLUDE__BASE__INTERNAL__IPC_SERVER_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/ipc.h>

/* base-internal includes */
#include <base/internal/native_connection_state.h>

namespace Genode {

	class Native_connection_state;
	class Ipc_server;
}


class Genode::Ipc_server : public Ipc_marshaller, public Ipc_unmarshaller,
                           public Native_capability
{
	private:

		bool _reply_needed = false;   /* false for the first reply_wait */

		Native_capability _caller;

		Native_connection_state &_rcv_cs;

		void _prepare_next_reply_wait();

		unsigned long _badge = 0;

		Rpc_exception_code _exception_code { 0 };

	public:

		/**
		 * Constructor
		 */
		Ipc_server(Native_connection_state &,
		           Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg);

		~Ipc_server();

		/**
		 * Send reply to destination
		 */
		void reply();

		/**
		 * Send result of previous RPC request and wait for new one
		 */
		void reply_wait();

		/**
		 * Set return value of server call
		 */
		void ret(Rpc_exception_code code) { _exception_code = code; }

		/**
		 * Read badge that was supplied with the message
		 */
		unsigned long badge() const { return _badge; }

		/**
		 * Set reply destination
		 */
		void caller(Native_capability const &caller)
		{
			_caller       = caller;
			_reply_needed = caller.valid();
		}

		Native_capability caller() const { return _caller; }
};

#endif /* _INCLUDE__BASE__INTERNAL__IPC_SERVER_H_ */
