/*
 * \brief  Framebuffer driver for Raspberry Pi
 * \author Norman Feske
 * \date   2013-09-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <util/reconstructible.h>
#include <os/static_root.h>
#include <framebuffer_session/framebuffer_session.h>
#include <platform_session/connection.h>
#include <blit/blit.h>
#include <timer_session/connection.h>

namespace Framebuffer {
	using namespace Genode;
	class Session_component;
	struct Main;
};


class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		size_t                          const _width;
		size_t                          const _height;
		Constructible<Attached_ram_dataspace> _bb_mem;
		Attached_io_mem_dataspace             _fb_mem;
		Timer::Connection                     _timer;

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

		Session_component(Genode::Env &env, addr_t phys_addr,
		                  size_t size, size_t width, size_t height,
		                  bool buffered)
		:
			_width(width), _height(height), _fb_mem(env, phys_addr, size),
			_timer(env)
		{
			if (buffered) {
				_bb_mem.construct(env.ram(), env.rm(), size);
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


static bool config_buffered(Genode::Xml_node node)
{
	return node.attribute_value("buffered", false);
}


struct Framebuffer::Main
{
	Env        &_env;
	Entrypoint &_ep;

	Attached_rom_dataspace _config { _env, "config" };

	Platform::Connection _platform { _env };

	Platform::Framebuffer_info _fb_info {1024, 768, 16 };

	Constructible<Framebuffer::Session_component>    _fb_session;
	Constructible<Static_root<Framebuffer::Session>> _fb_root;

	Main(Genode::Env &env) : _env(env), _ep(_env.ep())
	{
		log("--- fb_drv started ---");

		_platform.setup_framebuffer(_fb_info);

		_fb_session.construct(_env, _fb_info.addr, _fb_info.size,
		                      _fb_info.phys_width, _fb_info.phys_height,
		                      config_buffered(_config.xml()));

		_fb_root.construct(_ep.manage(*_fb_session));

		/* announce service */
		env.parent().announce(_ep.manage(*_fb_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Framebuffer::Main main(env);
}
