/*
 * \brief  Client-side UART session interface
 * \author Norman Feske
 * \date   2012-10-29
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UART_SESSION__CLIENT_H_
#define _INCLUDE__UART_SESSION__CLIENT_H_

/* Genode includes */
#include <uart_session/uart_session.h>
#include <terminal_session/client.h>

namespace Uart { class Session_client; }


class Uart::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Terminal::Session_client _terminal;

	public:

		Session_client(Genode::Region_map &local_rm, Genode::Capability<Session> cap)
		:
			Genode::Rpc_client<Session>(cap), _terminal(local_rm, cap)
		{ }

		Session_client(Genode::Capability<Session> cap) __attribute__((deprecated))
		:
			Genode::Rpc_client<Session>(cap), _terminal(*Genode::env_deprecated()->rm_session(), cap)
		{ }


		/********************
		 ** UART interface **
		 ********************/

		void baud_rate(Genode::size_t bits_per_second)
		{
			call<Rpc_baud_rate>(bits_per_second);
		}


		/************************
		 ** Terminal interface **
		 ************************/

		Size size() { return _terminal.size(); }

		bool avail() { return _terminal.avail(); }

		Genode::size_t read(void *buf, Genode::size_t buf_size)
		{
			return _terminal.read(buf, buf_size);
		}

		Genode::size_t write(void const *buf, Genode::size_t num_bytes)
		{
			return _terminal.write(buf, num_bytes);
		}

		void connected_sigh(Genode::Signal_context_capability cap)
		{
			_terminal.connected_sigh(cap);
		}

		void read_avail_sigh(Genode::Signal_context_capability cap)
		{
			_terminal.read_avail_sigh(cap);
		}

		Genode::size_t io_buffer_size() const
		{
			return _terminal.io_buffer_size();
		}
};

#endif /* _INCLUDE__UART_SESSION__CLIENT_H_ */
