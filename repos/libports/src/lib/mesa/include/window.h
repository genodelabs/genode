/**
 * \brief  EGL window with framebuffer connection
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <base/debug.h>
#include <base/env.h>
#include <framebuffer_session/connection.h>
#include <util/reconstructible.h>

#include <EGL/eglplatform.h>

struct Window : Genode_egl_window
{
	Genode::Constructible<Framebuffer::Connection> framebuffer;
	Genode::Io_signal_handler<Window>              sync_dispatcher;
	Genode::Io_signal_handler<Window>              mode_dispatcher;
	Genode::Env                                   &env;

	Window(Genode::Env &env, int w, int h)
	:
		sync_dispatcher(env.ep(), *this, &Window::sync_handler),
	  mode_dispatcher(env.ep(), *this, &Window::mode_handler),
	  env(env)
	{
		width  = w;
		height = h;

		framebuffer.construct(env, Framebuffer::Mode(width, height, Framebuffer::Mode::RGB565));
		addr = env.rm().attach(framebuffer->dataspace());

		framebuffer->sync_sigh(sync_dispatcher);
		framebuffer->mode_sigh(mode_dispatcher);

		mode_handler();
	}

	void sync_handler();
	void mode_handler();

	void update()
	{
		env.rm().detach(addr);
		addr = env.rm().attach(framebuffer->dataspace());
	}
};

#endif /* _WINDOW_H_ */
