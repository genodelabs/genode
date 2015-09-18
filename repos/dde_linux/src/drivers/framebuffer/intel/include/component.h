/*
 * \brief  Intel framebuffer driver session component
 * \author Stefan Kalkowski
 * \date   2015-10-16
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __COMPONENT_H__
#define __COMPONENT_H__

/* Genode includes */
#include <base/rpc_server.h>
#include <root/component.h>
#include <dataspace/capability.h>
#include <framebuffer_session/framebuffer_session.h>
#include <timer_session/connection.h>
#include <util/volatile_object.h>
#include <os/attached_dataspace.h>
#include <os/attached_ram_dataspace.h>
#include <blit/blit.h>

namespace Framebuffer {
	class Session_component;
	class Root;

	extern Root * root;
	Genode::Dataspace_capability framebuffer_dataspace();
}


class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		template <typename T> using Lazy = Genode::Lazy_volatile_object<T>;

		int                                  _height;
		int                                  _width;
		Genode::Signal_context_capability    _mode_sigh;
		Timer::Connection                    _timer;
		bool const                           _buffered;
		Lazy<Genode::Attached_dataspace>     _fb_ds;
		Lazy<Genode::Attached_ram_dataspace> _bb_ds;
		bool                                 _in_update = false;
		static constexpr unsigned            _bytes_per_pixel = 2;

		void _refresh_buffered(int x, int y, int w, int h)
		{
			using namespace Genode;
			/* clip specified coordinates against screen boundaries */
			int x2 = min(x + w - 1, (int)_width  - 1),
			    y2 = min(y + h - 1, (int)_height - 1);
			int x1 = max(x, 0),
			    y1 = max(y, 0);
			if (x1 > x2 || y1 > y2) return;

			int const bpp = _bytes_per_pixel;

			/* copy pixels from back buffer to physical frame buffer */
			char *src = _bb_ds->local_addr<char>() + bpp*(_width*y1 + x1),
			     *dst = _fb_ds->local_addr<char>() + bpp*(_width*y1 + x1);

			blit(src, bpp*_width, dst, bpp*_width,
			     bpp*(x2 - x1 + 1), y2 - y1 + 1);
		}

	public:

		Session_component(bool buffered)
		: _height(0), _width(0), _buffered(buffered) {}

		void update(int height, int width)
		{
			_in_update = true;
			_height = height;
			_width  = width;

			if (_mode_sigh.valid())
				Genode::Signal_transmitter(_mode_sigh).submit();
		}


		/***********************************
		 ** Framebuffer session interface **
		 ***********************************/

		Genode::Dataspace_capability dataspace() override
		{
			_in_update = false;

			if (_fb_ds.is_constructed())
				_fb_ds.destruct();

			_fb_ds.construct(framebuffer_dataspace());
			if (!_fb_ds.is_constructed())
				PERR("framebuffer dataspace not initialized");

			if (_buffered) {
				_bb_ds.construct(Genode::env()->ram_session(),
				                 _width * _height * _bytes_per_pixel);
				if (!_bb_ds.is_constructed()) {
					PERR("buffered mode enabled, but buffer not initialized");
					return Genode::Dataspace_capability();
				}
				return _bb_ds->cap();
			}

			return _fb_ds->cap();
		}

		Mode mode() const override {
			return Mode(_width, _height, Mode::RGB565); }

		void mode_sigh(Genode::Signal_context_capability sigh) override {
			_mode_sigh = sigh; }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override {
			if (_buffered && !_in_update) _refresh_buffered(x, y, w, h); }
};


class Framebuffer::Root
: public Genode::Root_component<Framebuffer::Session_component,
                                Genode::Single_client>
{
	private:

		Session_component _single_session;

		Session_component *_create_session(const char *args) override {
			return &_single_session; }

	public:

		Root(Genode::Rpc_entrypoint *session_ep, Genode::Allocator *md_alloc,
		     bool buffered)
		: Genode::Root_component<Session_component,
		                         Genode::Single_client>(session_ep, md_alloc),
		  _single_session(buffered) { }

		void update(int height, int width) {
			_single_session.update(height, width); }
};

#endif /* __COMPONENT_H__ */
