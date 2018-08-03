/*
 * \brief  MuPDF for Genode
 * \author Norman Feske
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <libc/component.h>
#include <framebuffer_session/client.h>
#include <base/sleep.h>
#include <util/reconstructible.h>
#include <base/attached_rom_dataspace.h>
#include <util/geometry.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <nitpicker_session/connection.h>

/* MuPDF includes */
extern "C" {
#include <fitz.h>
#include <mupdf.h>
#include <muxps.h>
#include <pdfapp.h>
}

/* libc includes */
#include <dirent.h>
#include <unistd.h>


/***************
 ** Dithering **
 ***************/

/*
 * XXX blatantly copied from 'demo/src/app/backdrop/main.cc'
 *
 * We should factor-out the dithering support into a separate header file.
 * But where is a good place to put it?
 */

enum { DITHER_SIZE = 16, DITHER_MASK = DITHER_SIZE - 1 };

static const int dither_matrix[DITHER_SIZE][DITHER_SIZE] = {
  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
};


static inline uint16_t rgb565(int r, int g, int b)
{
	enum {
		R_MASK = 0xf800, R_LSHIFT = 8,
		G_MASK = 0x07e0, G_LSHIFT = 3,
		B_MASK = 0x001f, B_RSHIFT = 3
	};
	return ((r << R_LSHIFT) & R_MASK)
	     | ((g << G_LSHIFT) & G_MASK)
	     | ((b >> B_RSHIFT) & B_MASK);
}


static void convert_line_rgba_to_rgb565(const unsigned char *rgba_src,
                                        uint16_t *dst, int num_pixels, int line)
{
	using namespace Genode;

	enum { CHANNEL_MAX = 255 };

	int const *dm = dither_matrix[line & DITHER_MASK];

	for (int i = 0; i < num_pixels; i++) {
		int v = dm[i & DITHER_MASK] >> 5;

		*dst++ = rgb565(min(v + (int)rgba_src[0], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[1], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[2], (int)CHANNEL_MAX));

		/* we ignore the alpha channel */

		rgba_src += 4; /* next pixel */
	}
}


static int pdf_select(const struct dirent *d)
{
	char const *name = d->d_name;
	int n = strlen(name);
	return (n > 4)
		? (!strncmp(&name[n-4], ".pdf", 4))
		: 0;
}


/**************
 ** PDF view **
 **************/

class Pdf_view
{
	public:

		/**
		 * Exception types
		 */
		class Non_supported_framebuffer_mode { };
		class Unexpected_document_color_depth { };

		typedef uint16_t pixel_t;
		typedef Framebuffer::Mode Mode;

	private:

		enum { NO_ALPHA = false };

		Genode::Env &_env;

		Nitpicker::Connection  _nitpicker { _env };
		Framebuffer::Session  &_framebuffer = *_nitpicker.framebuffer();
		Input::Session_client &_input       = *_nitpicker.input();

		Framebuffer::Mode _nit_mode = _nitpicker.mode();
		Framebuffer::Mode  _fb_mode {};

		Nitpicker::Area _view_area { };

		Genode::Constructible<Genode::Attached_dataspace> _fb_ds { };

		Genode::Signal_handler<Pdf_view> _nit_mode_handler {
			_env.ep(), *this, &Pdf_view::_handle_nit_mode };

		Genode::Signal_handler<Pdf_view> _sync_handler {
			_env.ep(), *this, &Pdf_view::_refresh };

		Genode::Signal_handler<Pdf_view> _input_handler {
			_env.ep(), *this, &Pdf_view::_handle_input_events };

		Nitpicker::Session::View_handle _view = _nitpicker.create_view();

		pdfapp_t _pdfapp { };

		int _motion_x = 0;
		int _motion_y = 0;

		pixel_t *_fb_base() { return _fb_ds->local_addr<pixel_t>(); }

		/**
		 * Replace the backing framebuffer
		 *
		 * The Nitpicker view is reduced if rebuffering
		 * is too expensive.
		 */
		void _rebuffer(int w, int h)
		{
			while (!_fb_ds.constructed()
			    || w > _fb_mode.width()
			    || h > _fb_mode.height())
			{
				try {
					Mode new_mode(w, h, _nit_mode.format());
					_nitpicker.buffer(new_mode, NO_ALPHA);
					_fb_mode = new_mode;
					if (_fb_ds.constructed())
						_fb_ds.destruct();
					_fb_ds.construct(_env.rm(), _framebuffer.dataspace());
					break;
				} catch (Genode::Out_of_ram) { }
				w -= w >> 2;
				h -= h >> 2;
			}

			_view_area = Nitpicker::Area(w, h);
		}

