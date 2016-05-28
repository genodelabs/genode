/*
 * \brief  Xvfb display application for Nitpicker
 * \author Norman Feske
 * \date   2009-11-03
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* X11 includes */
#include <X11/XWDFile.h>
#include <X11/Xlib.h>

/* Genode includes */
#include <base/printf.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_view/client.h>
#include <framebuffer_session/client.h>
#include <input_session/client.h>
#include <input/event.h>
#include <timer_session/connection.h>
#include <blit/blit.h>
#include <xev_track/xev_track.h>
#include <os/config.h>

/* local includes */
#include "inject_input.h"


typedef Genode::uint16_t Pixel;


/**
 * Variables defined by the supplied config file
 */
char *config_xvfb_file_name  = 0;
char *config_x_display       = 0;
int   config_force_top       = 1;

/**
 * Variables derived from the xvfb screen
 */
static int    xvfb_width, xvfb_height;
static Pixel *xvfb_addr;

static Framebuffer::Session *fb_session;
static Framebuffer::Mode     fb_mode;
static Pixel                *fb_addr;


static Nitpicker::Session *nitpicker()
{
	struct Connection : Nitpicker::Connection
	{
		Connection()
		{
			Nitpicker::Connection::buffer(mode(), false);
		}
	};

	static Connection connection;
	return &connection;
}


/**
 * Parse configuration
 *
 * \return true if configuration is complete
 */
static bool read_config()
{
	using namespace Genode;

	Xml_node config_node = config()->xml_node();

	try {
		static char buf[256];
		config_node.sub_node("xvfb").value(buf, sizeof(buf));
		config_xvfb_file_name = buf;
	} catch (Xml_node::Nonexistent_sub_node) {
		printf("Error: Declaration of xvfb file name is missing\n");
		return false;
	}

	try {
		static char buf[256];
		config_node.sub_node("display").value(buf, sizeof(buf));
		config_x_display = buf;
	} catch (Xml_node::Nonexistent_sub_node) {
		printf("Error: Display declaration is missing\n");
		return false;
	}

	return true;
}


static inline long convert_from_big_endian(long x)
{
	char v[4] = {
		(char)((x & 0xff000000) >> 24),
		(char)((x & 0x00ff0000) >> 16),
		(char)((x & 0x0000ff00) >>  8),
		(char)((x & 0x000000ff) >>  0),
	};
	return *(long *)v;
}


/**
 * Map file (read only) to local address space
 */
static void *mmap_file(const char *file_name)
{
	int fd = open(file_name, O_RDONLY);
	if (fd < 0)
		Genode::printf("Error: Could not open file %s\n", file_name);

	struct stat stat;
	if (fstat(fd, &stat) < 0)
		Genode::printf("Error: Could not fstat file %s\n", file_name);

	void *addr = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == (void *)-1)
		Genode::printf("Error: Could not mmap file\n");

	return addr;
}


/**
 * Flush part of the Xvfb screen to the Nitpicker session
 */
static void flush(int x, int y, int width, int height, Framebuffer::Mode const &mode)
{
	/* clip arguments against framebuffer geometry */
	if (x < 0) width  += x, x = 0;
	if (y < 0) height += y, y = 0;
	if (width  > mode.width()  - x) width  = mode.width()  - x;
	if (height > mode.height() - y) height = mode.height() - y;

	if (width <= 0 || height <= 0) return;

	/* copy pixels from xvfb to the nitpicker buffer */
	blit(xvfb_addr + x + xvfb_width*y,   xvfb_width*sizeof(Pixel),
	       fb_addr + x + mode.width()*y, mode.width()*sizeof(Pixel),
	     width*sizeof(Pixel), height);

	/* refresh nitpicker views */
	fb_session->refresh(x, y, width, height);
}


class Bounding_box
{
	private:

		int _x1, _y1, _x2, _y2;


	public:

		/**
		 * Constructor
		 */
		Bounding_box() { reset(); }

		bool valid() { return _x1 <= _x2 && _y1 <= _y2; }

		void reset() { _x1 = 0, _y1 = 0, _x2 = -1, _y2 = -1; }

