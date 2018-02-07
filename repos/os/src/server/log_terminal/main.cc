/*
 * \brief  Terminal that directs output to the LOG interface
 * \author Norman Feske
 * \date   2013-11-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/attached_ram_dataspace.h>
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

		Buffered_output _output { };

	public:

		Session_component(Ram_session &ram,
		                  Region_map  &rm,
		                  size_t       io_buffer_size)
		:
			_io_buffer(ram, rm, io_buffer_size)
		{ }


		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override { return Size(0, 0); }

		bool avail() override { return false; }

		size_t _read(size_t) { return 0; }

		size_t _write(Genode::size_t num_bytes)
		{
			/* sanitize argument */
			num_bytes = Genode::min(num_bytes, _io_buffer.size());

			char const *src = _io_buffer.local_addr<char>();

			size_t written_bytes;
			for (written_bytes = 0; written_bytes < num_bytes; )
				written_bytes += _output.write(src + written_bytes,
				                               num_bytes - written_bytes);

			return written_bytes;
		}

		Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		void read_avail_sigh(Signal_context_capability) override { }

		void size_changed_sigh(Signal_context_capability) override { }

		void connected_sigh(Signal_context_capability sigh)
		{
			/*
			 * Immediately reflect connection-established signal to the
			 * client because the session is ready to use immediately after
			 * creation.
			 */
			Signal_transmitter(sigh).submit();
		}

		size_t read(void *, size_t) override { return 0; }
		size_t write(void const *, size_t) override { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Ram_session &_ram;
		Region_map  &_rm;

	protected:

		Session_component *_create_session(const char *)
		{
			size_t const io_buffer_size = 4096;
			return new (md_alloc()) Session_component(_ram, _rm, io_buffer_size);
		}

	public:

		Root_component(Entrypoint  &ep,
		               Allocator   &md_alloc,
		               Ram_session &ram,
		               Region_map  &rm)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ram(ram), _rm(rm)
		{ }
};


struct Terminal::Main
{
	Env &_env;

	Sliced_heap sliced_heap { _env.ram(), _env.rm() };

	Root_component terminal_root { _env.ep(), sliced_heap,
	                               _env.ram(), _env.rm() };

	Main(Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(terminal_root));
	}
};

void Component::construct(Genode::Env &env) { static Terminal::Main main(env); }
