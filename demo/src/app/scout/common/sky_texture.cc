/*
 * \brief   Sky texture element for the use as background
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * At initialization time, we generate four 4-bit maps based on
 * bicubic interpolation of some noise at different frequencies.
 * At runtime, we overlay (add their values) the generated map
 * and use the result as index of a color table.
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "config.h"
#include "miscmath.h"
#include "sky_texture.h"


/***********************
 ** Texture generator **
 ***********************/

/**
 * Calculate fractional part of texture position for a given coordinate
 */
static inline int calc_u(int x, int w, int texture_w)
{
	return ((texture_w*x<<8)/w) & 0xff;
}


/**
 * Kubic interpolation
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
		dst[i*dst_w + j] = random()%256 - 128;

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
 * Multiply buffer values with 24:8 fixpoint value
 */
static void multiply_buf(short dst[], int len, int factor)
{
	for (int i = 0; i < len; i++)
		dst[i] = (dst[i]*factor)>>8;
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
static void brew_texture(short tmp[], short tmp2[], short dst[], int w, int h,
                         int lf_start, int lf_end, int lf_incr, int lf_mul,
                         int hf_val, int hf_mul)
{
	for (int i = lf_start; i < lf_end; i += lf_incr) {
		gen_buf(tmp, i, i, tmp2, w, h);
		multiply_buf(tmp2, w*h, (lf_mul - i)*32);
		add_bufs(tmp2, dst, dst, w*h);
	}
	if (hf_val) {
		gen_buf(tmp, hf_val, hf_val, tmp2, w, h);
		multiply_buf(tmp2, w*h, hf_mul*32);
		add_bufs(tmp2, dst, dst, w*h);
	}

	/* normalize texture to use four bits */
	normalize_buf(dst, w*h, 15);
}


/***************************
 ** Color table generator **
 ***************************/

static inline int mix_channel(int value1, int value2, int alpha)
{
	return (value1*(255 - alpha) + value2*alpha)>>8;
}


/**
 * Create 3D color table
 */
template <typename PT>
static void create_coltab(PT *dst, Color c0, Color c1, Color c2, Color bg)
{
	for (int i = 0; i < 16; i++)
		for (int j = 0; j < 16; j++)
			for (int k = 0; k < 16; k++) {

				int r = bg.r;
				int g = bg.g;
				int b = bg.b;

				r = mix_channel(r, c2.r, k*16);
				g = mix_channel(g, c2.g, k*16);
				b = mix_channel(b, c2.b, k*16);

				r = mix_channel(r, c1.r, j*16);
				g = mix_channel(g, c1.g, j*16);
				b = mix_channel(b, c1.b, j*16);

				r = mix_channel(r, c0.r, i*8);
				g = mix_channel(g, c0.g, i*8);
				b = mix_channel(b, c0.b, i*8);

				int v = (((i ^ j ^ k)<<1) & 0xff) + 128 + 64;

				r = (r + v)>>1;
				g = (g + v)>>1;
				b = (b + v)>>1;

//				r = g = b = min(255, 50 + ((i*j*128 + j*k*128 + k*i*128)>>8));

				v = 180;
				r = (v*r + (255 - v)*255)>>8;
				g = (v*g + (255 - v)*255)>>8;
				b = (v*b + (255 - v)*255)>>8;

				dst[(k<<8) + (j<<4) + i].rgba(r, g, b);
			}
}


template <typename PT>
static void compose(PT    *dst,  int dst_w, int dst_h, int x_start, int x_end,
                    short src1[], int src1_y,
                    short src2[], int src2_y,
                    short src3[], int src3_y, int src_w, int src_h,
                    PT    coltab[])
{
	for (int k = 0; k <= x_end; k += src_w) {

		int x_offset = max(0, x_start - k);
		int x_max    = min(x_end - k, src_w - 1);

		for (int j = 0; j < dst_h; j++) {

			short *s1 = src1 + x_offset + ((src1_y + j)%src_h)*src_w;
			short *s2 = src2 + x_offset + ((src2_y + j)%src_h)*src_w;
			short *s3 = src3 + x_offset + ((src3_y + j)%src_h)*src_w;
			PT    *d  = dst  + x_offset + j*dst_w  + k;

			for (int i = x_offset; i <= x_max; i++)
				*d++ = coltab[*s1++ + *s2++ + *s3++];
		}
	}
}


template <typename PT>
static void copy(PT *dst, int dst_w, int dst_h, int x_start, int x_end,
                 PT *src, int src_y, int src_w, int src_h)
{
	for (int k = 0; k <= x_end; k += src_w) {

		int x_offset = max(0, x_start - k);
		int x_max    = min(x_end - k, src_w - 1);

		for (int j = 0; j < dst_h; j++) {

			PT    *s  = src  + x_offset + ((src_y + j)%src_h)*src_w;
			PT    *d  = dst  + x_offset + j*dst_w  + k;

			if (x_max - x_offset >= 0)
				memcpy(d, s, (x_max - x_offset + 1)*sizeof(PT));
		}
	}
}


/*****************
 ** Constructor **
 *****************/

template <typename PT, int TW, int TH>
Sky_texture<PT, TW, TH>::Sky_texture()
{
	/* create nice-looking textures */
	brew_texture(_tmp[0], _buf[0], _bufs[0][0], TW, TH, 3,  7,  1, 30, 30, 10);
	brew_texture(_tmp[0], _buf[0], _bufs[1][0], TW, TH, 3, 16,  3, 50, 40, 30);
	brew_texture(_tmp[0], _buf[0], _bufs[2][0], TW, TH, 5, 40, 11, 70,  0,  0);

	/* shift texture 1 to bits 4 to 7 */
	multiply_buf(_bufs[1][0], TW*TH, 16*256);

	/* shift texture 2 to bits 8 to 11 */
	multiply_buf(_bufs[2][0], TW*TH, 16*16*256);

	/* create color table */
	create_coltab(_coltab, Color(255, 255, 255),
	                       Color(  0,   0,   0),
	                       Color(255, 255, 255),
	                       Color( 80,  88, 112));

	/* create fallback texture */
	compose(_fallback[0], TW, TH, 0, TW - 1,
	        _bufs[0][0], 0, _bufs[1][0], 0, _bufs[2][0], 0,
	        TW, TH, _coltab);
}


/*****************************************
 ** Implementation of Element interface **
 *****************************************/

template <typename PT, int TW, int TH>
void Sky_texture<PT, TW, TH>::draw(Canvas *c, int px, int py)
{
	PT *addr = static_cast<PT *>(c->addr());

	if (!addr) return;

	int cx1 = c->clip_x1();
	int cy1 = c->clip_y1();
	int cx2 = c->clip_x2();
	int cy2 = c->clip_y2();

	int v  = -py;
	int y0 = cy1 + v;
	int y1 = cy1 + (( (5*v)/16)%TH);
	int y2 = cy1 + (((11*v)/16)%TH);

	addr  += cy1*c->w();

	if (Config::background_detail == 0) {
		copy(addr, c->w(), cy2 - cy1 + 1, cx1, cx2,
		     _fallback[0], cy1 - py, TW, TH);
		return;
	}

	compose(addr, c->w(), cy2 - cy1 + 1, cx1, cx2,
	        _bufs[0][0], y0, _bufs[1][0], y1, _bufs[2][0], y2,
	        TW, TH, _coltab);
}


#include "canvas_rgb565.h"
template class Sky_texture<Pixel_rgb565, 512, 512>;
