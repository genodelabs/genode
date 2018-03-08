/*
 * \brief  Section widget
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SECTION_H_
#define _SECTION_H_

#include "widgets.h"


template <typename PT>
class Section : public Scout::Parent_element
{
	private:

		/*
		 * Noncopyable
		 */
		Section(Section const &);
		Section &operator = (Section const &);

		enum { _SH  = 8 };   /* shadow height */
		enum { _STH = 20 };  /* shadow height */

		Scout::Horizontal_shadow<PT, 40>  _bg;
		Scout::Horizontal_shadow<PT, 160> _shadow;

		char  const *_txt;
		Scout::Font *_font;
		int          _txt_w   = _font->string_width(_txt, Scout::strlen(_txt)).decimal();
		int          _txt_h   = _font->bounding_box().h();
		int          _txt_len = Scout::strlen(_txt);
		int          _r_add;

	public:

		Section(const char *txt, Scout::Font *font)
		:
			_bg(_STH), _shadow(_SH), _txt(txt), _font(font), _r_add(100)
		{
			append(&_bg);
			append(&_shadow);
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w)
		{
			using namespace Scout;

			_min_size = Area(w, _format_children(0, w) + _SH/2);

			_bg.geometry(Rect(_bg.position(),
			                  Area(_bg.size().w() + _r_add, _bg.size().h())));

			_shadow.geometry(Rect(_shadow.position(),
			                      Area(_shadow.size().w() + _r_add,
			                           _shadow.size().h())));
		}

		void draw(Scout::Canvas_base &canvas, Scout::Point abs_position)
		{
			using namespace Scout;

			canvas.draw_box(abs_position.x() + _position.x(),
			                abs_position.y() + _position.y() + 1,
			                _size.w() + _r_add, _txt_h - 1, Color(240,240,240,130));

			int _txt_x = abs_position.x() + _position.x() + max((_size.w() - _txt_w)/2, 8UL);
			int _txt_y = abs_position.y() + _position.y() + max((_STH - _SH - _txt_h)/2, 0) - 1;

			Parent_element::draw(canvas, abs_position);

			canvas.draw_string(_txt_x , _txt_y, _font, Color(0,0,0,150), _txt, strlen(_txt));
			canvas.draw_box(abs_position.x() + _position.x(), abs_position.y() + _position.y(),
			                _size.w() + _r_add, 1, Color(0,0,0,64));
		}
};

#endif
