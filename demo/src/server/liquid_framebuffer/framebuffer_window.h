/*
 * \brief  Window with holding a fixed-size content element
 * \author Norman Feske
 * \date   2006-09-21
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FRAMEBUFFER_WINDOW_H_
#define _FRAMEBUFFER_WINDOW_H_

#include "window.h"
#include "titlebar.h"
#include "sky_texture.h"

#define TITLEBAR_RGBA _binary_titlebar_rgba_start

extern unsigned char TITLEBAR_RGBA[];


template <typename PT>
class Framebuffer_window : public Window
{
	private:

		/**
		 * Constants
		 */
		enum { _TH   = 32  };  /* height of title bar */

		/**
		 * Widgets
		 */
		Titlebar<PT>              _titlebar;
		Sky_texture<PT, 512, 512> _bg_texture;
		int                       _bg_offset;
		Element                  *_content;

	public:

		/**
		 * Constructor
		 */
		Framebuffer_window(Platform       *pf,
		                   Redraw_manager *redraw,
		                   Element        *content,
		                   const char     *name)
		:
			Window(pf, redraw, content->min_w() + 2, content->min_h() + 1 + _TH),
			_bg_offset(0), _content(content)
		{
			/* titlebar */
			_titlebar.rgba(TITLEBAR_RGBA);
			_titlebar.text(name);
			_titlebar.event_handler(new Mover_event_handler(this));

			append(&_titlebar);
			append(_content);

			_min_w = max_w();
			_min_h = max_h();
		}

		/**
		 * Window interface
		 */
		void format(int w, int h)
		{
			_w = w;
			_h = h;

			Parent_element::_format_children(1, w);

			pf()->view_geometry(pf()->vx(), pf()->vy(), _w, _h);
			redraw()->size(_w, _h);
			refresh();
		}

		/**
		 * Configure background texture offset (for background animation)
		 */
		void bg_offset(int bg_offset) { _bg_offset = bg_offset; }

		/**
		 * Element interface
		 */
		void draw(Canvas *c, int x, int y)
		{
			_bg_texture.draw(c, 0, - _bg_offset);

			::Parent_element::draw(c, x, y);

			/* border */
			Color col(0, 0, 0);
			c->draw_box(0, 0, _w, 1, col);
			c->draw_box(0, _TH, _w, 1, col);
			c->draw_box(0, _h - 1, _w, 1, col);
			c->draw_box(0, 1, 1, _h - 2, col);
			c->draw_box(_w - 1, 1, 1, _h - 2, col);
		};
};

#endif
