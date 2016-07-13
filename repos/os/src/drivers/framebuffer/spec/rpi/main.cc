/*
 * \brief  Framebuffer driver for Raspberry Pi
 * \author Norman Feske
 * \date   2013-09-14
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/volatile_object.h>
#include <os/attached_io_mem_dataspace.h>
#include <os/attached_ram_dataspace.h>
#include <os/static_root.h>
#include <os/config.h>
#include <cap_session/connection.h>
#include <base/sleep.h>
#include <framebuffer_session/framebuffer_session.h>
#include <base/rpc_server.h>
#include <platform_session/connection.h>
#include <blit/blit.h>
#include <timer_session/connection.h>

namespace Framebuffer {
	using namespace Genode;
	class Session_component;
};


class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		size_t                                 const _width;
		size_t                                 const _height;
		Lazy_volatile_object<Attached_ram_dataspace> _bb_mem;
		Attached_io_mem_dataspace                    _fb_mem;
		Timer::Connection                            _timer;

		void _refresh_buffered(int x, int y, int w, int h)
		{
			Mode _mode = mode();

			/* clip specified coordinates against screen boundaries */
			int x2 = min(x + w - 1, (int)_mode.width()  - 1),
				y2 = min(y + h - 1, (int)_mode.height() - 1);
			int x1 = max(x, 0),
				y1 = max(y, 0);
			if (x1 > x2 || y1 > y2) return;

			int bypp = _mode.bytes_per_pixel();

			/* copy pixels from back buffer to physical frame buffer */
			char *src = _bb_mem->local_addr<char>() + bypp*(_width*y1 + x1),
			     *dst = _fb_mem.local_addr<char>() + bypp*(_width*y1 + x1);

			blit(src, bypp*_width, dst, bypp*_width,
				 bypp*(x2 - x1 + 1), y2 - y1 + 1);
		}

	public:

		Session_component(addr_t phys_addr, size_t size,
		                  size_t width, size_t height,
		                  bool buffered)
		:
			_width(width), _height(height), _fb_mem(phys_addr, size)
		{
			if (buffered) {
				_bb_mem.construct(env()->ram_session(), size);
			}
		}

		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override
		{
			if (_bb_mem.constructed())
				return _bb_mem->cap();
			else
				return _fb_mem.cap();
		}

		Mode mode() const override
		{
			return Mode(_width, _height, Mode::RGB565);
		}

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override
		{
			if (_bb_mem.constructed())
				_refresh_buffered(x, y, w, h);
		}
};


static bool config_is_buffered()
{
	return Genode::config()->xml_node().attribute_value("buffered", false);
}


int main(int, char **)
{
	using namespace Framebuffer;
	using namespace Genode;

	log("--- fb_drv started ---");

	static Platform::Connection platform;

	Platform::Framebuffer_info fb_info(1024, 768, 16);
	platform.setup_framebuffer(fb_info);

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_ep");

	/*
	 * Let the entry point serve the framebuffer session and root interfaces
	 */
	static Session_component fb_session(fb_info.addr,
	                                    fb_info.size,
	                                    fb_info.phys_width,
	                                    fb_info.phys_height,
	                                    config_is_buffered());
	static Static_root<Framebuffer::Session> fb_root(ep.manage(&fb_session));

	/*
	 * Announce service
	 */
	env()->parent()->announce(ep.manage(&fb_root));

	sleep_forever();
	return 0;
}
