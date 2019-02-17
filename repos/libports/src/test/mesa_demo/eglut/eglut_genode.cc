/**
 * \brief  eglut bindings for Genode Mesa demos
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) Genode Labs GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <base/heap.h>
#include <base/debug.h>
#include <framebuffer_session/connection.h>
#include <libc/component.h>

extern "C" {
#include "eglutint.h"
#include <sys/select.h>
}


Genode::Env *genode_env;

static Genode::Constructible<Genode::Entrypoint> signal_ep;

Genode::Entrypoint &genode_entrypoint()
{
	return *signal_ep;
}


struct Window : Genode_egl_window
{
	Genode::Env                                   &env;
	Genode::Constructible<Framebuffer::Connection> framebuffer;
	Genode::Io_signal_handler<Window>              mode_dispatcher;
	bool                                           mode_change_pending = false;

	Window(Genode::Env &env, int w, int h)
	:
		env(env),
	  mode_dispatcher(*signal_ep, *this, &Window::mode_handler)
	{
		width  = w;
		height = h;

		framebuffer.construct(env, Framebuffer::Mode(width, height, Framebuffer::Mode::RGB565));
		addr = env.rm().attach(framebuffer->dataspace());

		framebuffer->mode_sigh(mode_dispatcher);

		mode_change();
	}

	void mode_handler()
	{
		mode_change_pending = true;
	}

	void update()
	{
		env.rm().detach(addr);
		addr = env.rm().attach(framebuffer->dataspace());
	}

	void mode_change()
	{
		Framebuffer::Mode mode = framebuffer->mode();

		eglut_window *win  = _eglut->current;
		if (win) {
			win->native.width  = mode.width();
			win->native.height = mode.height();
			width  = mode.width();
			height = mode.height();

			if (win->reshape_cb)
				win->reshape_cb(win->native.width, win->native.height);
		}
	
		update();
		mode_change_pending = false;
	}

	void refresh()
	{
		framebuffer->refresh(0, 0, width, height);
	}
};

Genode::Constructible<Window> eglut_win;


void _eglutNativeInitDisplay()
{
	_eglut->surface_type = EGL_WINDOW_BIT;
}


void _eglutNativeFiniDisplay(void)
{
	PDBG("not implemented");
}


void _eglutNativeInitWindow(struct eglut_window *win, const char *title,
                            int x, int y, int w, int h)
{
	eglut_win.construct(*genode_env, w, h);
	Genode_egl_window *native = &*eglut_win;
	win->native.u.window = native;
	win->native.width = w;
	win->native.height = h;
}


void _eglutNativeFiniWindow(struct eglut_window *win)
{
	PDBG("not implemented");
}


void _eglutNativeEventLoop()
{
	while (true) {
		struct eglut_window *win =_eglut->current;

		if (eglut_win->mode_change_pending) {
			eglut_win->mode_change();
		}

		if (_eglut->idle_cb)
			_eglut->idle_cb();


		if (win->display_cb)
			win->display_cb();

		if (eglut_win.constructed()) {
			eglWaitClient();
			eglSwapBuffers(_eglut->dpy, win->surface);
			eglut_win->refresh();
		}
	}
}


/*
 * 'eglut_main' will be called instead of 'main' by component initialization
 */
extern "C" int eglut_main(int argc, char *argv[]);


void Libc::Component::construct(Libc::Env &env)
{
	genode_env = &env;
	signal_ep.construct(env, 1024*sizeof(long), "eglut_signal_ep",
	                    Genode::Affinity::Location());
	Libc::with_libc([] () { eglut_main(1, nullptr); });
}
