/*
 * \brief  Terminal that directs output to the LOG interface
 * \author Norman Feske
 * \date   2013-11-08
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <root/component.h>
#include <os/server.h>
#include <os/attached_ram_dataspace.h>
#include <terminal_session/terminal_session.h>
#include <log_session/log_session.h>

namespace Terminal {

	class Session_component;
	class Root_component;
	class Main;

	using namespace Genode;
};


/**
 * Utility for the buffered output of small successive write operations
 */
class Buffered_output
{
	private:

		enum { SIZE = Genode::Log_session::String::MAX_SIZE };

		typedef Genode::size_t size_t;

		char _buf[SIZE + 1 /* room for null-termination */ ];

		/* index of next character within '_buf' to write */
		unsigned _index = 0;

		void _flush()
		{
			/* append null termination */
			_buf[_index] = 0;

			/* flush buffered characters to LOG */
			Genode::log(Genode::Cstring(_buf));

			/* reset */
			_index = 0;
		}

		size_t _remaining_capacity() const { return SIZE - _index; }

	public:

		size_t write(char const *src, size_t num_bytes)
		{
			size_t const consume_bytes = Genode::min(num_bytes,
			                                         _remaining_capacity());

			for (unsigned i = 0; i < consume_bytes; i++) {
				char const c = src[i];
				if (c == '\n')
					_flush();
				else
					_buf[_index++] = c;
			}

			if (_remaining_capacity() == 0)
				_flush();

			return consume_bytes;
		}
};


/**************
 ** Terminal **
 **************/

class Terminal::Session_component : public Rpc_object<Session, Session_component>
{
	private:

		/**
		 * Buffer shared with the terminal client
		 */
		Attached_ram_dataspace _io_buffer;

		Buffered_output _output;

	public:

		Session_component(size_t io_buffer_size)
		:
			_io_buffer(env()->ram_session(), io_buffer_size)
		{ }


		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() { return Size(0, 0); }

		bool avail() { return false; }

		size_t _read(size_t dst_len) { return 0; }

		void _write(Genode::size_t num_bytes)
		{
			/* sanitize argument */
			num_bytes = Genode::min(num_bytes, _io_buffer.size());

			char const *src = _io_buffer.local_addr<char>();

			for (size_t written_bytes = 0; written_bytes < num_bytes; )
				written_bytes += _output.write(src + written_bytes,
				                               num_bytes - written_bytes);
		}

		Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		void read_avail_sigh(Signal_context_capability) { }

		void connected_sigh(Signal_context_capability sigh)
		{
			/*
			 * Immediately reflect connection-established signal to the
			 * client because the session is ready to use immediately after
			 * creation.
			 */
			Signal_transmitter(sigh).submit();
		}

		size_t read(void *buf, size_t) { return 0; }
		size_t write(void const *buf, size_t) { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	protected:

		Session_component *_create_session(const char *args)
		{
			size_t const io_buffer_size = 4096;
			return new (md_alloc()) Session_component(io_buffer_size);
		}

	public:

		Root_component(Server::Entrypoint &ep, Genode::Allocator  &md_alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc)
		{ }
};


struct Terminal::Main
{
	Server::Entrypoint &ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root_component terminal_root = { ep, sliced_heap };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(terminal_root));
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "log_terminal_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Terminal::Main terminal(ep);
	}
}