		void extend(int x, int y, int w, int h)
		{
			int nx2 = x + w - 1;
			int ny2 = y + h - 1;

			if (!valid()) {
				_x1 = x, _y1 = y, _x2 = x + w - 1, _y2 = y + h - 1;
			} else {
				_x1 = Genode::min(_x1, x);
				_y1 = Genode::min(_y1, y);
				_x2 = Genode::max(_x2, nx2);
				_y2 = Genode::max(_y2, ny2);
			}
		}

		int x() { return _x1; }
		int y() { return _y1; }
		int w() { return _x2 - _x1 + 1; }
		int h() { return _y2 - _y1 + 1; }
};

static Bounding_box pending_redraw;


/**************************************************
 ** Hook functions called by the X event tracker **
 **************************************************/

struct View
{
	Nitpicker::View_capability cap;
};

static View views[MAX_VIEWS];


void create_view(int view_id)
{
	views[view_id].cap = nitpicker()->create_view();
}


void destroy_view(int view_id)
{
	nitpicker()->destroy_view(views[view_id].cap);

	/* invalidate capability */
	views[view_id].cap = Nitpicker::View_capability();
}


void set_background_view(int view_id)
{
	nitpicker()->background(views[view_id].cap);
}


void place_view(int view_id, int x, int y, int w, int h)
{
	Nitpicker::View_client view(views[view_id].cap);
	view.viewport(x, y, w, h, -x, -y, false);
}


void stack_view(int view_id, int neighbor_id, bool behind)
{
	Nitpicker::View_client view(views[view_id].cap);

	/* translate invalid neighbor ID to invalid capability */
	Nitpicker::View_capability neighbor_cap;
	if (neighbor_id >= 0)
		neighbor_cap = views[neighbor_id].cap;

	view.stack(neighbor_cap, behind, true);
}


void refresh(int x, int y, int w, int h)
{
	pending_redraw.extend(x, y, w, h);
}


int main(int, char **)
{
	if (!read_config()) return -1;

	static Timer::Connection timer;

	static Framebuffer::Session_client fb(nitpicker()->framebuffer_session());
	static Input::Session_client input(nitpicker()->input_session());
	fb_session = &fb;
	fb_addr = Genode::env()->rm_session()->attach(fb.dataspace());
	fb_mode = fb.mode();

	XWDFileHeader *xwd = (XWDFileHeader *)mmap_file(config_xvfb_file_name);
	if (!xwd) return -1;

	xvfb_width  = convert_from_big_endian(xwd->pixmap_width);
	xvfb_height = convert_from_big_endian(xwd->pixmap_height);

	enum { SUPPORTED_BPP = 16 };
	if (convert_from_big_endian(xwd->bits_per_pixel) != SUPPORTED_BPP) {
		Genode::printf("Error: Color depth %d is not supported (use %d bpp)\n",
		               (int)convert_from_big_endian(xwd->pixmap_depth), SUPPORTED_BPP);
		return -2;
	}

	xvfb_addr = (Pixel *)((long)xwd + convert_from_big_endian(xwd->header_size)
	                                + convert_from_big_endian(xwd->ncolors)*sizeof(XWDColor));

	if (xvfb_width != fb_mode.width() || xvfb_height != fb_mode.height()) {
		Genode::printf("Error: Xvfb mode must equal the Nitpicker screen mode of %dx%d\n",
		               fb_mode.width(), fb_mode.height());
		return -3;
	}

	Input::Event *ev_buf = Genode::env()->rm_session()->attach(input.dataspace());

	Display *dpy = XOpenDisplay(config_x_display);
	if (!dpy) {
		Genode::printf("Error: Cannot open display\n");
		return -4;
	}

	if (!inject_input_init(dpy))
		return -5;

	if (!xev_track_init(dpy))
		return -6;

	for (;;) {
		pending_redraw.reset();

		/*
		 * Process due X events and update 'pending_redraw' as side effect
		 */
		XEvent ev;
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			xev_track_handle_event(dpy, ev);
		}

		/*
		 * Forward input events to the X session
		 */
		while (input.pending()) {

			int num_ev = input.flush();
			for (int i = 0; i < num_ev; i++)
				inject_input_event(dpy, ev_buf[i]);
		}

		/*
		 * Track mouse cursor and update 'pending_redraw' as side effect
		 */
		xev_track_handle_cursor(dpy);

		if (pending_redraw.valid())
			flush(pending_redraw.x(), pending_redraw.y(),
			      pending_redraw.w(), pending_redraw.h(), fb_mode);

		timer.msleep(5);
	}
	return 0;
}
