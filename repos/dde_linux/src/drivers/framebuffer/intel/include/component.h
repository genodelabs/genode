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

struct drm_display_mode;
struct drm_connector;
struct drm_framebuffer;

namespace Framebuffer {
	class Driver;
	class Session_component;
	class Root;
}


class Framebuffer::Driver
{
	private:

		Session_component        &_session;
		int                       _height = 0;
		int                       _width  = 0;
		static constexpr unsigned _bytes_per_pixel = 2;
		void                     *_new_fb_ds_base = nullptr;
		void                     *_cur_fb_ds_base = nullptr;
		Genode::uint64_t          _cur_fb_ds_size = 0;
		drm_framebuffer          *_new_fb = nullptr;
		drm_framebuffer          *_cur_fb = nullptr;

		drm_display_mode * _preferred_mode(drm_connector *connector);

	public:

		Driver(Session_component &session) : _session(session) {}

		int      width()  const { return _width;           }
		int      height() const { return _height;          }
		unsigned bpp()    const { return _bytes_per_pixel; }

		Genode::size_t size() const {
			return _width * _height * _bytes_per_pixel; }

		void finish_initialization();
		bool mode_changed();
		void generate_report();
		void free_framebuffer();
		Genode::Dataspace_capability dataspace();
};


class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		template <typename T> using Lazy = Genode::Lazy_volatile_object<T>;

		Driver                               _driver;
		Genode::Signal_context_capability    _mode_sigh;
		Timer::Connection                    _timer;
		bool const                           _buffered;
		Lazy<Genode::Attached_dataspace>     _fb_ds;
		Lazy<Genode::Attached_ram_dataspace> _bb_ds;
		bool                                 _in_update = false;

		void _refresh_buffered(int x, int y, int w, int h)
		{
			using namespace Genode;

			int width = _driver.width(), height = _driver.height();
			unsigned bpp = _driver.bpp();

			/* clip specified coordinates against screen boundaries */
			int x2 = min(x + w - 1, width  - 1),
			    y2 = min(y + h - 1, height - 1);
			int x1 = max(x, 0),
			    y1 = max(y, 0);
			if (x1 > x2 || y1 > y2) return;

			/* copy pixels from back buffer to physical frame buffer */
			char *src = _bb_ds->local_addr<char>() + bpp*(width*y1 + x1),
			     *dst = _fb_ds->local_addr<char>() + bpp*(width*y1 + x1);

			blit(src, bpp*width, dst, bpp*width,
			     bpp*(x2 - x1 + 1), y2 - y1 + 1);
		}

	public:

		Session_component(bool buffered)
		: _driver(*this), _buffered(buffered) {}

		Driver & driver() { return _driver; }

		void config_changed()
		{
			_in_update = true;
			if (_driver.mode_changed() && _mode_sigh.valid())
				Genode::Signal_transmitter(_mode_sigh).submit();
			else
				_in_update = false;
		}


		/***********************************
		 ** Framebuffer session interface **
		 ***********************************/

		Genode::Dataspace_capability dataspace() override
		{
			_in_update = false;

			if (_fb_ds.constructed())
				_fb_ds.destruct();

			_fb_ds.construct(_driver.dataspace());
			if (!_fb_ds.is_constructed())
				PERR("framebuffer dataspace not initialized");

			if (_buffered) {
				if (_bb_ds.is_constructed())
					_bb_ds.destruct();

				_bb_ds.construct(Genode::env()->ram_session(), _driver.size());
				if (!_bb_ds.is_constructed()) {
					PERR("buffered mode enabled, but buffer not initialized");
					return Genode::Dataspace_capability();
				}
				return _bb_ds->cap();
			}

			return _fb_ds->cap();
		}

		Mode mode() const override {
			return Mode(_driver.width(), _driver.height(), Mode::RGB565); }

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


struct Framebuffer::Root
: Genode::Root_component<Framebuffer::Session_component, Genode::Single_client>
{
	Session_component session; /* single session */

	Session_component *_create_session(const char *args) override {
		return &session; }

	Root(Genode::Rpc_entrypoint *ep, Genode::Allocator *alloc, bool buffered)
	: Genode::Root_component<Session_component,
	                         Genode::Single_client>(ep, alloc),
	  session(buffered) { }
};

#endif /* __COMPONENT_H__ */
