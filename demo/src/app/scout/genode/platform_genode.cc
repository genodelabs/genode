/*
 * \brief  Main program of Genode version of Scout
 * \author Norman Feske
 * \date   2006-08-28
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/thread.h>
#include <base/printf.h>
#include <base/lock.h>
#include <nitpicker_session/connection.h>
#include <timer_session/connection.h>
#include <nitpicker_view/client.h>
#include <input/event.h>
#include <base/semaphore.h>
#include <blit/blit.h>

#include "platform.h"
#include "config.h"

static int                        _scr_w;
static int                        _scr_h;
static Framebuffer::Mode::Format  _scr_format;
static Input::Event              *_ev_buf;
static char                      *_scr_adr;
static char                      *_buf_adr;
static int                        _mx, _my;
static int                        _flip_state;   /* visible buffer (0..first, 1..second) */

static Nitpicker::Connection     *_nitpicker;
static Timer::Session            *_timer;
static unsigned long              _timer_tick;
static int                        _init_flag;
static bool                       _view_initialized;
static int                        _vx, _vy, _vw, _vh, _vbx, _vby;  /* view geometry */


/**
 * Create view and bring it to front
 *
 * This function is executed once during the construction of the static
 * 'View_client' object in the 'view' function.
 */
static Nitpicker::View_capability create_and_top_view()
{
	Nitpicker::View_capability cap = _nitpicker->create_view();
	Nitpicker::View_client(cap).stack(Nitpicker::View_capability(), true, true);
	Nitpicker::View_client(cap).viewport(_vx - _vbx, _vy - _vby, _vw, _vh,
	                                     _vbx, _flip_state ? _vby - _scr_h : _vby, true);
	return cap;
}


/**
 * Return Nitpicker view for the application
 *
 * On the first call of this function, the view gets created as static object.
 * All subsequent calls just return the pointer to this object.
 */
static Nitpicker::View *view()
{
	static Nitpicker::View_client view(create_and_top_view());
	_view_initialized = true;
	return &view;
}


using namespace Genode;


void *operator new(size_t size)
{
	void *addr = env()->heap()->alloc(size);
	if (!addr) {
		PERR("env()->heap() has consumed %zd", env()->heap()->consumed());
		PERR("env()->ram_session()->quota = %zd", env()->ram_session()->quota());
		throw Genode::Allocator::Out_of_memory();
	}
	return addr;
}


/*****************
 ** Event queue **
 *****************/

class Eventqueue
{
	private:

		static const int queue_size = 1024;

		int           _head;
		int           _tail;
		Semaphore     _sem;
		Genode::Lock  _head_lock;  /* synchronize add */

		Event _queue[queue_size];

	public:

		/**
		 * Constructor
		 */
		Eventqueue(): _head(0), _tail(0)
		{
			memset(_queue, 0, sizeof(_queue));
		}

		void add(Event *ev)
		{
			Lock::Guard lock_guard(_head_lock);

			if ((_head + 1)%queue_size != _tail) {

				_queue[_head] = *ev;
				_head = (_head + 1)%queue_size;
				_sem.up();
			}
		}

		void get(Event *dst_ev)
		{
			_sem.down();
			*dst_ev = _queue[_tail];
			_tail = (_tail + 1)%queue_size;
		}

		int pending() { return _head != _tail; }

} _evqueue;


/******************
 ** Timer thread **
 ******************/

class Timer_thread : public Thread<4096>
{
	private:

