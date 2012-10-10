/*
 * \brief   Template specializations for the RGB565 pixel format
 * \date    2010-09-27
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NANO3D__CANVAS_RGB565_H_
#define _INCLUDE__NANO3D__CANVAS_RGB565_H_

#include <nano3d/misc_math.h>
#include <nano3d/canvas.h>
#include <nano3d/allocator.h>

namespace Nano3d {

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


	class Canvas_rgb565::Texture : public Area
	{
		private:

			unsigned char *_alpha;   /* alpha channel */
			Pixel_rgb565  *_pixel;   /* pixel data    */
			Allocator     *_alloc;   /* backing store */

		public:

			void *operator new (__SIZE_TYPE__ size, Allocator &alloc)
			{
				return alloc.alloc(size);
			}

			/**
			 * Constructor
			 */
			Texture(Allocator *alloc, Area size) : Area(size), _alloc(alloc)
			{
				_alpha = (unsigned char *)alloc->alloc(w()*h());
				_pixel = (Pixel_rgb565 *)alloc->alloc(w()*h()*sizeof(Pixel_rgb565));
			}

			Texture(Pixel_rgb565 *pixel, unsigned char *alpha, Area size)
			: Area(size), _alpha(alpha), _pixel(pixel), _alloc(0) { }

			/**
			 * Destructor
			 */
			~Texture()
			{
				if (!_alloc)
					return;

				_alloc->free(_alpha, w()*h());
				_alloc->free(_pixel, w()*h()*sizeof(Pixel_rgb565));
			}

			/**
			 * Accessor functions
			 */
			inline unsigned char *alpha() { return _alpha; }
			inline Pixel_rgb565  *pixel() { return _pixel; }

			/**
			 * Convert rgba data line to texture
			 */
			void rgba(unsigned char *rgba, unsigned len, unsigned y)
			{
				if (len > w()) len = w();
				if (y < 0 || y >= h()) return;

				int const *dm = dither_matrix[y & dither_mask];

				Pixel_rgb565 *dst_pixel  = _pixel + y*w();
				unsigned char *dst_alpha = _alpha + y*w();

				for (unsigned i = 0; i < len; i++) {

					int v = dm[i & dither_mask] >> 5;
					int r = *rgba++ + v;
					int g = *rgba++ + v;
					int b = *rgba++ + v;
					int a = *rgba++ + v;

					dst_pixel[i].rgba(min(r, 255), min(g, 255), min(b, 255));
					dst_alpha[i] = min(a, 255);
				}
			}

			Allocator *allocator() { return _alloc; }
	};


	template <>
	inline Canvas_rgb565::Texture *Canvas_rgb565::alloc_texture(Allocator *alloc, Area size)
	{
		return new (*alloc) Canvas_rgb565::Texture(alloc, size);
	}


	template <>
	inline void Canvas_rgb565::free_texture(Canvas_rgb565::Texture *texture)
	{
		if (!texture)
			return;

		Allocator *alloc = texture->allocator();

		/* call destructors */
		texture->Canvas_rgb565::Texture::~Texture();

		/* free memory at the allocator */
		alloc->free(texture, sizeof(*texture));
	}


	template <>
	inline void Canvas_rgb565::set_rgba_texture(Canvas_rgb565::Texture *dst,
	                                            unsigned char *rgba, int len, int y)
	{
		dst->rgba(rgba, len, y);
	}


	/**
	 * Specialization that employs dithering
	 */
	template <>
	inline void interpolate_colors(Color start, Color end, Pixel_rgb565 *dst,
	                               unsigned char *dst_alpha, int num_values,
	                               int x, int y)
	{
		/* sanity check */
		if (num_values <= 0) return;

		/* use 16.16 fixpoint values for the calculation */
		int r_ascent = ((end.r - start.r)<<16)/num_values,
		    g_ascent = ((end.g - start.g)<<16)/num_values,
		    b_ascent = ((end.b - start.b)<<16)/num_values,
		    a_ascent = ((end.a - start.a)<<16)/num_values;

		/* set start values for color components */
		int r = start.r<<16,
		    g = start.g<<16,
		    b = start.b<<16,
		    a = start.a<<16;

		int const * dither_line = dither_matrix[y & dither_mask];

		for ( ; num_values--; dst++, dst_alpha++, x++) {

			int const dither_value = dither_line[x & dither_mask] << 12;

			/* combine current color value with existing pixel via alpha blending */
			*dst = Pixel_rgb565::mix(*dst,
			                         Pixel_rgb565((r + dither_value) >> 16,
			                                      (g + dither_value) >> 16,
			                                      (b + dither_value) >> 16),
			                         (a + dither_value) >> 16);

			*dst_alpha += ((255 - *dst_alpha)*(a + dither_value)) >> (16 + 8);

			/* increment color-component values by ascent */
			r += r_ascent;
			g += g_ascent;
			b += b_ascent;
			a += a_ascent;
		}
	}


	template <>
	inline void Canvas_rgb565::draw_texture(Texture *src, Point point)
	{
		unsigned char *src_alpha = src->alpha();
		Pixel_rgb565  *src_pixel = src->pixel();

		int x1 = point.x(), x2 = x1 + src->w() - 1;
		int y1 = point.y(), y2 = y1 + src->h() - 1;

		/* right clipping */
		if (x2 > _clip.x2())
			x2 = _clip.x2();

		/* bottom clipping */
		if (y2 > _clip.y2())
			y2 = _clip.y2();

		/* left clipping */
		if (x1 < _clip.x1()) {
			src_alpha += _clip.x1() - x1;
			src_pixel += _clip.x1() - x1;
			x1 = _clip.x1();
		}

		/* top clipping */
		if (y1 < _clip.y1()) {
			int offset = (_clip.y1() - y1)*src->w();
			src_alpha += offset;
			src_pixel += offset;
			y1 = _clip.y1();
		}

		/* check if there is anything left */
		if (x1 > x2 || y1 > y2) return;

		int width  = x2 - x1 + 1;
		int height = y2 - y1 + 1;

		Pixel_rgb565 *dst_pixel = _addr + y1*w() + x1;

		for (int j = 0; j < height; j++) {

			Pixel_rgb565  *sp = src_pixel;
			unsigned char *sa = src_alpha;
			Pixel_rgb565  *d  = dst_pixel;

			/* copy texture line */
			for (int i = 0; i < width; i++, sp++, sa++, d++)
				*d = Pixel_rgb565::mix(*d, *sp, *sa);

			/* add line offsets to source texture and destination */
			src_pixel += src->w();
			src_alpha += src->w();
			dst_pixel += w();
		}
	}

	template <>
	inline void Canvas_rgb565::draw_textured_polygon(Textured_polypoint *points,
	                                                 int num_points, Texture *texture)
	{
		/* sanity check for the presence of the required edge buffers */
		if (!_l_edge || !_r_edge) return;

		Textured_polypoint clipped[2*_max_points_clipped(num_points)];
		num_points = _clip_polygon<Textured_polypoint>
		                          (points, num_points, clipped);

		int y_min = 0, y_max = 0;
		_calc_y_range(clipped, num_points, &y_min, &y_max);

		_fill_edge_buffers(clipped, num_points);

		/* calculate addresses of edge buffers holding the texture attributes */
		int *tx_l_edge = _l_edge   + h(),
		    *tx_r_edge = _r_edge   + h(),
		    *ty_l_edge = tx_l_edge + h(),
		    *ty_r_edge = tx_r_edge + h();

		/* render scanlines */
		Pixel_rgb565 *src = texture->pixel();
		Pixel_rgb565 *dst = _addr + w()*y_min;  /* begin of destination scanline */
		unsigned char *src_alpha = texture->alpha();
		unsigned char *dst_alpha = _alpha + w()*y_min;
		for (int y = y_min; y < y_max; y++, dst += w(), dst_alpha += w()) {

			/* read left and right color values from corresponding edge buffers */
			Point l_texpos(tx_l_edge[y], ty_l_edge[y]);
			Point r_texpos(tx_r_edge[y], ty_r_edge[y]);
			texturize_scanline(l_texpos, r_texpos, 
			                   dst + _l_edge[y], dst_alpha + _l_edge[y],
			                   _r_edge[y] - _l_edge[y],
			                   src, src_alpha, texture->w());
		}
	}
}

#endif /* _INCLUDE__NANO3D__CANVAS_RGB565_H_ */
