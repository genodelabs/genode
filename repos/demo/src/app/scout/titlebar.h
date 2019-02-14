/*
 * \brief   Titlebar interface
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TITLEBAR_H_
#define _TITLEBAR_H_

#include "widgets.h"
#include "styles.h"

namespace Scout { template <typename PT> class Titlebar; }


template <typename PT>
class Scout::Titlebar : public Parent_element
{
	private:

		/*
		 * Noncopyable
		 */
		Titlebar(Titlebar const &);
		Titlebar &operator = (Titlebar const &);

		Icon<PT, 32, 32> _fg { };
		const char *_txt = nullptr;
		int _txt_w = 0, _txt_h = 0, _txt_len = 0;

	public:

		/**
		 * Define text displayed within titlebar
		 */
		void text(const char *txt)
		{
			_txt     = txt ? txt : "Scout";
			_txt_w   = title_font.string_width(_txt, strlen(_txt)).decimal();
			_txt_h   = title_font.bounding_box().h();
			_txt_len = strlen(_txt);
		}

		/**
		 * Constructor
		 */
		Titlebar()
		{
			_fg.alpha(255);
			_fg.findable(0);
			text(0);

			append(&_fg);
		}

		/**
		 * Define foreground of titlebar
		 */
		void rgba(unsigned char *rgba) { _fg.rgba(rgba, 0, 0); };

		/**
		 * Element interface
		 */

		void format_fixed_width(int w) override
		{
			_min_size = Area(w, 32);
			_fg.geometry(Rect(Point(0, 0), _min_size));
		}

		void draw(Canvas_base &canvas, Point abs_position) override
		{
			const int b = 180, a = 200;
			canvas.draw_box(abs_position.x() + _position.x(),
			                abs_position.y() + _position.y(),
			                _size.w(), _size.h(), Color(b, b, b, a));

			int _txt_x = abs_position.x() + _position.x() + max((_size.w() - _txt_w)/2, 8UL);
			int _txt_y = abs_position.y() + _position.y() + max((_size.h() - _txt_h)/2, 0UL) - 1;
			canvas.draw_string(_txt_x , _txt_y, &title_font, Color(0,0,0,200), _txt, strlen(_txt));
			Parent_element::draw(canvas, abs_position);
		}
};

#endif
