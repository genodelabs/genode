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


class Terminal::Session_client : public Rpc_client<Session>
{
	private:

		using Local_rm = Local::Constrained_region_map;

		Mutex _mutex { };

		/**
		 * Shared-memory buffer used for carrying the payload
		 * of read/write operations
		 */
		Attached_dataspace _io_buffer;

	public:

		Session_client(Local_rm &local_rm, Capability<Session> cap)
		:
			Rpc_client<Session>(cap),
			_io_buffer(local_rm, call<Rpc_dataspace>())
		{ }

		Size size() override { return call<Rpc_size>(); }

		bool avail() override { return call<Rpc_avail>(); }

		size_t read(void *buf, size_t buf_size) override
		{
			Mutex::Guard _guard(_mutex);

			/* instruct server to fill the I/O buffer */
			size_t num_bytes = call<Rpc_read>(buf_size);

			/* copy-out I/O buffer */
			num_bytes = min(num_bytes, buf_size);
			Genode::memcpy(buf, _io_buffer.local_addr<char>(), num_bytes);

			return num_bytes;
		}

		size_t write(void const *buf, size_t num_bytes) override
		{
			Mutex::Guard _guard(_mutex);

			size_t             written_bytes = 0;
			char const * const src           = (char const *)buf;

			while (written_bytes < num_bytes) {

				/* copy payload to I/O buffer */
				size_t payload_bytes = Genode::min(num_bytes - written_bytes,
				                                   _io_buffer.size());
				Genode::memcpy(_io_buffer.local_addr<char>(),
				               src + written_bytes, payload_bytes);

				/* tell server to pick up new I/O buffer content */
				size_t written_payload_bytes = call<Rpc_write>(payload_bytes);

				written_bytes += written_payload_bytes;

				if (written_payload_bytes != payload_bytes)
					return written_bytes;

			}
			return written_bytes;
		}

		void connected_sigh(Signal_context_capability cap) override
		{
			call<Rpc_connected_sigh>(cap);
		}

		void read_avail_sigh(Signal_context_capability cap) override
		{
			call<Rpc_read_avail_sigh>(cap);
		}

		void size_changed_sigh(Signal_context_capability cap) override
		{
			call<Rpc_size_changed_sigh>(cap);
		}

		size_t io_buffer_size() const { return _io_buffer.size(); }
};

#endif /* _INCLUDE__TERMINAL_SESSION__CLIENT_H_ */
