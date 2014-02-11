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
#include <os/attached_io_mem_dataspace.h>
#include <os/static_root.h>
#include <cap_session/connection.h>
#include <base/sleep.h>
#include <framebuffer_session/framebuffer_session.h>
#include <base/rpc_server.h>
#include <platform_session/connection.h>

namespace Framebuffer {
	using namespace Genode;
	class Session_component;
};


class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		size_t const _width;
		size_t const _height;
		Attached_io_mem_dataspace _fb_mem;

	public:

		Session_component(addr_t phys_addr, size_t size, size_t width, size_t height)
		:
			_width(width), _height(height), _fb_mem(phys_addr, size)
		{ }

		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() { return _fb_mem.cap(); }

		Mode mode() const
		{
			return Mode(_width, _height, Mode::RGB565);
		}

		void mode_sigh(Genode::Signal_context_capability) { }

		void refresh(int, int, int, int) { }
};


int main(int, char **)
{
	using namespace Framebuffer;
	using namespace Genode;

	printf("--- fb_drv started ---\n");

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
	                                    fb_info.phys_height);
	static Static_root<Framebuffer::Session> fb_root(ep.manage(&fb_session));

	/*
	 * Announce service
	 */
	env()->parent()->announce(ep.manage(&fb_root));

	sleep_forever();
	return 0;
}
