/*
 * \brief  Terminal session interface
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_H_
#define _SESSION_H_

/* Genode includes */
#include <util/utf8.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>
#include <terminal/read_buffer.h>

/* local includes */
#include "types.h"

namespace Terminal {
	class Session_component;
	class Root_component;
}


class Terminal::Session_component : public Rpc_object<Session, Session_component>
{
	private:

		Read_buffer              &_read_buffer;
		Character_consumer       &_character_consumer;
		Size                      _terminal_size { };
		Size                      _reported_terminal_size { };
		Attached_ram_dataspace    _io_buffer;
		Signal_context_capability _size_changed_sigh { };

		Terminal::Position _last_cursor_pos { };

	public:

		/**
		 * Constructor
		 */
		Session_component(Env                &env,
		                  Read_buffer        &read_buffer,
		                  Character_consumer &character_consumer,
		                  size_t              io_buffer_size)
		:
			_read_buffer(read_buffer),
			_character_consumer(character_consumer),
			_io_buffer(env.ram(), env.rm(), io_buffer_size)
		{ }

		void notify_resized(Size new_size)
		{
			_terminal_size = new_size;

			bool const client_is_out_of_date =
				(_reported_terminal_size.columns() != new_size.columns()) ||
				(_reported_terminal_size.lines()   != new_size.lines());

			if (client_is_out_of_date && _size_changed_sigh.valid())
				Signal_transmitter(_size_changed_sigh).submit();
		}


		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override
		{
			_reported_terminal_size = _terminal_size;

			return _terminal_size;
		}

		bool avail() override { return !_read_buffer.empty(); }

		size_t _read(size_t dst_len)
		{
			/* read data, block on first byte if needed */
			unsigned       num_bytes = 0;
			unsigned char *dst       = _io_buffer.local_addr<unsigned char>();
			size_t         dst_size  = min(_io_buffer.size(), dst_len);

			while (!_read_buffer.empty() && num_bytes < dst_size)
				dst[num_bytes++] = _read_buffer.get();

			return num_bytes;
		}

		size_t _write(size_t num_bytes)
		{
			char const *src = _io_buffer.local_addr<char const>();

			unsigned const max = min(num_bytes, _io_buffer.size());

			unsigned i = 0;
			for (Utf8_ptr utf8(src); utf8.complete() && i < max; ) {

				_character_consumer.consume_character(
					Terminal::Character(utf8.codepoint()));

				i += utf8.length();
				if (i >= max)
					break;

				utf8 = Utf8_ptr(src + i);
			}

			/* consume trailing zero characters */
			for (; i < max && src[i] == 0; i++);

			/* we don't support UTF-8 sequences split into multiple writes */
			if (i != num_bytes) {
				warning("truncated UTF-8 sequence");
				for (unsigned j = i; j < num_bytes; j++)
					warning("(unhandled value ", Hex(src[j]), ")");
			}

			return num_bytes;
		}

		Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		void connected_sigh(Signal_context_capability sigh) override
		{
			/*
			 * Immediately reflect connection-established signal to the
			 * client because the session is ready to use immediately after
			 * creation.
			 */
			Signal_transmitter(sigh).submit();
		}

		void read_avail_sigh(Signal_context_capability cap) override
		{
			_read_buffer.sigh(cap);
		}

		void size_changed_sigh(Signal_context_capability cap) override
		{
			_size_changed_sigh = cap;

			notify_resized(_terminal_size);
		}

		size_t read(void *, size_t) override { return 0; }
		size_t write(void const *, size_t) override { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Env                &_env;
		Read_buffer        &_read_buffer;
		Character_consumer &_character_consumer;
		Session::Size       _terminal_size { };

		Registry<Registered<Session_component> > _sessions { };

	protected:

		Session_component *_create_session(const char *) override
		{
			/*
			 * XXX read I/O buffer size from args
			 */
			size_t io_buffer_size = 4096;

			Session_component &session = *new (md_alloc())
				Registered<Session_component>(_sessions,
				                              _env,
				                              _read_buffer,
				                              _character_consumer,
				                              io_buffer_size);

			/* propagate current terminal size to client */
			session.notify_resized(_terminal_size);

			return &session;
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Env                &env,
		               Allocator          &md_alloc,
		               Read_buffer        &read_buffer,
		               Character_consumer &character_consumer)
		:
			Genode::Root_component<Session_component>(env.ep(), md_alloc),
			_env(env),
			_read_buffer(read_buffer),
			_character_consumer(character_consumer)
		{ }

		void notify_resized(Session::Size size)
		{
			_terminal_size = size;

			_sessions.for_each([&] (Session_component &session) {
				session.notify_resized(size); });
		}
};

#endif /* _SESSION_H_ */
