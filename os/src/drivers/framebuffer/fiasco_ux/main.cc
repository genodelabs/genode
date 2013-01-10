/**
 * \brief  Framebuffer driver front-end
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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

/* Local */
#include "framebuffer.h"

using namespace Genode;


/***********************************************
 ** Implementation of the framebuffer service **
 ***********************************************/

/*
 * Screen configuration
 *
 * FIXME currently it's 640x480@16 and not configurable
 */
static int scr_width = 640, scr_height = 480, scr_mode = 16;

namespace Framebuffer {

	class Session_component : public Genode::Rpc_object<Session>
	{
		public:

			Dataspace_capability dataspace() { return Framebuffer_drv::hw_framebuffer(); }

			void release() { }

			Mode mode() const
			{
				if (scr_mode != 16)
					return Mode(); /* invalid mode */

				return Mode(scr_width, scr_height, Mode::RGB565);
			}

			void mode_sigh(Genode::Signal_context_capability sigh) { }

			void refresh(int x, int y, int w, int h)
			{
#if 0
				/* clip refresh area to screen boundaries */
				int x1 = max(x, 0);
				int y1 = max(y, 0);
				int x2 = min(x + w - 1, scr_width  - 1);
				int y2 = min(y + h - 1, scr_height - 1);

				if (x1 > x2 || y1 > y2) return;

				/* copy pixels from shared dataspace to sdl surface */
				const int start_offset    = bytes_per_pixel()*(y1*scr_width + x1);
				const int line_len        = bytes_per_pixel()*(x2 - x1 + 1);
				const int pitch           = bytes_per_pixel()*scr_width;

				char *src = (char *)fb_ds_addr     + start_offset;
				char *dst = (char *)screen->pixels + start_offset;

				for (int i = y1; i <= y2; i++, src += pitch, dst += pitch)
					Genode::memcpy(dst, src, line_len);

				/* flush pixels in sdl window */
				SDL_UpdateRect(screen, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
#endif
			}
	};


	class Root : public Root_component<Session_component>
	{
		protected:

			Session_component *_create_session(const char *args) {
				return new (md_alloc()) Session_component(); }

		public:

			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc)
			: Root_component<Session_component>(session_ep, md_alloc) { }
	};
}


int main(int argc, char **argv)
{
	/* initialize server entry point */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_ep");

	/* init driver back-end */
	if (Framebuffer_drv::init()) {
		PERR("H/W driver init failed");
		return 3;
	}

	static Framebuffer::Root fb_root(&ep, env()->heap());

	/* tell parent about the service */
	env()->parent()->announce(ep.manage(&fb_root));

	/* main's done - go to sleep */

	sleep_forever();
	return 0;
}
