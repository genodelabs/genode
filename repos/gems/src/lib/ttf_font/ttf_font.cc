/*
 * \brief  TrueType implementation of 'Text_painter::Font' interface
 * \author Norman Feske
 * \date   2018-03-20
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/bezier.h>

/* gems includes */
#include <gems/ttf_font.h>

/* libc includes */
#include <math.h>


/*
 * STB TrueType library
 */

static void *local_malloc(Genode::size_t, void *);
static void  local_free(void *, void *);
#define STBTT_malloc local_malloc
#define STBTT_free   local_free

#define STBTT_assert(cond) do { if (!(cond)) { \
	Genode::error("assertion " #cond " failed at stb_truetype.h:", __LINE__); \
	for (;;); } } while (0)

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static void *STBTT_malloc(size_t size, void *userdata)
{
	return ((Genode::Allocator *)userdata)->alloc(size);
}

static void STBTT_free(void *ptr, void *userdata)
{
	if (ptr)
		((Genode::Allocator *)userdata)->free(ptr, 0);
}


/*
 * Horizontal and vertical padding around the glyphs
 */
enum { PAD_X = 1, PAD_Y = 1 };


/**
 * Buffer for storing the opacity value of a single glyph
 *
 * Allocated once at 'Ttf_font' construction time, reused for every glyph.
 */
struct Ttf_font::Glyph_buffer
{
	private:

		/*
		 * Noncopyable
		 */
		Glyph_buffer(Glyph_buffer const &);
		Glyph_buffer &operator = (Glyph_buffer const &);

		/**
		 * Lookup table applied to opacity values to achieve a more even
		 * intensity of glyphs at different subpixel positions.
		 */
		struct Lut
		{
			unsigned char value[256];

			Lut()
			{
				auto fill_segment = [&] (long x1, long y1, long x2, long) {
					for (long i = x1>>8; i < x2>>8; i++)
						value[i] = Genode::min(255, y1>>8); };

				bezier(0, 0, 0, 130<<8, 256<<8, 260<<8, fill_segment, 7);
				value[0] = 0;
			}
		} _lut { };

	public:

		Allocator &alloc;

		typedef Glyph_painter::Glyph::Opacity Opacity;

		/**
		 * Maximum number of opacity values that fit in the buffer
		 */
		size_t const capacity;

		size_t const _headroom = 5;

		size_t _num_bytes() const { return (capacity + _headroom)*sizeof(Opacity); }

		Opacity * const _values = (Opacity *)alloc.alloc(_num_bytes());

		Glyph_buffer(Allocator &alloc, Area bounding_box)
		:
			alloc(alloc),

			/* glyphs are horizontally stretched by factor 4 */
			capacity(4*(bounding_box.w() + PAD_X)*(bounding_box.h() + PAD_Y))
		{ }

		~Glyph_buffer() { alloc.free(_values, _num_bytes()); }

		Glyph render_shifted(Codepoint, stbtt_fontinfo const &, float scale,
		                     unsigned baseline, float shift_y, bool apply_lut);
};


/**
 * Compute quality value for the vertical sharpness of the glyph
 */
static unsigned vertical_sharpness(Glyph_painter::Glyph const &glyph)
{
	unsigned const w = glyph.width;
	unsigned sum = 0;

	unsigned char const * const values = (unsigned char const *)glyph.values;

	for (unsigned j = 0; j < glyph.height - 1; j++) {
		for (unsigned i = 0; i < w - 1; i++) {
			int const dy = (int)values[w*(j + 1) + i] - (int)values[w*j + i];
			sum += dy*dy;
		}
	}
	return sum;
}


