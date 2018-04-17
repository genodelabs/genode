/*
 * \brief  Functor for drawing text on a surface
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_GFX__TEXT_PAINTER_H_
#define _INCLUDE__NITPICKER_GFX__TEXT_PAINTER_H_

#include <util/interface.h>
#include <util/utf8.h>
#include <nitpicker_gfx/glyph_painter.h>


struct Text_painter
{
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Rect  Rect;
	typedef Genode::Codepoint           Codepoint;

	typedef Glyph_painter::Fixpoint_number Fixpoint_number;
	typedef Glyph_painter::Position        Position;
	typedef Glyph_painter::Glyph           Glyph;


	/***************************************
	 ** Interface for accessing font data **
	 ***************************************/

	class Font : public Genode::Interface
	{
		protected:

			struct Apply_fn : Genode::Interface
			{
				virtual void apply(Glyph const &) const = 0;
			};

			virtual void _apply_glyph(Codepoint c, Apply_fn const &) const = 0;

		public:

			template <typename FN>
			void apply_glyph(Codepoint c, FN const &fn) const
			{
				/* helper to pass lambda 'fn' to virtual '_apply_glyph' method */
				struct Wrapped_fn : Apply_fn
				{
					FN const &_fn;
					void apply(Glyph const &glyph) const override { _fn(glyph); }
					Wrapped_fn(FN const &fn) : _fn(fn) { }
				};

				_apply_glyph(c, Wrapped_fn(fn));
			}

			struct Advance_info
			{
				unsigned const width;
				Fixpoint_number const advance;
			};

			virtual Advance_info advance_info(Codepoint c) const = 0;

			/**
			 * Return distance from the top of a glyph the baseline of the font
			 */
			virtual unsigned baseline() const = 0;

			/**
			 * Return height of text in pixels when rendered with the font
			 */
			virtual unsigned height() const = 0;

			/**
			 * Return the bounding box that fits each single glyph of the font
			 */
			virtual Area bounding_box() const = 0;

			/**
			 * Compute width of UTF8 string in pixels when rendered with the font
			 */
			Fixpoint_number string_width(Genode::Utf8_ptr utf8, unsigned len = ~0U) const
			{
				Fixpoint_number result { (int)0 };

				for (; utf8.complete() && len--; utf8 = utf8.next())
					result.value += advance_info(utf8.codepoint()).advance.value;

				return result;
			}
	};


	/**
	 * Paint UTF8 string to surface
	 */
	template <typename PT>
	static inline void paint(Genode::Surface<PT> &surface,
	                         Position             position,
	                         Font const          &font,
	                         Genode::Color        color,
	                         char const          *string)
	{
		/* use sub-pixel positioning horizontally */
		Fixpoint_number       x = position.x();
		Fixpoint_number const y = position.y();

		int const clip_top    = surface.clip().y1(),
		          clip_bottom = surface.clip().y2() + 1,
		          clip_left   = surface.clip().x1(),
		          clip_right  = surface.clip().x2() + 1;

		Genode::Utf8_ptr utf8(string);

		/* skip glyphs hidden behind left clipping border */
		bool skip = true;
		while (skip && utf8.complete()) {
			auto const glyph = font.advance_info(utf8.codepoint());
			skip = x.decimal() + (int)glyph.width < clip_left;
			if (skip) {
				x.value += glyph.advance.value;
				utf8 = utf8.next();
			}
		}

		int const x_start = x.decimal();

		unsigned const dst_line_len = surface.size().w();

		PT * const dst = surface.addr();

		PT  const pixel(color.r, color.g, color.b);
		int const alpha = color.a;

		/* draw glyphs */
		for ( ; utf8.complete() && (x.decimal() <= clip_right); utf8 = utf8.next()) {

			font.apply_glyph(utf8.codepoint(), [&] (Glyph const &glyph) {

				Glyph_painter::paint(Position(x, y), glyph, dst, dst_line_len,
				                     clip_top, clip_bottom, clip_left, clip_right,
				                     pixel, alpha);

				x.value += glyph.advance.value;
			});
		}

		surface.flush_pixels(Rect(Point(x_start, y.decimal()),
		                          Area(x.decimal() - x_start + 1,
		                               font.bounding_box().h())));
	}
};

#endif /* _INCLUDE__NITPICKER_GFX__TEXT_PAINTER_H_ */
