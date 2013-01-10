/*
 * \brief  Section widget
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SECTION_H_
#define _SECTION_H_

#include "widgets.h"


template <typename PT>
class Section : public Parent_element
{
	private:

		enum { _SH  = 8 };   /* shadow height */
		enum { _STH = 20 };  /* shadow height */

		Horizontal_shadow<PT, 40>  _bg;
		Horizontal_shadow<PT, 160> _shadow;
		const char                *_txt;
		int                        _txt_w, _txt_h;
		int                        _txt_len;
		Font                      *_font;
		int                        _r_add;

	public:

		Section(const char *txt, Font *font)
		: _bg(_STH), _shadow(_SH), _txt(txt), _font(font), _r_add(100)
		{
			_txt_w   = font->str_w(_txt, strlen(_txt));
			_txt_h   = font->str_h(_txt, strlen(_txt));
			_txt_len = strlen(_txt);
			append(&_bg);
			append(&_shadow);
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w)
		{
			_min_h = _format_children(0, w) + _SH/2;
			_min_w = w;

			_bg.geometry(_bg.x(), _bg.y(), _bg.w() + _r_add, _bg.h());
			_shadow.geometry(_shadow.x(), _shadow.y(), _shadow.w() + _r_add, _shadow.h());
		}

		void draw(Canvas *c, int x, int y)
		{
			c->draw_box(x + _x, y + _y + 1, _w + _r_add, _txt_h - 1, Color(240,240,240,130));

			int _txt_x = x + _x + max((_w - _txt_w)/2, 8);
			int _txt_y = y + _y + max((_STH - _SH - _txt_h)/2, 0) - 1;

			Parent_element::draw(c, x, y);
			c->draw_string(_txt_x , _txt_y, _font, Color(0,0,0,150), _txt, strlen(_txt));
			c->draw_box(x + _x, y + _y, _w + _r_add, 1, Color(0,0,0,64));
		}
};

#endif
