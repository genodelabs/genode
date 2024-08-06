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
#include <gui_session/connection.h>
#include <os/pixel_rgb888.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

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

#pragma GCC diagnostic pop  /* restore -Wconversion warnings */

using pixel_t = Genode::Pixel_rgb888;


static void copy_line_rgba(const unsigned char *rgba_src,
                           pixel_t *dst, int num_pixels)
{
	for (int i = 0; i < num_pixels; i++) {
		unsigned const r = *rgba_src++;
		unsigned const g = *rgba_src++;
		unsigned const b = *rgba_src++;
		rgba_src++; /* ignore alpha */

		*dst++ = pixel_t(r, g, b);
	}
}


static int pdf_select(const struct dirent *d)
{
	char const *name = d->d_name;
	size_t n = strlen(name);
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

		using Mode = Framebuffer::Mode;

	private:

		enum { NO_ALPHA = false };

		Genode::Env &_env;

		Gui::Connection _gui { _env };

		Framebuffer::Mode _nit_mode = _gui.mode();
		Framebuffer::Mode  _fb_mode {};

		Genode::Constructible<Genode::Attached_dataspace> _fb_ds { };

		Genode::Signal_handler<Pdf_view> _nit_mode_handler {
			_env.ep(), *this, &Pdf_view::_handle_nit_mode };

		Genode::Signal_handler<Pdf_view> _sync_handler {
			_env.ep(), *this, &Pdf_view::_refresh };

		Genode::Signal_handler<Pdf_view> _input_handler {
			_env.ep(), *this, &Pdf_view::_handle_input_events };

		Gui::Session::View_handle _view = _gui.create_view();

		pixel_t *_fb_base() { return _fb_ds->local_addr<pixel_t>(); }

		void _rebuffer()
		{
			using namespace Gui;

			_nit_mode = _gui.mode();

			unsigned max_x = Genode::max(_nit_mode.area.w, _fb_mode.area.w);
			unsigned max_y = Genode::max(_nit_mode.area.h, _fb_mode.area.h);

			if (max_x > _fb_mode.area.w || max_y > _fb_mode.area.h) {
				_fb_mode = Mode { .area = { max_x, max_y } };
				_gui.buffer(_fb_mode, NO_ALPHA);
				if (_fb_ds.constructed())
					_fb_ds.destruct();
				_fb_ds.construct(_env.rm(), _gui.framebuffer.dataspace());
			}

			_pdfapp.scrw = _nit_mode.area.w;
			_pdfapp.scrh = _nit_mode.area.h;

			/*
			 * XXX replace heuristics with a meaningful computation
			 *
			 * The magic values are hand-tweaked manually to accommodating the
			 * use case of showing slides.
			 */
			_pdfapp.resolution = Genode::min(_nit_mode.area.w/5,
			                                 _nit_mode.area.h/4);

			using Command = Gui::Session::Command;
			_gui.enqueue<Command::Geometry>(_view, Gui::Rect(Gui::Point(), _nit_mode.area));
			_gui.enqueue<Command::To_front>(_view, Gui::Session::View_handle());
			_gui.execute();
		}

		void _handle_nit_mode()
		{
			_rebuffer();
			pdfapp_onresize(&_pdfapp, _nit_mode.area.w, _nit_mode.area.h);
		}

		pdfapp_t _pdfapp { };

		int _motion_x = 0;
		int _motion_y = 0;

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
				_gui.input.for_each_event([&] (Input::Event const &ev) {
					_handle_input_event(ev); }); });
		}

		void _refresh()
		{
			_gui.framebuffer.refresh(0, 0, _nit_mode.area.w, _nit_mode.area.h);

			/* handle one sync signal only */
			_gui.framebuffer.sync_sigh(Genode::Signal_context_capability());
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
			_gui.mode_sigh(_nit_mode_handler);
			_gui.input.sigh(_input_handler);

			pdfapp_init(&_pdfapp);
			_pdfapp.userdata = this;
			_pdfapp.pageno   = 0;

			_rebuffer();

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
			using Command = Gui::Session::Command;
			_gui.enqueue<Command::Title>(_view, msg);
			_gui.execute();
		}

		void show();

		void exit(int code = 0) { _env.parent().exit(code); }
};


void Pdf_view::show()
{
	auto reduce_by = [] (auto value, auto diff) {
		return (value >= diff) ? value - diff : 0; };

	Framebuffer::Area const fb_size = _fb_mode.area;
	int const x_max = Genode::min((int)fb_size.w, reduce_by(_pdfapp.image->w, 2));
	int const y_max = Genode::min((int)fb_size.h, _pdfapp.image->h);

	/* clear framebuffer */
	::memset((void *)_fb_base(), 0, _fb_ds->size());

	Genode::size_t src_line_bytes   = _pdfapp.image->n * _pdfapp.image->w;
	unsigned char *src_line         = _pdfapp.image->samples;

	Genode::size_t dst_line_width   = fb_size.w; /* in pixels */
	pixel_t *dst_line = _fb_base();

	/* skip first two lines as they contain white (XXX) */
	src_line += 2*src_line_bytes;
	dst_line += 2*dst_line_width;
	int const tweaked_y_max = y_max - 2;

	/* center vertically if the dst buffer is higher than the image */
	if ((unsigned)_pdfapp.image->h < _nit_mode.area.h)
		dst_line += dst_line_width*((_nit_mode.area.h - _pdfapp.image->h)/2);

	/* center horizontally if the dst buffer is wider than the image */
	if ((unsigned)_pdfapp.image->w < _nit_mode.area.w)
		dst_line += (_nit_mode.area.w - _pdfapp.image->w)/2;

	for (int y = 0; y < tweaked_y_max; y++) {
		copy_line_rgba(src_line, dst_line, x_max);
		src_line += src_line_bytes;
		dst_line += dst_line_width;
	}

	/* refresh after the next sync signal */
	_gui.framebuffer.sync_sigh(_sync_handler);
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
