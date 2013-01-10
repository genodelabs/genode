/*
 * \brief  Client-side Terminal session interface
 * \author Norman Feske
 * \date   2011-08-11
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TERMINAL_SESSION__CLIENT_H_
#define _INCLUDE__TERMINAL_SESSION__CLIENT_H_

/* Genode includes */
#include <util/misc_math.h>
#include <util/string.h>
#include <base/lock.h>
#include <base/env.h>
#include <base/rpc_client.h>

#include <terminal_session/terminal_session.h>

namespace Terminal {

	class Session_client : public Genode::Rpc_client<Session>
	{
		private:

			/**
			 * Shared-memory buffer used for carrying the payload
			 * of read/write operations
			 */
			struct Io_buffer
			{
				Genode::Dataspace_capability ds_cap;
				char                        *base;
				Genode::size_t               size;
				Genode::Lock                 lock;

				Io_buffer(Genode::Dataspace_capability ds_cap)
				:
					ds_cap(ds_cap),
					base(Genode::env()->rm_session()->attach(ds_cap)),
					size(ds_cap.call<Genode::Dataspace::Rpc_size>())
				{ }

				~Io_buffer()
				{
					Genode::env()->rm_session()->detach(base);
				}
			};

			Io_buffer _io_buffer;

		public:

			Session_client(Genode::Capability<Session> cap)
			:
				Genode::Rpc_client<Session>(cap),
				_io_buffer(call<Rpc_dataspace>())
			{ }

			Size size() { return call<Rpc_size>(); }

			bool avail() { return call<Rpc_avail>(); }

			Genode::size_t read(void *buf, Genode::size_t buf_size)
			{
				Genode::Lock::Guard _guard(_io_buffer.lock);

				/* instruct server to fill the I/O buffer */
				Genode::size_t num_bytes = call<Rpc_read>(buf_size);

				/* copy-out I/O buffer */
				num_bytes = Genode::min(num_bytes, buf_size);
				Genode::memcpy(buf, _io_buffer.base, num_bytes);

				return num_bytes;
			}

			Genode::size_t write(void const *buf, Genode::size_t num_bytes)
			{
				Genode::Lock::Guard _guard(_io_buffer.lock);

				Genode::size_t     written_bytes = 0;
				char const * const src           = (char const *)buf;

				while (written_bytes < num_bytes) {

					/* copy payload to I/O buffer */
					Genode::size_t n = Genode::min(num_bytes - written_bytes,
					                               _io_buffer.size);
					Genode::memcpy(_io_buffer.base, src + written_bytes, n);

					/* tell server to pick up new I/O buffer content */
					call<Rpc_write>(n);

					written_bytes += n;
				}
				return num_bytes;
			}

			void connected_sigh(Genode::Signal_context_capability cap)
			{
				call<Rpc_connected_sigh>(cap);
			}

			void read_avail_sigh(Genode::Signal_context_capability cap)
			{
				call<Rpc_read_avail_sigh>(cap);
			}

			Genode::size_t io_buffer_size() const { return _io_buffer.size; }
	};
}

#endif /* _INCLUDE__TERMINAL_SESSION__CLIENT_H_ */
