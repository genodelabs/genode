/*
 * \brief  Framebuffer driver front end
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/env.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <framebuffer_session/framebuffer_session.h>
#include <rom_session/connection.h>
#include <util/xml_node.h>
#include <dataspace/client.h>
#include <blit/blit.h>
#include <os/config.h>
#include <timer_session/connection.h>

/* Local */
#include "framebuffer.h"

using namespace Genode;


/***************
 ** Utilities **
 ***************/

/**
 * Determine session argument value based on config file and session arguments
 *
 * \param attr_name   attribute name of config node
 * \param args        session argument string
 * \param arg_name    argument name
 * \param default     default session argument value if value is neither
 *                    specified in config node nor in session arguments
 */
unsigned long session_arg(const char *attr_name, const char *args,
                          const char *arg_name, unsigned long default_value)
{
	unsigned long result = default_value;

	/* try to obtain value from config file */
	try { Genode::config()->xml_node().attribute(attr_name).value(&result); }
	catch (...) { }

	/* check session argument to override value from config file */
	result = Arg_string::find_arg(args, arg_name).ulong_value(result);
	return result;
}


bool config_attribute(const char *attr_name)
{
	return Genode::config()->xml_node().attribute_value(attr_name, false);
}


/***********************************************
 ** Implementation of the framebuffer service **
 ***********************************************/

namespace Framebuffer {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			unsigned _scr_width, _scr_height, _scr_mode;
			bool     _buffered;

			/* dataspace uses a back buffer (if '_buffered' is true) */
			Genode::Ram_dataspace_capability _bb_ds;
			void                            *_bb_addr;

			/* dataspace of physical frame buffer */
			Genode::Dataspace_capability _fb_ds;
			void                        *_fb_addr;

			Timer::Connection _timer;

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
				if (_scr_mode == 16) bypp = 2;
				if (!bypp) return;

				/* copy pixels from back buffer to physical frame buffer */
				char *src = (char *)_bb_addr + bypp*(_scr_width*y1 + x1),
				     *dst = (char *)_fb_addr + bypp*(_scr_width*y1 + x1);

				blit(src, bypp*_scr_width, dst, bypp*_scr_width,
				     bypp*(x2 - x1 + 1), y2 - y1 + 1);
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(unsigned scr_width, unsigned scr_height, unsigned scr_mode,
			                  Genode::Dataspace_capability fb_ds, bool buffered)
			:
				_scr_width(scr_width), _scr_height(scr_height), _scr_mode(scr_mode),
				_buffered(buffered), _fb_ds(fb_ds)
			{
				if (!_buffered) return;

				if (scr_mode != 16) {
					Genode::warning("buffered mode not supported for mode ", (int)scr_mode);
					_buffered = false;
				}

				size_t buf_size = scr_width*scr_height*scr_mode/8;
				try { _bb_ds = Genode::env()->ram_session()->alloc(buf_size); }
				catch (...) {
					Genode::warning("could not allocate back buffer, disabled buffered output");
					_buffered = false;
				}

				if (_buffered && _bb_ds.valid()) {
					_bb_addr = Genode::env()->rm_session()->attach(_bb_ds);
					_fb_addr = Genode::env()->rm_session()->attach(_fb_ds);
				}

				if (_buffered)
					Genode::log("using buffered output");
			}

			/**
			 * Destructor
			 */
			~Session_component()
			{
				if (!_buffered) return;

				Genode::env()->rm_session()->detach(_bb_addr);
				Genode::env()->ram_session()->free(_bb_ds);
				Genode::env()->rm_session()->detach(_fb_addr);
			}


			/***********************************
			 ** Framebuffer session interface **
			 ***********************************/

			Dataspace_capability dataspace() override {
				return _buffered ? Dataspace_capability(_bb_ds)
				                 : Dataspace_capability(_fb_ds); }

			Mode mode() const override
			{
				return Mode(_scr_width, _scr_height,
				            _scr_mode == 16 ? Mode::RGB565 : Mode::INVALID);
			}

			void mode_sigh(Genode::Signal_context_capability) override { }

			void sync_sigh(Genode::Signal_context_capability sigh) override
			{
				_timer.sigh(sigh);
				_timer.trigger_periodic(10*1000);
			}

			void refresh(int x, int y, int w, int h) override
			{
				if (_buffered)
					_refresh_buffered(x, y, w, h);
			}
	};


	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;

	class Root : public Root_component
	{
		protected:

			Session_component *_create_session(const char *args) override
			{
				unsigned long scr_width  = session_arg("width",  args, "fb_width",  0),
				              scr_height = session_arg("height", args, "fb_height", 0),
				              scr_mode   = session_arg("depth",  args, "fb_mode",   16);
				bool          buffered   = config_attribute("buffered");

				if (Framebuffer_drv::set_mode(scr_width, scr_height, scr_mode) != 0) {
					Genode::warning("Could not set vesa mode ",
					                scr_width, "x", scr_height, "@", scr_mode);
					throw Root::Invalid_args();
				}

				Genode::log("using video mode: ", scr_width, "x", scr_height, "@", scr_mode);

				return new (md_alloc()) Session_component(scr_width, scr_height, scr_mode,
				                                          Framebuffer_drv::hw_framebuffer(),
				                                          buffered);
			}

		public:

			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc)
			: Root_component(session_ep, md_alloc) { }
	};
}


int main(int argc, char **argv)
{
	/* We need to create capabilities for sessions. Therefore, we request the
	 * CAP service. */
	static Cap_connection cap;

	/* initialize server entry point */
	enum { STACK_SIZE = 8*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "vesa_ep");

	/* init driver back end */
	if (Framebuffer_drv::init()) {
		Genode::error("H/W driver init failed");
		return 3;
	}

	/* entry point serving framebuffer root interface */
	static Framebuffer::Root fb_root(&ep, env()->heap());

	/* tell parent about the service */
	env()->parent()->announce(ep.manage(&fb_root));

	/* main's done - go to sleep */

	sleep_forever();
	return 0;
}
