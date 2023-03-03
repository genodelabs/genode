/*
 * \brief  Connection to capture service
 * \author Norman Feske
 * \date   2020-06-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CAPTURE_SESSION__CONNECTION_H_
#define _INCLUDE__CAPTURE_SESSION__CONNECTION_H_

#include <capture_session/client.h>
#include <base/connection.h>
#include <base/attached_dataspace.h>
#include <os/texture.h>
#include <blit/painter.h>

namespace Capture { class Connection; }


class Capture::Connection : public Genode::Connection<Session>,
                            public Session_client
{
	private:

		size_t _session_quota = 0;

	public:

		/**
		 * Constructor
		 */
		Connection(Genode::Env &env, Label const &label = Label())
		:
			Genode::Connection<Capture::Session>(env, label,
			                                     Ram_quota { 36*1024 }, Args()),
			Session_client(cap())
		{ }

		void buffer(Area size) override
		{
			size_t const needed  = buffer_bytes(size);
			size_t const upgrade = needed > _session_quota
			                     ? needed - _session_quota
			                     : 0;
			if (upgrade > 0) {
				this->upgrade_ram(upgrade);
				_session_quota += upgrade;
			}

			Session_client::buffer(size);
		}

		struct Screen;
};


class Capture::Connection::Screen
{
	public:

		Area const size;

	private:

		Capture::Connection &_connection;

		bool const _buffer_initialized = ( _connection.buffer(size), true );

		Attached_dataspace _ds;

		Texture<Pixel> const _texture { _ds.local_addr<Pixel>(), nullptr, size };

	public:

		Screen(Capture::Connection &connection, Region_map &rm, Area size)
		:
			size(size), _connection(connection), _ds(rm, _connection.dataspace())
		{ }

		template <typename FN>
		void with_texture(FN const &fn) const
		{
			fn(_texture);
		}

		void apply_to_surface(Surface<Pixel> &surface)
		{
			Affected_rects const affected = _connection.capture_at(Capture::Point(0, 0));

			with_texture([&] (Texture<Pixel> const &texture) {

				affected.for_each_rect([&] (Capture::Rect const rect) {

					surface.clip(rect);

					Blit_painter::paint(surface, texture, Capture::Point(0, 0));
				});
			});
		}
};

#endif /* _INCLUDE__CAPTURE_SESSION__CONNECTION_H_ */
