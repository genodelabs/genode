/*
 * \brief   Template specializations for the RGB565 pixel format
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CANVAS_RGB565_H_
#define _CANVAS_RGB565_H_

#include "miscmath.h"
#include "canvas.h"
#include "alloc.h"

typedef Pixel_rgba<unsigned short, 0xf800, 8, 0x07e0, 3, 0x001f, -3, 0, 0> Pixel_rgb565;


template <>
inline Pixel_rgb565 Pixel_rgb565::blend(Pixel_rgb565 src, int alpha)
{
	Pixel_rgb565 res;
	res.pixel = ((((alpha >> 3) * (src.pixel & 0xf81f)) >> 5) & 0xf81f)
	          | ((( alpha       * (src.pixel & 0x07c0)) >> 8) & 0x07c0);
	return res;
}


template <>
inline Pixel_rgb565 Pixel_rgb565::mix(Pixel_rgb565 p1, Pixel_rgb565 p2, int alpha)
{
	Pixel_rgba res;

	/*
	 * We substract the alpha from 264 instead of 255 to
	 * compensate the brightness loss caused by the rounding
	 * error of the blend function when having only 5 bits
	 * per channel.
	 */
	res.pixel = blend(p1, 264 - alpha).pixel + blend(p2, alpha).pixel;
	return res;
}


template <>
inline Pixel_rgb565 Pixel_rgb565::avr(Pixel_rgb565 p1, Pixel_rgb565 p2)
{
	Pixel_rgb565 res;
	res.pixel = ((p1.pixel&0xf7df)>>1) + ((p2.pixel&0xf7df)>>1);
	return res;
}


static const int dither_size = 16;
static const int dither_mask = dither_size - 1;

static const int dither_matrix[dither_size][dither_size] = {
  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
};


typedef Chunky_canvas<Pixel_rgb565> Canvas_rgb565;


class Texture_rgb565 : public Canvas::Texture
{
	private:

		int            _w, _h;           /* size of texture */
		unsigned char *_alpha;           /* alpha channel   */
		Pixel_rgb565  *_pixel;           /* pixel data      */
		bool           _preallocated;

	public:

		/**
		 * Constructor
		 */
		Texture_rgb565(int w, int h)
		:
			_w(w), _h(h),
			_alpha((unsigned char *)scout_malloc(w*h)),
			_pixel((Pixel_rgb565 *)scout_malloc(w*h*sizeof(Pixel_rgb565))),
			_preallocated(false)
		{ }

		Texture_rgb565(Pixel_rgb565 *pixel, unsigned char *alpha, int w, int h):
			_w(w), _h(h), _alpha(alpha), _pixel(pixel), _preallocated(true) { }

		/**
		 * Destructor
		 */
		~Texture_rgb565()
		{
			if (!_preallocated) {
				scout_free(_alpha);
				scout_free(_pixel);
			}
			_w = _h = 0;
		}

		/**
		 * Accessor functions
		 */
		inline unsigned char *alpha() { return _alpha; }
		inline Pixel_rgb565  *pixel() { return _pixel; }
		inline int            w()     { return _w;     }
		inline int            h()     { return _h;     }

		/**
		 * Convert rgba data line to texture
		 */
		void rgba(unsigned char *rgba, int len, int y)
		{
			if (len > _w) len = _w;
			if (y < 0 || y >= _h) return;

			int const *dm = dither_matrix[y & dither_mask];

			Pixel_rgb565 *dst_pixel  = _pixel + y*_w;
			unsigned char *dst_alpha = _alpha + y*_w;

			for (int i = 0; i < len; i++) {

				int v = dm[i & dither_mask] >> 5;
				int r = *rgba++ + v;
				int g = *rgba++ + v;
				int b = *rgba++ + v;
				int a = *rgba++ + v;

				dst_pixel[i].rgba(min(r, 255), min(g, 255), min(b, 255));
				dst_alpha[i] = min(a, 255);
			}
		}
};


template <>
inline Canvas::Texture *Canvas_rgb565::alloc_texture(int w, int h)
{
	return new Texture_rgb565(w, h);
}


template <>
inline void Canvas_rgb565::free_texture(Canvas::Texture *texture)
{
	if (texture) delete static_cast<Texture_rgb565*>(texture);
}


template <>
inline void Canvas_rgb565::set_rgba_texture(Canvas::Texture *dst,
                                            unsigned char *rgba, int len, int y)
{
	(static_cast<Texture_rgb565*>(dst))->rgba(rgba, len, y);
}


template <>
inline void Canvas_rgb565::draw_texture(Texture *src_texture, int x1, int y1)
{
	Texture_rgb565 *src = static_cast<Texture_rgb565*>(src_texture);
	unsigned char  *src_alpha = src->alpha();
	Pixel_rgb565   *src_pixel = src->pixel();

	int x2 = x1 + src->w() - 1;
	int y2 = y1 + src->h() - 1;

	/* right clipping */
	if (x2 > _clip_x2)
		x2 = _clip_x2;

	/* bottom clipping */
	if (y2 > _clip_y2)
		y2 = _clip_y2;

	/* left clipping */
	if (x1 < _clip_x1) {
		src_alpha += _clip_x1 - x1;
		src_pixel += _clip_x1 - x1;
		x1 = _clip_x1;
	}

	/* top clipping */
	if (y1 < _clip_y1) {
		int offset = (_clip_y1 - y1)*src->w();
		src_alpha += offset;
		src_pixel += offset;
		y1 = _clip_y1;
	}

	/* check if there is anything left */
	if (x1 > x2 || y1 > y2) return;

	int w = x2 - x1 + 1;
	int h = y2 - y1 + 1;

	Pixel_rgb565 *dst_pixel = _addr + y1*_w + x1;

	for (int j = 0; j < h; j++) {

		Pixel_rgb565  *sp = src_pixel;
		unsigned char *sa = src_alpha;
		Pixel_rgb565  *d  = dst_pixel;

		/* copy texture line */
		for (int i = 0; i < w; i++, sp++, sa++, d++)
			*d = Pixel_rgb565::mix(*d, *sp, *sa);

		/* add line offsets to source texture and destination */
		src_pixel += src->w();
		src_alpha += src->w();
		dst_pixel += _w;
	}
}


#endif /* _CANVAS_RGB565_ */
