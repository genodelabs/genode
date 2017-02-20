/*
 * \brief  Functor for drawing a sky texture into a surface
 * \author Norman Feske
 * \date   2005-10-24
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <scout_gfx/random.h>
#include <scout_gfx/sky_texture_painter.h>

/**
 * Calculate fractional part of texture position for a given coordinate
 */
static inline int calc_u(int x, int w, int texture_w)
{
	return ((texture_w*x<<8)/w) & 0xff;
}


/**
 * Cubic interpolation
 *
 * \param u   relative position between x1 and x2 (0..255)
 */
static inline int filter(int x0, int x1, int x2, int x3, int u)
{
	static int cached_u = -1;
	static int k0, k1, k2, k3;

	/*
	 * Do not recompute coefficients when called
	 * with the same subsequencing u values.
	 */
	if (u != cached_u) {

		int v   = 255 - u;
		int uuu = (u*u*u)>>16;
		int vvv = (v*v*v)>>16;
		int uu  = (u*u)>>8;
		int vv  = (v*v)>>8;

		k0 = vvv/6;
		k3 = uuu/6;
		k1 = k3*3 - uu + (4<<8)/6;
		k2 = k0*3 - vv + (4<<8)/6;

		cached_u = u;
	}

	return (x0*k0 + x1*k1 + x2*k2 + x3*k3)>>8;
}


/**
 * Determine texture position by given position in image
 */
static inline int get_idx(int x, int w, int texture_w, int offset)
{
	return (offset + texture_w + (texture_w*x)/w) % texture_w;
}


/**
 * Generate sky texture based on bicubic interpolation of some noise
 */
static void gen_buf(short tmp[], int noise_w, int noise_h,
                    short dst[], int dst_w,   int dst_h)
{
	/* generate noise */
	for (int i = 0; i < noise_h; i++) for (int j = 0; j < noise_w; j++)
		dst[i*dst_w + j] = Scout::random()%256 - 128;

	/* interpolate horizontally */
	for (int j = dst_w - 1; j >= 0; j--) {

		int x0_idx = get_idx(j, dst_w, noise_w, -1);
		int x1_idx = get_idx(j, dst_w, noise_w,  0);
		int x2_idx = get_idx(j, dst_w, noise_w,  1);
		int x3_idx = get_idx(j, dst_w, noise_w,  2);
		int u      =  calc_u(j, dst_w, noise_w);

		for (int i = 0; i < noise_h; i++) {

			int x0 = dst[i*dst_w + x0_idx];
			int x1 = dst[i*dst_w + x1_idx];
			int x2 = dst[i*dst_w + x2_idx];
			int x3 = dst[i*dst_w + x3_idx];

			tmp[i*dst_w + j] = filter(x0, x1, x2, x3, u);
		}
	}

	/* vertical interpolation */
	for (int i = dst_h - 1; i >= 0; i--) {

		int y0_idx = get_idx(i, dst_h, noise_h, -1)*dst_w;
		int y1_idx = get_idx(i, dst_h, noise_h,  0)*dst_w;
		int y2_idx = get_idx(i, dst_h, noise_h,  1)*dst_w;
		int y3_idx = get_idx(i, dst_h, noise_h,  2)*dst_w;
		int u      =  calc_u(i, dst_h, noise_h);

		for (int j = 0; j < dst_w; j++) {

			int y0 = tmp[y0_idx + j];
			int y1 = tmp[y1_idx + j];
			int y2 = tmp[y2_idx + j];
			int y3 = tmp[y3_idx + j];

			dst[i*dst_w + j] = filter(y0, y1, y2, y3, u);
		}
	}
}
/**
 * Normalize buffer values to specified maximum
 */
static void normalize_buf(short dst[], int len, int amp)
{
	int min = 0x7ffffff, max = 0;

	for (int i = 0; i < len; i++) {
		if (dst[i] < min) min = dst[i];
		if (dst[i] > max) max = dst[i];
	}

	if (max == min) return;

	for (int i = 0; i < len; i++)
		dst[i] = (amp*(dst[i] - min))/(max - min);
}


/**
 * Add each pair of values of two buffers
 */
static void add_bufs(short src1[], short src2[], short dst[], int len)
{
	for (int i = 0; i < len; i++)
		dst[i] = src1[i] + src2[i];
}

/**
 * We combine (add) multiple low-frequency textures with one high-frequency
 * texture to get nice shapes.
 */
void Sky_texture_painter::Sky_texture_base::_brew_texture
(
	short tmp[], short tmp2[], short dst[], int w, int h,
	int lf_start, int lf_end, int lf_incr, int lf_mul,
	int hf_val, int hf_mul
)
{
	for (int i = lf_start; i < lf_end; i += lf_incr) {
		gen_buf(tmp, i, i, tmp2, w, h);
		_multiply_buf(tmp2, w*h, (lf_mul - i)*32);
		add_bufs(tmp2, dst, dst, w*h);
	}
	if (hf_val) {
		gen_buf(tmp, hf_val, hf_val, tmp2, w, h);
		_multiply_buf(tmp2, w*h, hf_mul*32);
		add_bufs(tmp2, dst, dst, w*h);
	}

	/* normalize texture to use four bits */
	normalize_buf(dst, w*h, 15);
}