Ttf_font::Glyph
Ttf_font::Glyph_buffer::render_shifted(Codepoint      const c,
                                       stbtt_fontinfo const &font,
                                       float          const scale,
                                       unsigned       const baseline,
                                       float          const shift_y,
                                       bool           const apply_lut)
{
	float const shift_x = 0;

	int const filter_x = 4, filter_y = 1;

	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	stbtt_GetCodepointBitmapBoxSubpixel(&font, c.value,
	                                    scale, scale,
	                                    shift_x, shift_y,
	                                    &x0, &y0, &x1, &y1);

	/* clamp glyph dimensions to the area of the glyph image */
	if (y0 < -(int)baseline)
		y0 = -(int)baseline;

	/* x0 may be negative, clamp its lower bound to headroom of the buffer */
	x0 = Genode::max(-(int)_headroom, x0);

	unsigned const dx = x1 - x0;
	unsigned const dy = y1 - y0;

	unsigned const width  = dx + 1 + PAD_X;
	unsigned const height = dy + 1 + PAD_Y;

	unsigned        const dst_width = filter_x*width;
	unsigned char * const dst_ptr   = (unsigned char *)_values + _headroom;

	::memset(dst_ptr, 0, dst_width*height);

	float sub_x = 0, sub_y = 0;
	stbtt_MakeCodepointBitmapSubpixelPrefilter(&font, dst_ptr + x0,
	                                           dst_width, dy + 1, dst_width,
	                                           scale*4, scale,
	                                           shift_x, shift_y,
	                                           filter_x, filter_y,
	                                           &sub_x, &sub_y,
	                                           c.value);

	int advance = 0, lsb = 0;
	stbtt_GetCodepointHMetrics(&font, c.value, &advance, &lsb);

	/* apply non-linear transfer function */
	if (apply_lut)
		for (unsigned i = 0; i < dst_width*height; i++)
			_values[i].value = _lut.value[_values[i].value];

	return Glyph { .width   = width,
	               .height  = height,
	               .vpos    = (unsigned)((int)baseline + y0),
	               .advance = scale*advance,
	               .values  = _values + _headroom };
}


struct Ttf_font::Stbtt_font_info : stbtt_fontinfo
{
	Stbtt_font_info() : stbtt_fontinfo() { }
};


static Text_painter::Area obtain_bounding_box(stbtt_fontinfo const &font, float scale)
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	stbtt_GetFontBoundingBox(&font, &x0, &y0, &x1, &y1);

	int const w = x1 - x0 + 1,
	          h = y1 - y0 + 1;

	if (w < 1 || h < 1)
		throw Ttf_font::Unsupported_data();

	return Text_painter::Area(w*scale + 2*PAD_X, h*scale + 2*PAD_Y);
}


static unsigned obtain_baseline(stbtt_fontinfo const &font, float scale)
{
	int ascent = 0;
	stbtt_GetFontVMetrics(&font, &ascent, 0, 0);

	return (unsigned)(ascent*scale);
}


Ttf_font::Stbtt_font_info &
Ttf_font::_create_stbtt_font_info(Allocator &alloc, void const *ttf)
{
	if (alloc.need_size_for_free())
		throw Invalid_allocator();

	Stbtt_font_info &stbtt_font_info = *new (alloc) Stbtt_font_info();
	stbtt_font_info.userdata = &alloc;
	stbtt_InitFont(&stbtt_font_info, (unsigned char *)ttf, 0);

	return stbtt_font_info;
}


Ttf_font::Ttf_font(Allocator &alloc, void const *ttf, float px)
:
	_stbtt_font_info(_create_stbtt_font_info(alloc, ttf)),
	_scale(stbtt_ScaleForPixelHeight(&_stbtt_font_info, px)),
	_baseline(obtain_baseline(_stbtt_font_info, _scale)),
	_height(px + 0.5 /* round to integer */),
	_bounding_box(obtain_bounding_box(_stbtt_font_info, _scale)),
	_glyph_buffer(*new (alloc) Glyph_buffer(alloc, _bounding_box))
{ }


Ttf_font::~Ttf_font()
{
	Allocator &alloc = *(Allocator *)(_stbtt_font_info.userdata);

	destroy(alloc, &_glyph_buffer);
	destroy(alloc, &_stbtt_font_info);
}


void Ttf_font::_apply_glyph(Codepoint c, Apply_fn const &fn) const
{
	/* find vertical subpixel position that yields the sharpest glyph */
	float best_shift_y = 0;
	{
		unsigned sharpest = 0;
		for (float shift_y = -0.3; shift_y < 0.3; shift_y += 0.066) {

			Glyph const glyph =
				_glyph_buffer.render_shifted(c, _stbtt_font_info, _scale,
				                             _baseline, shift_y, false);
			unsigned const s = vertical_sharpness(glyph);
			if (s > sharpest) {
				sharpest = s;
				best_shift_y = shift_y;
			}
		}
	}
	best_shift_y = 0;

	/* re-render the best result with the LUT applied */
	Glyph const glyph =
		_glyph_buffer.render_shifted(c, _stbtt_font_info, _scale,
		                             _baseline, best_shift_y, true);
	fn.apply(glyph);
}


Text_painter::Font::Advance_info Ttf_font::advance_info(Codepoint c) const
{
	int advance = 0, lsb = 0;
	stbtt_GetCodepointHMetrics(&_stbtt_font_info, c.value, &advance, &lsb);

	return Font::Advance_info { .width   = (unsigned)(_scale*advance),
	                            .advance = _scale*advance };
}

