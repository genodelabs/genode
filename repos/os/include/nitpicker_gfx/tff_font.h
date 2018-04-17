/*
 * \brief  Implementation of 'Text_painter::Font' using a trivial font format
 * \author Norman Feske
 * \date   2018-03-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_GFX__TFF_FONT_H_
#define _INCLUDE__NITPICKER_GFX__TFF_FONT_H_

#include <base/allocator.h>
#include <nitpicker_gfx/text_painter.h>

class Tff_font : public Text_painter::Font
{
	public:

		struct Glyph_buffer
		{
			char         * const ptr;
			Genode::size_t const size;
		};

		template <unsigned SIZE>
		struct Static_glyph_buffer : Glyph_buffer
		{
			char _data[SIZE];
			Static_glyph_buffer() : Glyph_buffer({_data, sizeof(_data)}) { }
		};

		struct Allocated_glyph_buffer : Tff_font::Glyph_buffer
		{
			Genode::Allocator &_alloc;

			Allocated_glyph_buffer(void const *tff, Genode::Allocator &alloc)
			:
				Tff_font::Glyph_buffer({
					(char *)alloc.alloc(Tff_font::glyph_buffer_size(tff)),
					Tff_font::glyph_buffer_size(tff) }),
				_alloc(alloc)
			{ }

			~Allocated_glyph_buffer()
			{
				_alloc.free(ptr, size);
			}
		};

	private:

		typedef Genode::int32_t int32_t;

		typedef Text_painter::Codepoint Codepoint;
		typedef Text_painter::Area      Area;
		typedef Text_painter::Glyph     Glyph;

		Glyph_buffer &_buf;

		enum { NUM_GLYPHS = 256, PAD_LEFT = 1 };

		struct Tff
		{
			unsigned char const *img;             /* font image         */
			int           const  img_w, img_h;    /* size of font image */
			int32_t       const *otab;            /* offset table       */
			int32_t       const *wtab;            /* width table        */

			Tff(void const *data)
			:
				img((unsigned char *)data + 2056),

				img_w(*((int32_t *)((unsigned char *)data + 2048))),
				img_h(*((int32_t *)((unsigned char *)data + 2052))),

				otab((int32_t *)(data)),
				wtab((int32_t *)((unsigned char *)data + 1024))
			{ }

			Area bounding_box() const
			{
				unsigned max_w = 0;
				for (unsigned i = 0; i < NUM_GLYPHS; i++)
					max_w = Genode::max(max_w, (unsigned)wtab[i]);

				return Area(max_w, img_h);
			}

			bool _glyph_line_empty(unsigned char c, unsigned y) const
			{
				unsigned char const * const line = img + otab[c] + y*img_w;
				for (unsigned i = 0; i < (unsigned)wtab[c]; i++)
					if (line[i])
						return false;

				return true;
			}

			struct Vertical_metrics
			{
				unsigned vpos;
				unsigned height;
			};

			Vertical_metrics vertical_metrics(unsigned char c) const
			{
				unsigned y_start = 0;
				unsigned y_end   = img_h;

				/* determine empty lines below glyph */
				for (; y_end > 0; y_end--)
					if (!_glyph_line_empty(c, y_end - 1))
						break;

				/* determine empty lines above glyph */
				for (; y_start < (unsigned)img_h; y_start++)
					if (!_glyph_line_empty(c, y_start))
						break;

				return Vertical_metrics {
					.vpos   = y_start,
					.height = (y_end > y_start) ? y_end - y_start : 0
				};
			}
		};

		Tff const _tff;

		Tff::Vertical_metrics _vertical_metrics[NUM_GLYPHS];

		Area const _bounding_box = _tff.bounding_box();

		/*
		 * Noncopyable
		 */
		Tff_font(Tff_font const &);
		Tff_font &operator = (Tff_font const &);

	public:

		struct Invalid_format            : Genode::Exception { };
		struct Insufficient_glyph_buffer : Genode::Exception { };

		/**
		 * Constructor
		 *
		 * \param tff           font data
		 * \param glyph_buffer  buffer for rendered glyph
		 *
		 * \throw Invalid_format
		 * \throw Insufficient_glyph_buffer
		 *
		 * The 'glyph_buffer' should be dimensioned via 'glyph_buffer_size()'.
		 */
		Tff_font(void const *tff, Glyph_buffer &glyph_buffer)
		:
			_buf(glyph_buffer), _tff(tff)
		{
			if (_tff.img_h < 1 || _tff.img_w < 1)
				throw Invalid_format();

			if (_buf.size < glyph_buffer_size(tff))
				throw Insufficient_glyph_buffer();

			for (unsigned i = 0; i < NUM_GLYPHS; i++)
				_vertical_metrics[i] = _tff.vertical_metrics(i);
		}

		/**
		 * Return required glyph-buffer size for a given font
		 */
		static Genode::size_t glyph_buffer_size(void const *tff)
		{
			/* account for four-times horizontal supersampling */
			return Tff(tff).bounding_box().count()*4;
		}

		void _apply_glyph(Codepoint c, Apply_fn const &fn) const override
		{
			unsigned const ascii = c.value & 0xff;

			unsigned const w    = _tff.wtab[ascii],
			               h    = _vertical_metrics[ascii].height,
			               vpos = _vertical_metrics[ascii].vpos;

			unsigned char const *glyph_line = _tff.img + _tff.otab[ascii]
			                                           + vpos*_tff.img_w;

			Glyph::Opacity *dst = (Glyph::Opacity *)_buf.ptr;

			for (unsigned j = 0; j < h; j++) {

				/* insert padding in front */
				for (unsigned k = 0; k < PAD_LEFT*4; k++)
					*dst++ = Glyph::Opacity { 0 };

				/* copy line from font image to glyph */
				for (unsigned i = 0; i < w; i++) {
					Glyph::Opacity const opacity { glyph_line[i] };
					for (unsigned k = 0; k < 4; k++)
						*dst++ = opacity;
				}
				glyph_line += _tff.img_w;
			}

			Glyph const glyph { .width   = w + PAD_LEFT,
			                    .height  = h,
			                    .vpos    = vpos,
			                    .advance = (int)w,
			                    .values  = (Glyph::Opacity *)_buf.ptr };
			fn.apply(glyph);
		}

		Advance_info advance_info(Codepoint c) const override
		{
			unsigned const ascii = c.value & 0xff;
			unsigned const w     = _tff.wtab[ascii];

			return Advance_info { .width   = w + PAD_LEFT,
			                      .advance = (int)w };
		}

		unsigned baseline() const override
		{
			Tff::Vertical_metrics const m = _vertical_metrics['m'];
			return m.vpos + m.height;
		}

		unsigned height() const override
		{
			return _bounding_box.h();
		}

		Area bounding_box() const override { return _bounding_box; }
};

#endif /* _INCLUDE__NITPICKER_GFX__TFF_FONT_H_ */
