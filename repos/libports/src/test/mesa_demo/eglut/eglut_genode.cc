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

#include <base/attached_dataspace.h>
#include <base/heap.h>
#include <gui_session/connection.h>
#include <libc/component.h>
#include <libc/args.h>

extern "C" {
#include "eglutint.h"
#include <sys/select.h>
}


Genode::Env *genode_env;

struct Window : Genode_egl_window
{
	using Command = Gui::Session::Command;

	Genode::Env      &env;
	Framebuffer::Mode mode;
	Gui::Connection   gui { env };
	Genode::Constructible<Genode::Attached_dataspace> ds { };
	Gui::View_id      view { };

	Genode::addr_t fb_addr { 0 };
	Genode::addr_t fb_size { 0 };
	Genode::Ram_dataspace_capability buffer_cap { };

	Window(Genode::Env &env, int w, int h)
	:
		env(env), mode { .area = Gui::Area(w, h) }
	{
		width  = w;
		height = h;
		type   = WINDOW;

		gui.buffer(mode, false);
		view = gui.create_view();

		mode_change();

		gui.enqueue<Command::Title>(view, "eglut");
		gui.enqueue<Command::Front>(view);
		gui.execute();
	}

	void mode_change()
	{
		if (ds.constructed())
			ds.destruct();

		ds.construct(env.rm(), gui.framebuffer.dataspace());

		addr = ds->local_addr<unsigned char>();

		Gui::Rect rect { Gui::Point { 0, 0 }, mode.area };
		gui.enqueue<Command::Geometry>(view, rect);
		gui.execute();
	}

	void refresh()
	{
		gui.framebuffer.refresh(0, 0, mode.area.w, mode.area.h);
	}
};


static Genode::Constructible<Window> eglut_win;


void _eglutNativeInitDisplay()
{
	_eglut->surface_type = EGL_WINDOW_BIT;
}


void _eglutNativeFiniDisplay(void)
{
	Genode::warning(__PRETTY_FUNCTION__, " not implemented");
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
	Genode::warning(__PRETTY_FUNCTION__, " not implemented");
}


void _eglutNativeEventLoop()
{
	while (true) {
		struct eglut_window *win =_eglut->current;

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


/* initial environment for the FreeBSD libc implementation */
extern char **environ;


static void construct_component(Libc::Env &env)
{
	int argc    = 0;
	char **argv = nullptr;
	char **envp = nullptr;

	populate_args_and_env(env, argc, argv, envp);

	environ = envp;

	exit(eglut_main(argc, argv));
}


void Libc::Component::construct(Libc::Env &env)
{
	genode_env = &env;
	Libc::with_libc([&] () { construct_component(env); });
}
