/*
 * \brief  Client-side Terminal session interface
 * \author Norman Feske
 * \date   2011-08-11
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TERMINAL_SESSION__CLIENT_H_
#define _INCLUDE__TERMINAL_SESSION__CLIENT_H_

/* Genode includes */
#include <util/misc_math.h>
#include <util/string.h>
#include <base/mutex.h>
#include <base/rpc_client.h>
#include <base/attached_dataspace.h>

#include <terminal_session/terminal_session.h>

namespace Terminal { class Session_client; }


class Terminal::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Genode::Mutex _mutex { };

		/**
		 * Shared-memory buffer used for carrying the payload
		 * of read/write operations
		 */
		Genode::Attached_dataspace _io_buffer;

	public:

		Session_client(Genode::Region_map &local_rm, Genode::Capability<Session> cap)
		:
			Genode::Rpc_client<Session>(cap),
			_io_buffer(local_rm, call<Rpc_dataspace>())
		{ }

		Size size() override { return call<Rpc_size>(); }

		bool avail() override { return call<Rpc_avail>(); }

		Genode::size_t read(void *buf, Genode::size_t buf_size) override
		{
			Genode::Mutex::Guard _guard(_mutex);

			/* instruct server to fill the I/O buffer */
			Genode::size_t num_bytes = call<Rpc_read>(buf_size);

			/* copy-out I/O buffer */
			num_bytes = Genode::min(num_bytes, buf_size);
			Genode::memcpy(buf, _io_buffer.local_addr<char>(), num_bytes);

			return num_bytes;
		}

		Genode::size_t write(void const *buf, Genode::size_t num_bytes) override
		{
			Genode::Mutex::Guard _guard(_mutex);

			Genode::size_t     written_bytes = 0;
			char const * const src           = (char const *)buf;

			while (written_bytes < num_bytes) {

				/* copy payload to I/O buffer */
				Genode::size_t payload_bytes = Genode::min(num_bytes - written_bytes,
				                                           _io_buffer.size());
				Genode::memcpy(_io_buffer.local_addr<char>(),
				               src + written_bytes, payload_bytes);

				/* tell server to pick up new I/O buffer content */
				Genode::size_t written_payload_bytes = call<Rpc_write>(payload_bytes);

				written_bytes += written_payload_bytes;

				if (written_payload_bytes != payload_bytes)
					return written_bytes;

			}
			return written_bytes;
		}

		void connected_sigh(Genode::Signal_context_capability cap) override
		{
			call<Rpc_connected_sigh>(cap);
		}

		void read_avail_sigh(Genode::Signal_context_capability cap) override
		{
			call<Rpc_read_avail_sigh>(cap);
		}

		void size_changed_sigh(Genode::Signal_context_capability cap) override
		{
			call<Rpc_size_changed_sigh>(cap);
		}

		Genode::size_t io_buffer_size() const { return _io_buffer.size(); }
};

#endif /* _INCLUDE__TERMINAL_SESSION__CLIENT_H_ */
