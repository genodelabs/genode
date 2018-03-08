/*
 * \brief  Functor for drawing glyphs
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_GFX__GLYPH_PAINTER_H_
#define _INCLUDE__NITPICKER_GFX__GLYPH_PAINTER_H_

#include <util/noncopyable.h>
#include <base/stdint.h>
#include <os/surface.h>


struct Glyph_painter
{
	/**
	 * Subpixel positions are represented as fixpoint numbers that use 24 bits
	 * for the decimal and 8 bits for the fractional part.
	 */
	struct Fixpoint_number
	{
		int value;

		Fixpoint_number(float value) : value(value*256) { };

		Fixpoint_number(int decimal) : value(decimal << 8) { };

		int decimal() const { return value >> 8; }
	};

	typedef Genode::Point<Fixpoint_number> Position;


	struct Glyph
	{
		unsigned const width;
		unsigned const height;
		unsigned const vpos;

		Fixpoint_number const advance;

		struct Opacity { unsigned char value; };

		/**
		 * Pointer to opacity values (0 = transparent, 255 = opaque)
		 *
		 * The 'values' buffer contains the glyph horizontally scaled by four.
		 * Its size is width*4*height bytes. The first row (four values) of each
		 * line as well as the last line contains padding space, which does not
		 * need to be drawn in order to obtain the complete shape.
		 */
		Opacity const * const values;

		unsigned num_values() const { return 4*width*height; }
	};


	/**
	 * Draw single glyph on a 'dst' buffer, with clipping applied
	 *
	 * In contrast to most painter functions, which operate on a 'Surface',
	 * this function has a lower-level interface. It is intended as a utility
	 * called by painter implementations, not by applications directly.
	 */
	template <typename PT>
	static inline void paint(Position const position, Glyph const &glyph,
	                         PT *dst, unsigned const dst_line_len,
	                         int const clip_top,  int const clip_bottom,
	                         int const clip_left, int const clip_right,
	                         PT const color, int const alpha)
	{
		Fixpoint_number const x = position.x();
		int             const y = position.y().decimal();

		int const dst_y1 = y + glyph.vpos,
		          dst_y2 = dst_y1 + glyph.height;

		unsigned const clipped_from_top = clip_top > dst_y1
		                                ? clip_top - dst_y1 : 0;

		unsigned const clipped_from_bottom = dst_y2 > clip_bottom
		                                   ? dst_y2 - clip_bottom : 0;

		if (clipped_from_top + clipped_from_bottom >= glyph.height)
			return;

		unsigned const num_lines = glyph.height - clipped_from_top
		                                        - clipped_from_bottom;
		int const w     = glyph.width;
		int const start = Genode::max(0,     clip_left  - x.decimal());
		int const end   = Genode::min(w - 1, clip_right - x.decimal());

		int const dst_x   = start + ((x.value) >> 8);
		int const glyph_x = start*4 + 3 - ((x.value & 0xc0) >> 6);

		unsigned const glyph_line_len = 4*glyph.width;

		PT *dst_column   = dst + dst_x
		                 + dst_line_len*(dst_y1 + clipped_from_top);

		typedef Glyph::Opacity Opacity;
		Opacity const *glyph_column = glyph.values + glyph_x
		                            + glyph_line_len*clipped_from_top;

		/* iterate over the visible columns of the glyph */
		for (int i = start; i < end; i++) {

			/* weights of the two sampled values (horizontal neighbors)*/
			int const u0 = x.value*4 & 0xff;
			int const u1 = 0x100 - u0;

			PT            *d = dst_column;
			Opacity const *s = glyph_column;

			/* iterate over one column */
			for (unsigned j = 0; j < num_lines; j++) {

				/* sample values from glyph image */
				unsigned const v0 = s->value;
				unsigned const v1 = (s + 1)->value;

				/* apply weights */
				int const value = (v0*u0 + v1*u1) >> 8;

				/* transfer pixel */
				if (value)
					*d = (value == 255 && alpha == 255)
					   ? color : PT::mix(*d, color, (alpha*value) >> 8);

				s += glyph_line_len;
				d += dst_line_len;
			}

			dst_column   += 1;
			glyph_column += 4;
		}
	}
};

#endif /* _INCLUDE__NITPICKER_GFX__GLYPH_PAINTER_H_ */
