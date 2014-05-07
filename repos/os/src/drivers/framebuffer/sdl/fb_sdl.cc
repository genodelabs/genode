/*
 * \brief  SDL-based implementation of the Genode framebuffer
 * \author Norman Feske
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux */
#include <SDL/SDL.h>

/* Genode */
#include <util/misc_math.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <framebuffer_session/framebuffer_session.h>
#include <cap_session/connection.h>
#include <input/component.h>


/*
 * Variables for the libSDL output window
 */
static SDL_Surface *screen;
static int scr_width = 1024, scr_height = 768;
static Framebuffer::Mode::Format scr_format = Framebuffer::Mode::RGB565;

/*
 * Dataspace capability and local address of virtual frame buffer
 * that is shared with the client.
 */
static Genode::Dataspace_capability fb_ds_cap;
static void                        *fb_ds_addr;


/***********************************************
 ** Implementation of the framebuffer service **
 ***********************************************/

namespace Framebuffer {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Mode _mode;

			Genode::Signal_context_capability _sync_sigh;

		public:

			/**
			 * Constructor
			 */
			Session_component() : _mode(scr_width, scr_height, Mode::RGB565) { }

			Genode::Dataspace_capability dataspace() override { return fb_ds_cap; }

			Mode mode() const override { return _mode; }

			void mode_sigh(Genode::Signal_context_capability) override { }

			void sync_sigh(Genode::Signal_context_capability sigh) override
			{
				_sync_sigh = sigh;
			}

			void refresh(int x, int y, int w, int h) override
			{
				/* clip refresh area to screen boundaries */
				int x1 = Genode::max(x, 0);
				int y1 = Genode::max(y, 0);
				int x2 = Genode::min(x + w - 1, scr_width  - 1);
				int y2 = Genode::min(y + h - 1, scr_height - 1);

				if (x1 <= x2 && y1 <= y2) {

					/* copy pixels from shared dataspace to sdl surface */
					const int start_offset = _mode.bytes_per_pixel()*(y1*scr_width + x1);
					const int line_len     = _mode.bytes_per_pixel()*(x2 - x1 + 1);
					const int pitch        = _mode.bytes_per_pixel()*scr_width;

					char *src = (char *)fb_ds_addr     + start_offset;
					char *dst = (char *)screen->pixels + start_offset;

					for (int i = y1; i <= y2; i++, src += pitch, dst += pitch)
						Genode::memcpy(dst, src, line_len);

					/* flush pixels in sdl window */
					SDL_UpdateRect(screen, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
				}

				if (_sync_sigh.valid())
					Genode::Signal_transmitter(_sync_sigh).submit();
			}
	};


	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;


	class Root : public Root_component
	{
		protected:

			Session_component *_create_session(const char *args) override {
				return new (md_alloc()) Session_component(); }

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc)
			: Root_component(session_ep, md_alloc) { }
	};
}


/**
 * Main program
 */
extern "C" int main(int, char**)
{
	using namespace Genode;
	using namespace Framebuffer;

	/*
	 * FIXME: Read configuration (screen parameters)
	 */

	/*
	 * Initialize libSDL window
	 */
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		PERR("SDL_Init failed\n");
		return -1;
	}

	Genode::size_t bpp = Framebuffer::Mode::bytes_per_pixel(scr_format);
	screen = SDL_SetVideoMode(scr_width, scr_height, bpp*8, SDL_SWSURFACE);
	SDL_ShowCursor(0);

	Genode::printf("creating virtual framebuffer for mode %dx%d@%zd\n",
	               scr_width, scr_height, bpp*8);

	/*
	 * Create dataspace representing the virtual frame buffer
	 */
	try {
		static Attached_ram_dataspace fb_ds(env()->ram_session(), scr_width*scr_height*bpp);
		fb_ds_cap  = fb_ds.cap();
		fb_ds_addr = fb_ds.local_addr<void>();
	} catch (...) {
		PERR("Could not allocate dataspace for virtual frame buffer");
		return -2;
	}

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 16*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_ep");

	/*
	 * Let the entry point serve the framebuffer and input root interfaces
	 */
	static Framebuffer::Root framebuffer_root(&ep, env()->heap());
	static Input::Root       input_root(&ep, env()->heap());

	/*
	 * Now, the root interfaces are ready to accept requests.
	 * This is the right time to tell mummy about our services.
	 */
	env()->parent()->announce(ep.manage(&framebuffer_root));
	env()->parent()->announce(ep.manage(&input_root));

	sleep_forever();
	return 0;
}
