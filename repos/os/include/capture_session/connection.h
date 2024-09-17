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

#include <capture_session/capture_session.h>
#include <base/connection.h>
#include <base/attached_dataspace.h>
#include <os/texture.h>
#include <blit/painter.h>

namespace Capture { class Connection; }


class Capture::Connection : private Genode::Connection<Session>
{
	private:

		size_t _session_quota = 0;

	public:

		using Label = Genode::Session_label;

		/**
		 * Constructor
		 */
		Connection(Genode::Env &env, Label const &label = Label())
		:
			Genode::Connection<Capture::Session>(env, label,
			                                     Ram_quota { 36*1024 }, Args())
		{ }

		void buffer(Session::Buffer_attr attr)
		{
			size_t const needed  = Session::buffer_bytes(attr.px);
			size_t const upgrade = needed > _session_quota
			                     ? needed - _session_quota
			                     : 0;
			if (upgrade > 0) {
				this->upgrade_ram(upgrade);
				_session_quota += upgrade;
			}

			for (;;) {
				using Result = Session::Buffer_result;
				switch (cap().call<Session::Rpc_buffer>(attr)) {
				case Result::OUT_OF_RAM:  upgrade_ram(8*1024); break;
				case Result::OUT_OF_CAPS: upgrade_caps(2);     break;
				case Result::OK:
					return;
				}
			}
		}

		struct Screen;

		Area screen_size() const { return cap().call<Session::Rpc_screen_size>(); }

		void screen_size_sigh(Signal_context_capability sigh)
		{
			cap().call<Session::Rpc_screen_size_sigh>(sigh);
		}

		void wakeup_sigh(Signal_context_capability sigh)
		{
			cap().call<Session::Rpc_wakeup_sigh>(sigh);
		}

		Genode::Dataspace_capability dataspace()
		{
			return cap().call<Session::Rpc_dataspace>();
		}

		Session::Affected_rects capture_at(Point pos)
		{
			return cap().call<Session::Rpc_capture_at>(pos);
		}

		void capture_stopped() { cap().call<Session::Rpc_capture_stopped>(); }
};


class Capture::Connection::Screen
{
	public:

		struct Attr
		{
			Area px;  /* buffer area in pixels */
			Area mm;  /* physical size in millimeters */
		};

		Attr const attr;

	private:

		Capture::Connection &_connection;

		bool const _buffer_initialized = (
			_connection.buffer({ .px = attr.px, .mm = attr.mm }), true );

		Attached_dataspace _ds;

		Texture<Pixel> const _texture { _ds.local_addr<Pixel>(), nullptr, attr.px };

	public:

		Screen(Capture::Connection &connection, Region_map &rm, Attr attr)
		:
			attr(attr), _connection(connection), _ds(rm, _connection.dataspace())
		{ }

		void with_texture(auto const &fn) const { fn(_texture); }

		Rect apply_to_surface(Surface<Pixel> &surface)
		{
			Rect bounding_box { };

			using Affected_rects = Session::Affected_rects;

			Affected_rects const affected = _connection.capture_at(Capture::Point(0, 0));

			with_texture([&] (Texture<Pixel> const &texture) {

				affected.for_each_rect([&] (Capture::Rect const rect) {

					surface.clip(rect);

					Blit_painter::paint(surface, texture, Capture::Point(0, 0));

					bounding_box = bounding_box.area.count()
					             ? Rect::compound(bounding_box, rect)
					             : rect;
				});
			});
			return bounding_box;
		}
};

#endif /* _INCLUDE__CAPTURE_SESSION__CONNECTION_H_ */
