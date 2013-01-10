/*
 * \brief  Genode C API framebuffer functions of the Linux support library
 * \author Stefan Kalkowski
 * \date   2009-06-08
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <framebuffer_session/connection.h>

/* L4lx includes */
#include <env.h>
#include <linux.h>
#include <genode/framebuffer.h>


static Framebuffer::Connection *framebuffer() {
	static bool initialized = false;
	static Framebuffer::Connection *f = 0;

	if (!initialized) {
		try {
			static Framebuffer::Connection fb;
			f = &fb;
		} catch(...) {}
		initialized = true;
	}
	return f;
}


extern "C" {

	int genode_screen_count()
	{
		Linux::Irq_guard guard;

		return framebuffer() ? 1 : 0;
	}


	unsigned long genode_fb_size(unsigned screen)
	{
		Linux::Irq_guard guard;

		return Genode::Dataspace_client(framebuffer()->dataspace()).size();
	}


	void *genode_fb_attach(unsigned screen)
	{
		Linux::Irq_guard guard;

		return L4lx::Env::env()->rm()->attach(framebuffer()->dataspace(),
		                                      "framebuffer");
	}


	void genode_fb_info(unsigned screen, int *out_w, int *out_h)
	{
		Linux::Irq_guard guard;

		Framebuffer::Mode const mode = framebuffer()->mode();
		*out_w = mode.width();
		*out_h = mode.height();
	}


	void genode_fb_refresh(unsigned screen, int x, int y, int w, int h)
	{
		Linux::Irq_guard guard;

		framebuffer()->refresh(x,y,w,h);
	}


	void genode_fb_close(unsigned screen)
	{
		NOT_IMPLEMENTED;
	}

} // extern "C"
