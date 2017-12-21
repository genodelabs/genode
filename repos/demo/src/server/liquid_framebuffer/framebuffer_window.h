/*
 * \brief  Window with holding a fixed-size content element
 * \author Norman Feske
 * \date   2006-09-21
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FRAMEBUFFER_WINDOW_H_
#define _FRAMEBUFFER_WINDOW_H_

#include <scout/window.h>

#include "titlebar.h"
#include "sky_texture.h"
#include "fade_icon.h"

#define TITLEBAR_RGBA _binary_titlebar_rgba_start
#define SIZER_RGBA    _binary_sizer_rgba_start

extern unsigned char TITLEBAR_RGBA[];
extern unsigned char SIZER_RGBA[];


template <typename PT>
class Framebuffer_window : public Scout::Window
{
	private:

		/*
		 * Noncopyable
		 */
		Framebuffer_window(Framebuffer_window const &);
		Framebuffer_window &operator = (Framebuffer_window const &);

		/**
		 * Constants
		 */
		enum { _TH   = 32  };  /* height of title bar */

		/**
		 * Widgets
		 */
		Scout::Titlebar<PT>              _titlebar   { };
		Scout::Sky_texture<PT, 512, 512> _bg_texture { };
		int                              _bg_offset  { 0 };
		Scout::Fade_icon<PT, 32, 32>     _sizer      { };
		Scout::Element                  *_content;
		bool                             _config_alpha;
		bool                             _config_resize_handle;
		bool                             _config_decoration;

	public:

		/**
		 * Constructor
		 */
		Framebuffer_window(Scout::Graphics_backend &gfx_backend,
		                   Scout::Element          *content,
		                   Scout::Point             position,
		                   Scout::Area              /* size */,
		                   Scout::Area              max_size,
		                   char              const *name,
		                   bool                     config_alpha,
		                   bool                     config_resize_handle,
		                   bool                     config_decoration)
		:
			Scout::Window(gfx_backend, position,
			              Scout::Area(content->min_size().w() + 2,
			                          content->min_size().h() + 1 + _TH),
			              max_size, false),
			_content(content), _config_alpha(config_alpha),
			_config_resize_handle(config_resize_handle),
			_config_decoration(config_decoration)
		{
			/* titlebar */
			_titlebar.rgba(TITLEBAR_RGBA);
			_titlebar.text(name);
			_titlebar.event_handler(new Scout::Mover_event_handler(this));

			/* resize handle */
			_sizer.rgba(SIZER_RGBA);
			_sizer.event_handler(new Scout::Sizer_event_handler(this));
			_sizer.alpha(100);

			if (config_decoration)
				append(&_titlebar);

			append(_content);

			if (config_resize_handle)
				append(&_sizer);

			unsigned const BORDER = 1, TITLE = _TH, RESIZER = 32;
			_min_size = Scout::Area(BORDER + RESIZER + BORDER,
			                        BORDER + TITLE + RESIZER + BORDER);
		}

		/**
		 * Set the window title
		 */
		void name(const char *name)
		{
			_titlebar.text(name);
		}

		/**
		 * Set the alpha config option
		 */
		void config_alpha(bool alpha)
		{
			_config_alpha = alpha;
		}

		/**
		 * Set the resize_handle config option
		 */
		void config_resize_handle(bool resize_handle)
		{
			if (!_config_resize_handle && resize_handle)
				append(&_sizer);
			else if (_config_resize_handle && !resize_handle)
				remove(&_sizer);

			_config_resize_handle = resize_handle;
		}

		/**
		 * Set the decoration config option
		 */
		void config_decoration(bool decoration)
		{
			_config_decoration = decoration;
		}

		/**
		 * Move window to new position
		 */
		void vpos(int x, int y)
		{
			Window::vpos(x, y);
			format(_size);
		}

		/**
		 * Resize the window according to the new content size
		 */
		void content_geometry(int x, int y, int w, int h)
		{
			if (_config_decoration) {
				x -= 1;    /* border */
				y -= _TH;  /* title bar */
			}
			Window::vpos(x, y);
			format(Scout::Area(w + 2, h + 1 + _TH));
		}

		/**
		 * Window interface
		 */
		void format(Scout::Area size)
		{
			using namespace Scout;

			unsigned w = size.w();
			unsigned h = size.h();

			/* limit window size to valid values */
			w = max(w, min_size().w());
			h = max(h, min_size().h());
			w = min(w, max_size().w());
			h = min(h, max_size().h());

			_size = Scout::Area(w, h);

			int y = 0;

			if (_config_decoration) {
				_titlebar.format_fixed_width(w);
				_titlebar.geometry(Rect(Point(1, y),
				                        Area(_titlebar.min_size().w(),
				                             _titlebar.min_size().h())));
				y += _titlebar.min_size().h();
			}

			int const content_h = ((int)h > y + 1) ? (h - y - 1) : 0;
			int const content_x = _config_decoration ? 1 : 0;
			int const content_w = w - 2;

			_content->format_fixed_size(Area(content_w, content_h));
			_content->geometry(Rect(Point(content_x, y),
			                   Area(content_w, content_h)));

			_sizer.geometry(Rect(Point(_size.w() - 32, _size.h() - 32), Area(32, 32)));

			if (_config_decoration)
				Window::format(_size);
			else
				Window::format(Area(_size.w() - 2, _size.h() - 1 - _TH));

			refresh();
		}

		/**
		 * Configure background texture offset (for background animation)
		 */
		void bg_offset(int bg_offset) { _bg_offset = bg_offset; }

		/**
		 * Element interface
		 */
		void draw(Scout::Canvas_base &canvas, Scout::Point abs_position)
		{
			using namespace Scout;

			if (_config_alpha)
				_bg_texture.draw(canvas, Point(0, - _bg_offset));

			Parent_element::draw(canvas, abs_position);

			/* border */
			Color color(0, 0, 0);
			canvas.draw_box(0, 0, _size.w(), 1, color);
			if (_config_decoration)
				canvas.draw_box(0, _TH, _size.w(), 1, color);
			canvas.draw_box(0, _size.h() - 1, _size.w(), 1, color);
			canvas.draw_box(0, 1, 1, _size.h() - 2, color);
			canvas.draw_box(_size.w() - 1, 1, 1, _size.h() - 2, color);
		};
};

#endif
