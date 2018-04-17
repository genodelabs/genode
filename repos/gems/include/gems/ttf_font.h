/*
 * \brief  TrueType 'Text_painter::Font'
 * \author Norman Feske
 * \date   2018-03-20
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__TTF_FONT_T_
#define _INCLUDE__GEMS__TTF_FONT_T_

#include <base/allocator.h>
#include <nitpicker_gfx/text_painter.h>

class Ttf_font : public Text_painter::Font
{
	private:

		typedef Genode::int32_t   int32_t;
		typedef Genode::Allocator Allocator;

		typedef Text_painter::Codepoint Codepoint;
		typedef Text_painter::Area      Area;
		typedef Text_painter::Glyph     Glyph;

		struct Stbtt_font_info;

		static Stbtt_font_info &_create_stbtt_font_info(Allocator &, void const *);

		Stbtt_font_info &_stbtt_font_info;

		float    const _scale;
		unsigned const _baseline;
		unsigned const _height;
		Area     const _bounding_box;

		struct Glyph_buffer;

		Glyph_buffer &_glyph_buffer;

	public:

		struct Invalid_allocator : Genode::Exception { };
		struct Unsupported_data  : Genode::Exception { };

		/**
		 * Constructor
		 *
		 * \param alloc  allocator for dynamic allocations
		 * \param ttf    TrueType font data
		 * \param px     size in pixels
		 *
		 * \throw Invalid_allocator  'alloc' is an allocator that needs
		 *                           the block size for freeing a blocki
		 * \throw Unsupported_data   unable to parse 'ttf' data
		 */
		Ttf_font(Allocator &alloc, void const *ttf, float px);

		~Ttf_font();

		void _apply_glyph(Codepoint, Apply_fn const &) const override;

		Advance_info advance_info(Codepoint) const override;

		unsigned baseline() const override { return _baseline; }
		unsigned   height() const override { return _height; }
		Area bounding_box() const override { return _bounding_box; }
};

#endif /* _INCLUDE__GEMS__TTF_FONT_T_ */
