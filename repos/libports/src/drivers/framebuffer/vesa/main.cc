/*
 * \brief  Framebuffer driver front end
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <root/component.h>
#include <util/reconstructible.h>
#include <util/arg_string.h>
#include <blit/blit.h>
#include <timer_session/connection.h>
#include <framebuffer_session/framebuffer_session.h>

/* local includes */
#include "framebuffer.h"


namespace Framebuffer {
	struct Session_component;
	struct Root;
	struct Main;

	using Genode::size_t;
	using Genode::min;
	using Genode::max;
	using Genode::Dataspace_capability;
	using Genode::Attached_rom_dataspace;
	using Genode::Attached_ram_dataspace;
}


class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		Genode::Env &_env;

		unsigned const _scr_width, _scr_height, _scr_depth;

		Timer::Connection _timer { _env };

		/* dataspace of physical frame buffer */
		Dataspace_capability  _fb_cap;
		void                 *_fb_addr;

		/* dataspace uses a back buffer (if '_buffered' is true) */
		Genode::Constructible<Attached_ram_dataspace> _bb;

		void _refresh_buffered(int x, int y, int w, int h)
		{
			/* clip specified coordinates against screen boundaries */
			int x2 = min(x + w - 1, (int)_scr_width  - 1),
			    y2 = min(y + h - 1, (int)_scr_height - 1);
			int x1 = max(x, 0),
			    y1 = max(y, 0);
			if (x1 > x2 || y1 > y2) return;

			/* determine bytes per pixel */
			int bypp = 0;
			if (_scr_depth == 16) bypp = 2;
			if (!bypp) return;

			/* copy pixels from back buffer to physical frame buffer */
			char *src = _bb->local_addr<char>() + bypp*(_scr_width*y1 + x1),
			     *dst = (char *)_fb_addr + bypp*(_scr_width*y1 + x1);

			blit(src, bypp*_scr_width, dst, bypp*_scr_width,
			     bypp*(x2 - x1 + 1), y2 - y1 + 1);
		}

		bool _buffered() const { return _bb.constructed(); }

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Env &env,
		                  unsigned scr_width, unsigned scr_height, unsigned scr_depth,
		                  Dataspace_capability fb_cap, bool buffered)
		:
			_env(env),
			_scr_width(scr_width), _scr_height(scr_height), _scr_depth(scr_depth),
			_fb_cap(fb_cap)
		{
			if (!buffered) return;

			if (_scr_depth != 16) {
				Genode::warning("buffered mode not supported for depth ", _scr_depth);
				return;
			}

			size_t const bb_size = _scr_width*_scr_height*_scr_depth/8;
			try { _bb.construct(env.ram(), env.rm(), bb_size); }
			catch (...) {
				Genode::warning("could not allocate back buffer, disabled buffered output");
				return;
			}

			_fb_addr = env.rm().attach(_fb_cap);

			Genode::log("using buffered output");
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			if (_buffered()) {
				_bb.destruct();
				_env.rm().detach(_fb_addr);
			}
		}


		/***********************************
		 ** Framebuffer session interface **
		 ***********************************/

		Dataspace_capability dataspace() override
		{
			return _buffered() ? Dataspace_capability(_bb->cap())
			                   : Dataspace_capability(_fb_cap);
		}

		Mode mode() const override
		{
			return Mode(_scr_width, _scr_height,
			            _scr_depth == 16 ? Mode::RGB565 : Mode::INVALID);
		}

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override
		{
			if (_buffered())
				_refresh_buffered(x, y, w, h);
		}
};


/**
 * Shortcut for single-client root component
 */
typedef Genode::Root_component<Framebuffer::Session_component,
                               Genode::Single_client> Root_component;

class Framebuffer::Root : public Root_component
{
	private:

		Genode::Env &_env;

		Attached_rom_dataspace const &_config;

		unsigned _session_arg(char const *attr_name, char const *args,
		                      char const *arg_name, unsigned default_value)
		{
			/* try to obtain value from config file */
			unsigned result = _config.xml().attribute_value(attr_name,
			                                                default_value);

			/* check session argument to override value from config file */
			return Genode::Arg_string::find_arg(args, arg_name).ulong_value(result);
		}

	protected:

		Session_component *_create_session(char const *args) override
		{
			unsigned       scr_width  = _session_arg("width",  args, "fb_width",  0);
			unsigned       scr_height = _session_arg("height", args, "fb_height", 0);
			unsigned const scr_depth  = _session_arg("depth",  args, "fb_mode",   16);

			bool const buffered = _config.xml().attribute_value("buffered", false);

			if (Framebuffer::set_mode(scr_width, scr_height, scr_depth) != 0) {
				Genode::warning("Could not set vesa mode ",
				                scr_width, "x", scr_height, "@", scr_depth);
				throw Root::Invalid_args();
			}

			Genode::log("using video mode: ",
			            scr_width, "x", scr_height, "@", scr_depth);

			return new (md_alloc()) Session_component(_env,
			                                          scr_width, scr_height, scr_depth,
			                                          Framebuffer::hw_framebuffer(),
			                                          buffered);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &alloc,
		     Attached_rom_dataspace const &config)
		:
			Root_component(&env.ep().rpc_ep(), &alloc),
			_env(env), _config(config)
		{ }
};


struct Framebuffer::Main
{
	Genode::Env &env;

	Genode::Heap heap { env.ram(), env.rm() };

	Attached_rom_dataspace config { env, "config" };

	Root root { env, heap, config };

	Main(Genode::Env &env) : env(env)
	{
		try { Framebuffer::init(env, heap); } catch (...) {
			Genode::error("H/W driver init failed");
			throw;
		}

		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Framebuffer::Main inst(env); }