		void _resize(int w, int h)
		{
			/*
			 * XXX replace heuristics with a meaningful computation
			 *
			 * The magic values are hand-tweaked manually to accommodating the
			 * use case of showing slides.
			 */
			_pdfapp.resolution = Genode::min(_nit_mode.width()/5,
			                                 _nit_mode.height()/3.8);

			_rebuffer(w, h);

			_pdfapp.scrw = _view_area.w();
			_pdfapp.scrh = _view_area.h();
			pdfapp_onresize(&_pdfapp, _view_area.w(), _view_area.h());

			using namespace Nitpicker;
			typedef Nitpicker::Session::Command Command;
			_nitpicker.enqueue<Command::Geometry>(
				_view, Rect(Point(), _view_area));
			_nitpicker.enqueue<Command::To_front>(_view, Nitpicker::Session::View_handle());
			_nitpicker.execute();
		}

		void _handle_nit_mode()
		{
			_nit_mode = _nitpicker.mode();
			_resize(_nit_mode.width(), _nit_mode.height());
		}

		void _handle_input_event(Input::Event const &ev)
		{
			using namespace Input;

			ev.handle_relative_motion([&] (int x, int y) {
				_motion_x += x;
				_motion_y += y;
				//pdfapp_onmouse(&_pdfapp, _motion_x, _motion_y, 0, 0, 0);
			});

			ev.handle_absolute_motion([&] (int x, int y) {
				_motion_x = x;
				_motion_y = y;
				//pdfapp_onmouse(&_pdfapp, _motion_x, _motion_y, 0, 0, 0);
			});

			if (ev.key_press(BTN_LEFT))
				pdfapp_onmouse(&_pdfapp, _motion_x, _motion_y, 1, 0, -1);
			else
			if (ev.key_release(BTN_LEFT))
				pdfapp_onmouse(&_pdfapp, _motion_x, _motion_y, 1, 0, 1);
			else
			if (ev.key_press(KEY_PAGEDOWN) || ev.key_press(KEY_RIGHT))
				pdfapp_onkey(&_pdfapp, '.');
			else
			if (ev.key_press(KEY_PAGEUP) || ev.key_press(KEY_LEFT))
				pdfapp_onkey(&_pdfapp, ',');
			else
			if (ev.key_press(KEY_DOWN))
				pdfapp_onkey(&_pdfapp, 'j');
			else
			if (ev.key_press(KEY_UP))
				pdfapp_onkey(&_pdfapp, 'k');

			ev.handle_press([&] (Keycode, Codepoint glyph) {
				if ((glyph.value & 0x7f) && !(glyph.value & 0x80)) {
					pdfapp_onkey(&_pdfapp, glyph.value);
				}
			});

			/*
			ev.handle_wheel([&] (int, int y) {
				pdfapp_onmouse(
					&_pdfapp, _motion_x, _motion_y,
					y > 0 ? 4 : 5, 0, 1);
			});
			 */
		}

		void _handle_input_events()
		{
			Libc::with_libc([&] () {
				_input.for_each_event([&] (Input::Event const &ev) {
					_handle_input_event(ev); }); });
		}

