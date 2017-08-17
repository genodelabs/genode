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
#include <base/printf.h>
#include <libc/component.h>

extern "C" {
#include "eglutint.h"
#include <sys/select.h>
}

#include <window.h>

static bool initialized = false;
Genode::Env *genode_env;


struct Eglut_env
{
	Libc::Env &env;
	Genode::Heap heap { env.ram(), env.rm() };

	Eglut_env(Libc::Env &env) : env(env) { }
};

Genode::Constructible<Eglut_env> eglut_env;

void _eglutNativeInitDisplay()
{
	_eglut->surface_type = EGL_WINDOW_BIT;
}

void Window::sync_handler()
{
	struct eglut_window *win =_eglut->current;

	if (_eglut->idle_cb)
		_eglut->idle_cb();


	if (win->display_cb)
		win->display_cb();

	if (initialized) {
		eglSwapBuffers(_eglut->dpy, win->surface);

		//XXX: required till vsync interrupt
		eglWaitClient();
	}
}


void Window::mode_handler()
{
	if (!framebuffer.is_constructed())
		return;

	initialized = true;
	Framebuffer::Mode mode = framebuffer->mode();

	eglut_window *win  = _eglut->current;
	if (win) {
		win->native.width  = mode.width();
		win->native.height = mode.height();

		if (win->reshape_cb)
			win->reshape_cb(win->native.width, win->native.height);
	}

	update();
}


void _eglutNativeFiniDisplay(void)
{
	PDBG("not implemented");
}


void _eglutNativeInitWindow(struct eglut_window *win, const char *title,
                            int x, int y, int w, int h)
{
	Genode_egl_window *native = new (eglut_env->heap) Window(eglut_env->env, w, h);
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
		select(0, nullptr, nullptr, nullptr, nullptr);
	}
}


/*
 * 'eglut_main' will be called instead of 'main' by component initialization
 */
extern "C" int eglut_main(int argc, char *argv[]);


void Libc::Component::construct(Libc::Env &env)
{
	eglut_env.construct(env);

	genode_env = &env;

	Libc::with_libc([] () { eglut_main(1, nullptr); });
}