		void _import_events()
		{
			if (_nitpicker->input()->is_pending() == false) return;

			for (int i = 0, num = _nitpicker->input()->flush(); i < num; i++)
			{
				Event ev;
				Input::Event e = _ev_buf[i];

				if (e.type() == Input::Event::RELEASE
				 || e.type() == Input::Event::PRESS) {
					_mx = e.ax();
					_my = e.ay();
					ev.assign(e.type() == Input::Event::PRESS ? Event::PRESS : Event::RELEASE,
					          e.ax(), e.ay(), e.code());
					_evqueue.add(&ev);
				}

				if (e.type() == Input::Event::MOTION) {
					_mx = e.ax();
					_my = e.ay();
					ev.assign(Event::MOTION, e.ax(), e.ay(), e.code());
					_evqueue.add(&ev);
				}
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * Start thread immediately on construction.
		 */
		Timer_thread() : Thread("timer") { start(); }

		void entry()
		{
			while (1) {
				Event ev;
				ev.assign(Event::TIMER, _mx, _my, 0);
				_evqueue.add(&ev);

				_import_events();

				_timer->msleep(10);
				_timer_tick += 10;
			}
		}
};


/************************
 ** Platform interface **
 ************************/

/**
 * Initialization
 */
Platform::Platform(unsigned vx, unsigned vy, unsigned vw, unsigned vh,
                   unsigned max_vw, unsigned max_vh)
: _max_vw(max_vw), _max_vh(max_vh)
{
	_vx = vx, _vy = vy, _vw = vw, _vh = vh, _vbx = 0, _vby = 0;

	Config::mouse_cursor = 0;
	Config::browser_attr = 7;

	/*
	 * Create temporary nitpicker session just to determine the screen size
	 *
	 * NOTE: This approach has the disadvantage creating the nitpicker session
	 * is not an atomic operation. In theory, both session requests may be
	 * propagated to different nitpicker instances.
	 */
	_nitpicker = new (env()->heap()) Nitpicker::Connection();
	Framebuffer::Mode const query_mode = _nitpicker->framebuffer()->mode();
	_scr_w      = query_mode.width();
	_scr_h      = query_mode.height();
	_scr_format = query_mode.format();
	destroy(env()->heap(), _nitpicker);

	if (_max_vw) _scr_w = min(_max_vw, _scr_w);
	if (_max_vh) _scr_h = min(_max_vh, _scr_h);

	/*
	 * Allocate a nitpicker buffer double as high as the physical screen to
	 * use the upper/lower halves for double-buffering.
	 */
	_nitpicker = new (env()->heap())
		Nitpicker::Connection(_scr_w, _scr_h*2, false, _scr_format);

	static Timer::Connection timer;
	_timer = &timer;

	Framebuffer::Mode const used_mode = _nitpicker->framebuffer()->mode();

	/*
	 * We use the upper half the allocated nitpicker buffer for '_scr_adr'
	 * and the lower half for '_buf_adr'.
	 */
	_scr_adr = env()->rm_session()->attach(_nitpicker->framebuffer()->dataspace());
	_buf_adr = (char *)_scr_adr + _scr_w*_scr_h*used_mode.bytes_per_pixel();
	_ev_buf  = env()->rm_session()->attach(_nitpicker->input()->dataspace());

	new (env()->heap()) Timer_thread();

	/* mark platform as successfully initialized */
	_init_flag = 1;
}


/**
 * Platform information
 */
int   Platform::initialized() { return _init_flag; }
void *Platform::scr_adr()     { return _scr_adr; }
void *Platform::buf_adr()     { return _buf_adr; }
int   Platform::scr_w()       { return _scr_w; }
int   Platform::scr_h()       { return _scr_h; }


/**
 * Return pixel format used by the platform
 */
Platform::pixel_format Platform::scr_pixel_format() { return RGB565; }


/**
 * Exchange foreground and back buffers
 */
void Platform::flip_buf_scr()
{
	char *tmp    = _buf_adr;
	_buf_adr     = _scr_adr;
	_scr_adr     = tmp;
	_flip_state ^= 1;

	/* enable new foreground buffer by configuring the view port */
	view_geometry(_vx, _vy, _vw, _vh, false, _vbx, _vby);
}


/**
 * Copy background buffer pixels to foreground buffer
 */
void Platform::copy_buf_to_scr(int x, int y, int w, int h)
{
	Genode::size_t bpp = Framebuffer::Mode::bytes_per_pixel(_scr_format);

	/* copy background buffer to foreground buffer */
	int len     =      w*bpp;
	int linelen = _scr_w*bpp;

	char *src = _buf_adr + (y*_scr_w + x)*bpp;
	char *dst = _scr_adr + (y*_scr_w + x)*bpp;

	blit(src, linelen, dst, linelen, len, h);
//	for (int j = 0; j < h; j++, dst += linelen, src += linelen)
//		memcpy(dst, src, len);
}


/**
 * Flush pixels of specified area
 */
void Platform::scr_update(int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0) return;

	if (_flip_state) y += _scr_h;

	/*
	 * Initialize Nitpicker view
	 *
	 * We defer the initialization of the Nitpicker view to the occurrence of
	 * the first refresh to avoid artifacts during the startup of the program.
	 * Previous version used to create the view some time before calling
	 * 'refresh' for the first time. During that time, moving the mouse over
	 * the designated view area resulted in parts of the buffer to become
	 * visible.
	 */
	view();

	/* refresh part of the buffer */
	_nitpicker->framebuffer()->refresh(x, y, w, h);
}


void Platform::top_view()
{
	if (_view_initialized)
		view()->stack(Nitpicker::View_capability(), true, true);
}


/**
 * Report view geometry changes to Nitpicker.
 */
void Platform::view_geometry(int x, int y, int w, int h, int do_redraw,
                             int buf_x, int buf_y)
{
	_vx = x; _vy = y; _vw = w; _vh = h; _vbx = buf_x, _vby = buf_y;
	if (_view_initialized)
		view()->viewport(_vx - _vbx, _vy - _vby, _vw, _vh,
		                 _vbx,
		                 _flip_state ? _vby - _scr_h : _vby, do_redraw);
}


int Platform::vx()  { return _vx; }
int Platform::vy()  { return _vy; }
int Platform::vw()  { return _vw; }
int Platform::vh()  { return _vh; }
int Platform::vbx() { return _vbx; }
int Platform::vby() { return _vby; }


/**
 * Provide timer tick information
 */
unsigned long Platform::timer_ticks()
{
	return _timer_tick;
}


/**
 * Check if an event is pending
 */
int Platform::event_pending()
{
	return _evqueue.pending();
}


/**
 * Wait for an event, Zzz...zz..
 */
void Platform::get_event(Event *out_e)
{
	_evqueue.get(out_e);
}
