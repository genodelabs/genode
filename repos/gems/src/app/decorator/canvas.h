/*
 * \brief  Graphics back end for example window decorator
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CANVAS_H_
#define _CANVAS_H_

#include <decorator/types.h>

namespace Decorator {
	typedef Text_painter::Font Font;
	Font &default_font();
	template <typename PT> class Canvas;
	class Clip_guard;
}


#define FONT_START_SYMBOL _binary_droidsansb10_tff_start
extern char FONT_START_SYMBOL;


/**
 * Return default font
 */
Decorator::Font &Decorator::default_font()
{
	static Font font(&FONT_START_SYMBOL);
	return font;
}


/**
 * Abstract interface of graphics back end
 */
struct Decorator::Canvas_base
{
	virtual Rect clip() const = 0;
	virtual void clip(Rect) = 0;
	virtual void draw_box(Rect, Color) = 0;
	virtual void draw_text(Point, Font const &, Color, char const *) = 0;
};


template <typename PT>
class Decorator::Canvas : public Decorator::Canvas_base
{
	private:

		Genode::Surface<PT> _surface;

	public:

		Canvas(PT *base, Area size) : _surface(base, size) { }

		Rect clip() const override { return _surface.clip(); }

		void clip(Rect rect) override { _surface.clip(rect); }

		void draw_box(Rect rect, Color color) override
		{
			Box_painter::paint(_surface, rect, color);
		}

		void draw_text(Point pos, Font const &font,
		               Color color, char const *string) override
		{
			Text_painter::paint(_surface, pos, font, color, string);
		}
};


class Decorator::Clip_guard : Rect
{
	private:

		Canvas_base &_canvas;
		Rect const   _orig_rect;

	public:

		Clip_guard(Canvas_base &canvas, Rect clip_rect)
		:
			_canvas(canvas), _orig_rect(canvas.clip())
		{
			_canvas.clip(Rect::intersect(_orig_rect, clip_rect));
		}

		~Clip_guard() { _canvas.clip(_orig_rect); }
};

#endif /* _CANVAS_H_ */