		void _refresh()
		{
			_framebuffer.refresh(0, 0, _view_area.w(), _view_area.h());

			/* handle one sync signal only */
			_framebuffer.sync_sigh(Genode::Signal_context_capability());
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Non_supported_framebuffer_mode
		 * \throw Unexpected_document_color_depth
		 */
		Pdf_view(Genode::Env &env) : _env(env)
		{
			_nitpicker.mode_sigh(_nit_mode_handler);
			_input.sigh(_input_handler);

			pdfapp_init(&_pdfapp);
			_pdfapp.userdata = this;
			_pdfapp.pageno   = 0;

			{
				struct dirent **list = NULL;
				if (scandir("/", &list, pdf_select, alphasort) > 0) {

					char const *file_name = list[0]->d_name;
					int fd = open(file_name, O_BINARY | O_RDONLY, 0666);
					if (fd < 0) {
						Genode::error("Could not open input file \"", file_name, "\", Exiting.");
						exit(fd);
					}
					pdfapp_open(&_pdfapp, (char *)file_name, fd, 0);

				} else {
					Genode::error("failed to find a PDF to open");
					exit(~0);
				}
			}

			if (_pdfapp.image->n != 4) {
				Genode::error("Unexpected color depth, expected 4, got ",
				              _pdfapp.image->n, ", Exiting.");
				throw Unexpected_document_color_depth();
			}

			Genode::log(Genode::Cstring(pdfapp_version(&_pdfapp)));
		}

		void title(char const *msg)
		{
			typedef Nitpicker::Session::Command Command;
			_nitpicker.enqueue<Command::Title>(_view, msg);
			_nitpicker.execute();
		}

		void show();

		void exit(int code = 0) { _env.parent().exit(code); }
};


void Pdf_view::show()
{
	if (!_fb_ds.constructed())
		_resize(_pdfapp.image->w, _pdfapp.image->h);

	Genode::Area<> const fb_size(_fb_mode.width(), _fb_mode.height());
	int const x_max = Genode::min((int)fb_size.w(), _pdfapp.image->w);
	int const y_max = Genode::min((int)fb_size.h(), _pdfapp.image->h);

	/* clear framebuffer */
	memset(_fb_base(), 0, _fb_ds->size());

	Genode::size_t src_line_bytes   = _pdfapp.image->n * _pdfapp.image->w;
	unsigned char *src_line         = _pdfapp.image->samples;

	Genode::size_t dst_line_width   = fb_size.w(); /* in pixels */
	pixel_t *dst_line = _fb_base();

	/* skip first two lines as they contain white (XXX) */
	src_line += 2*src_line_bytes;
	dst_line += 2*dst_line_width;
	int const tweaked_y_max = y_max - 2;

	/* center vertically if the dst buffer is higher than the image */
	if (_pdfapp.image->h < (int)_view_area.h()) {
		dst_line += dst_line_width*((_view_area.h() - _pdfapp.image->h)/2);
	} else {
		auto n = src_line_bytes * Genode::min(_pdfapp.image->h - (int)_view_area.h(), -_pdfapp.pany);
		src_line += n;
	}

	/* center horizontally if the dst buffer is wider than the image */
	if (_pdfapp.image->w < (int)_view_area.w())
		dst_line += (_view_area.w() - _pdfapp.image->w)/2;

	for (int y = 0; y < tweaked_y_max; y++) {
		convert_line_rgba_to_rgb565(src_line, dst_line, x_max, y);
		src_line += src_line_bytes;
		dst_line += dst_line_width;
	}

	/* refresh after the next sync signal */
	_framebuffer.sync_sigh(_sync_handler);
}


extern "C" void _sigprocmask()
{
	/* suppress debug message by default "not-implemented" implementation */
}


/**************************
 ** Called from pdfapp.c **
 **************************/

void winrepaint(pdfapp_t *pdfapp)
{
	Pdf_view *pdf_view = (Pdf_view *)pdfapp->userdata;
	pdf_view->show();
}


void winrepaintsearch(pdfapp_t *pdfapp)
{
	Pdf_view *pdf_view = (Pdf_view *)pdfapp->userdata;
	pdf_view->show();
}


void wincursor(pdfapp_t *, int)
{
}


void windocopy(pdfapp_t*) { }


void winerror(pdfapp_t *pdfapp, fz_error error)
{
	Genode::error("winerror: error=", error);
	Pdf_view *pdf_view = (Pdf_view *)pdfapp->userdata;
	pdf_view->exit();
}


void winwarn(pdfapp_t *, char *msg)
{
	Genode::warning("MuPDF: ", Genode::Cstring(msg));
}


void winhelp(pdfapp_t *pdfapp)
{
	Genode::log(Genode::Cstring(pdfapp_usage(pdfapp)));
}


char *winpassword(pdfapp_t *, char *)
{
	Genode::warning(__func__, " not implemented");
	return NULL;
}


void winopenuri(pdfapp_t*, char *s)
{
	Genode::log(Genode::Cstring(s));
}


void winclose(pdfapp_t *pdfapp)
{
	Pdf_view *pdf_view = (Pdf_view *)pdfapp->userdata;
	pdf_view->exit();
}


void winreloadfile(pdfapp_t *)
{
	Genode::warning(__func__, " not implemented");
}


void wintitle(pdfapp_t *pdfapp, char *s)
{
	Pdf_view *pdf_view = (Pdf_view *)pdfapp->userdata;
	pdf_view->title(s);
}


void winresize(pdfapp_t*, int, int)
{
}


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] () { static Pdf_view main(env); });
}
