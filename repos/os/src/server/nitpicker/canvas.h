/*
 * \brief  Graphics backend used by nitpicker
 * \author Norman Feske
 * \date   2013-12-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CANVAS_H_
#define _CANVAS_H_

#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/text_painter.h>
#include <nitpicker_gfx/texture_painter.h>

/* local includes */
#include "types.h"

namespace Nitpicker {
	struct Canvas_base;
	template <typename PT> class Canvas;

	typedef Text_painter::Font Font;
}


/**
 * Pixel-type-independent interface of nitpicker's graphics backend
 */
struct Nitpicker::Canvas_base : Interface
{
	virtual Area size() const = 0;

	virtual Rect clip() const = 0;

	virtual void clip(Rect) = 0;

	virtual void flush_pixels(Rect) = 0;

	virtual void draw_box(Rect, Color) = 0;

	virtual void draw_texture(Point, Texture_base const &, Texture_painter::Mode,
	                          Color mix_color, bool allow_alpha) = 0;

	virtual void draw_text(Point, Text_painter::Font const &, Color,
	                       char const *string) = 0;
};


/**
 * Pixel-type-specific implementation of nitpicker's graphics backend
 */
template <typename PT>
class Nitpicker::Canvas : public Canvas_base, public Surface_base::Flusher
{
	private:

		Surface<PT> _surface;

	public:

		Canvas(PT *base, Area size) : _surface(base, size)
		{
			_surface.flusher(this);
		}

		/**
		 * Default implementation of Surface_base::Flusher interface
		 */
		void flush_pixels(Rect) { }

		Area size() const { return _surface.size(); }

		Rect clip() const { return _surface.clip(); }

		void clip(Rect rect) { _surface.clip(rect); }

		void draw_box(Rect rect, Color color)
		{
			Box_painter::paint(_surface, rect, color);
		}

		void draw_texture(Point pos, Texture_base const &texture_base,
		                  Texture_painter::Mode mode, Color mix_color,
		                  bool allow_alpha)
		{
			Texture<PT> const &texture = static_cast<Texture<PT> const &>(texture_base);
			Texture_painter::paint(_surface, texture, mix_color, pos, mode,
			                       allow_alpha);
		}

		void draw_text(Point pos, Font const &font,
		               Color color, char const *string)
		{
			Text_painter::paint(_surface, Text_painter::Position(pos.x(), pos.y()),
			                    font, color, string);
		}
};

#endif /* _CANVAS_H_ */
