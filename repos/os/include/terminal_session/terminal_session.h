/*
 * \brief  Terminal session interface
 * \author Norman Feske
 * \date   2011-08-11
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TERMINAL_SESSION__TERMINAL_SESSION_H_
#define _INCLUDE__TERMINAL_SESSION__TERMINAL_SESSION_H_

/* Genode includes */
#include <session/session.h>
#include <base/rpc.h>
#include <dataspace/capability.h>

namespace Terminal { struct Session; }


struct Terminal::Session : Genode::Session
{
	static const char *service_name() { return "Terminal"; }

	class Size
	{
		private:

			unsigned _columns, _lines;

		public:

			Size() : _columns(0), _lines(0) { }

			Size(unsigned columns, unsigned lines)
			: _columns(columns), _lines(lines) { }

			unsigned columns() const { return _columns; }
			unsigned lines()   const { return _lines; }
	};

	/**
	 * Return terminal size
	 */
	virtual Size size() = 0;

	/**
	 * Return true of one or more characters are available for reading
	 */
	virtual bool avail() = 0;

	/**
	 * Read characters from terminal
	 */
	virtual Genode::size_t read(void *buf, Genode::size_t buf_size) = 0;

	/**
	 * Write characters to terminal
	 */
	virtual Genode::size_t write(void const *buf, Genode::size_t num_bytes) = 0;

	/**
	 * Register signal handler to be informed about the established connection
	 *
	 * This method is used for a simple startup protocol of terminal
	 * sessions. At session-creation time, the terminal session may not
	 * be ready to use. For example, a TCP terminal session needs an
	 * established TCP connection first. However, we do not want to let the
	 * session-creation block on the server side because this would render
	 * the servers entrypoint unavailable for all other clients until the
	 * TCP connection is ready. Instead, we deliver a 'connected' signal
	 * to the client emitted when the session becomes ready to use. The
	 * Terminal::Connection waits for this signal at construction time.
	 */
	virtual void connected_sigh(Genode::Signal_context_capability cap) = 0;

	/**
	 * Register signal handler to be informed about ready-to-read characters
	 */
	virtual void read_avail_sigh(Genode::Signal_context_capability cap) = 0;


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_size, Size, size);
	GENODE_RPC(Rpc_avail, bool, avail);
	GENODE_RPC(Rpc_read, Genode::size_t, _read, Genode::size_t);
	GENODE_RPC(Rpc_write, Genode::size_t, _write, Genode::size_t);
	GENODE_RPC(Rpc_connected_sigh, void, connected_sigh, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_read_avail_sigh, void, read_avail_sigh, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, _dataspace);

	GENODE_RPC_INTERFACE(Rpc_size, Rpc_avail, Rpc_read, Rpc_write,
	                     Rpc_connected_sigh, Rpc_read_avail_sigh,
	                     Rpc_dataspace);
};

#endif /* _INCLUDE__TERMINAL_SESSION__TERMINAL_SESSION_H_ */
