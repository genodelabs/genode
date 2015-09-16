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

/* Painters of the nitpicker and scout graphics backends */
#include <nitpicker_gfx/text_painter.h>
#include <nitpicker_gfx/box_painter.h>
#include <scout_gfx/icon_painter.h>

/* decorator includes */
#include <decorator/types.h>

namespace Decorator {

	typedef Text_painter::Font Font;
	Font &default_font();

	enum Texture_id {
		TEXTURE_ID_CLOSER,
		TEXTURE_ID_MINIMIZE,
		TEXTURE_ID_MAXIMIZE,
		TEXTURE_ID_WINDOWED
	};

	Genode::Texture_base const &texture_by_id(Texture_id);

	class Canvas_base;
	template <typename PT> class Canvas;
	class Clip_guard;
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
	virtual void draw_texture(Point, Texture_id) = 0;
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

		void draw_texture(Point pos, Texture_id id)
		{
			Genode::Texture<PT> const &texture =
				static_cast<Genode::Texture<PT> const &>(texture_by_id(id));

			unsigned const alpha = 255;

			Icon_painter::paint(_surface, Rect(pos, texture.size()), texture, alpha);
			                    
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

