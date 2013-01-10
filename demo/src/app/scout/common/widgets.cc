/*
 * \brief   GUI elements
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "miscmath.h"
#include "widgets.h"


/*************
 ** Docview **
 *************/

void Docview::format_fixed_width(int w)
{
	_min_w = _min_h = 0;

	if (_cont) {
		_cont->format_fixed_width(w - 2*_padx - _right_pad);
		_min_w = w;
		_min_h = _voffset + _cont->min_h();
	}

	if (_bg)
		_bg->geometry(0, 0, _min_w, _min_h);
}


void Docview::draw(Canvas *c, int x, int y)
{
	if (_bg)     _bg->draw(c, _x + x, _y + y);
	if (_cont) _cont->draw(c, _x + x, _y + y);
}


Element *Docview::find(int x, int y)
{
	if (!Element::find(x, y)) return 0;
	Element *res = _cont ? _cont->find(x - _x, y - _y) : 0;
	return res ? res : this;
}


void Docview::geometry(int x, int y, int w, int h)
{
	::Element::geometry(x, y, w, h);

	if (_cont) _cont->geometry(_padx, _voffset, _cont->min_w(), h - _voffset);
}


/***********************
 ** Horizontal shadow **
 ***********************/

template <typename PT, int INTENSITY>
void Horizontal_shadow<PT, INTENSITY>::draw(Canvas *c, int x, int y)
{
	PT *addr = static_cast<PT *>(c->addr());

	if (!addr) return;

	const int cx1 = c->clip_x1();
	const int cy1 = c->clip_y1();
	const int cx2 = c->clip_x2();
	const int cy2 = c->clip_y2();

	x += _x;
	y += _y;
	int w = _w;
	int h = _h;

	int curr_a = INTENSITY;
	int step   = _h ? (curr_a/_h) : 0;

	if (x < cx1) {
		w -= cx1 - x;
		x = cx1;
	}

	if (y < cy1) {
		h      -=  cy1 - y;
		curr_a -= (cy1 - y)*step;
		y       = cy1;
	}

	if (w > cx2 - x + 1)
		w = cx2 - x + 1;

	if (h > cy2 - y + 1)
		h = cy2 - y + 1;

	addr += c->w()*y + x;

	PT shadow_color(0,0,0);

	for (int j = 0; j < h; j++, addr += c->w()) {

		PT *d = addr;

		for (int i = 0; i < w; i++, d++)
			*d = PT::mix(*d, shadow_color, curr_a);

		curr_a -= step;
	}
}


/**********
 ** Icon **
 **********/

template <typename PT, int W, int H>
Icon<PT, W, H>::Icon()
{
	memset(_pixel,  0, sizeof(_pixel));
	memset(_alpha,  0, sizeof(_alpha));
	memset(_shadow, 0, sizeof(_shadow));
	_icon_alpha = 255;
}


template <typename PT, int W, int H>
void Icon<PT, W, H>::rgba(unsigned char *src, int vshift, int shadow)
{
	/* convert rgba values to pixel type and alpha channel */
	for (int j = 0; j < H; j++)
		for (int i = 0; i < W; i++, src += 4) {
			_pixel[j][i].rgba(src[0], src[1], src[2]);
			_alpha[j][i] = src[3];
		}

	/* handle special case of no shadow */
	if (shadow == 0) return;

	/* generate shadow shape from blurred alpha channel */
	for (int j = 1; j < H - 4; j++)
		for (int i = 1; i < W - 2; i++) {
			int v = 0;
			for (int k = -1; k <= 1; k++)
				for (int l = -1; l <=1; l++)
					v += _alpha[(j + k + H)%H][(i + l + W)%W];

			_shadow[j + 3][i] = v>>shadow;
		}

	/* shift vertically */
	if (vshift > 0)
		for (int j = H - 1; j >= vshift; j--)
			for (int i = 0; i < W; i++) {
				_pixel[j][i] = _pixel[j - vshift][i];
				_alpha[j][i] = _alpha[j - vshift][i];
			}

	/* apply shadow to pixels */
	PT shcol(0, 0, 0);
	for (int j = 0; j < H; j++)
		for (int i = 0; i < W; i++) {
			_pixel[j][i] = PT::mix(shcol, _pixel[j][i], _alpha[j][i]);
			_alpha[j][i] = min(255, _alpha[j][i] + _shadow[j][i]);
		}
}


static inline void blur(unsigned char *src, unsigned char *dst, int w, int h)
{
	const int kernel = 3;
	int scale  = (kernel*2 + 1)*(kernel*2 + 1);

	scale = (scale*210)>>8;
	for (int j = kernel; j < h - kernel; j++)
		for (int i = kernel; i < w - kernel; i++) {
			int v = 0;
			for (int k = -kernel; k <= kernel; k++)
				for (int l = -kernel; l <= kernel; l++)
					v += src[w*(j + k) + (i + l)];

			dst[w*j + i] = min(v/scale, 255);
		}
}


template <typename PT, int W, int H>
void Icon<PT, W, H>::glow(unsigned char *src, Color c)
{
	/* extract shape from alpha channel of rgba source image */
	for (int j = 0; j < H; j++)
		for (int i = 0; i < W; i++, src += 4)
			_alpha[j][i] = src[3] ? 255 : 0;

	for (int i = 0; i < 2; i++) {
		blur(_alpha[0], _shadow[0], W, H);
		blur(_shadow[0], _alpha[0], W, H);
	}

	/* assign pixels and alpha */
	PT s(c.r, c.g, c.b);
	for (int j = 0; j < H; j++)
		for (int i = 0; i < W; i++, src += 4)
			_pixel[j][i] = s;
}


/*
 * An Icon has the following layout:
 *
 *  P1---+--------+----+
 *  | cs |   hs   | cs |   top row
 *  +----P2-------+----+
 *  |    |        |    |
 *  | vs |        | vs |   mid row
 *  |    |        |    |
 *  +----+--------P3---+
 *  | cs |   hs   | cs |   low row
 *  +------------------P4
 *
 * cs ... corner slice
 * hs ... horizontal slice
 * vs ... vertical slice
 */


/**
 * Copy pixel with alpha
 */
template <typename PT>
static inline void transfer_pixel(PT &src, int src_a, int alpha, PT *dst)
{
	if (src_a) {
		int register a = (src_a * alpha)>>8;
		if (a) *dst = PT::mix(*dst, src, a);
	}
}


/**
 * Draw corner slice
 */
template <typename PT>
static void draw_cslice(PT *src, unsigned char *src_a, int src_pitch, int alpha,
                        PT *dst, int dst_pitch, int w, int h)
{
	for (int j = 0; j < h; j++) {

		PT            *s  = src;
		unsigned char *sa = src_a;
		PT            *d  = dst;

		for (int i = 0; i < w; i++, s++, sa++, d++)
			transfer_pixel(*s, *sa, alpha, d);

		src += src_pitch, src_a += src_pitch, dst += dst_pitch;
	}
}


/**
 * Draw horizontal slice
 */
template <typename PT>
static void draw_hslice(PT *src, unsigned char *src_a, int src_pitch, int alpha,
                        PT *dst, int dst_pitch, int w, int h)
{
	for (int j = 0; j < h; j++) {

		PT   s = *src;
		int sa = *src_a;
		PT  *d =  dst;

		for (int i = 0; i < w; i++, d++)
			transfer_pixel(s, sa, alpha, d);

		src += src_pitch, src_a += src_pitch, dst += dst_pitch;
	}
}


/**
 * Draw vertical slice
 */
template <typename PT>
static void draw_vslice(PT *src, unsigned char *src_a, int src_pitch, int alpha,
                        PT *dst, int dst_pitch, int w, int h)
{
	for (int i = 0; i < w; i++) {

		PT   s = *src;
		int sa = *src_a;
		PT  *d =  dst;

		for (int j = 0; j < h; j++, d += dst_pitch)
			transfer_pixel(s, sa, alpha, d);

		src += 1, src_a += 1, dst += 1;
	}
}


/**
 * Draw center slice
 */
template <typename PT>
static void draw_center(PT *src, unsigned char *src_a, int src_pitch, int alpha,
                        PT *dst, int dst_pitch, int w, int h)
{
	PT   s = *src;
	int sa = *src_a;

	for (int j = 0; j < h; j++, dst += dst_pitch) {

		PT  *d =  dst;

		for (int i = 0; i < w; i++, d++)
			transfer_pixel(s, sa, alpha, d);
	}
}


/**
 * Clip rectangle against clipping region
 *
 * The out parameters are the resulting x/y offsets and the
 * visible width and height.
 *
 * \return  1 if rectangle intersects with clipping region,
 *          0 otherwise
 */
static inline int clip(int px1, int py1, int px2, int py2,
                       int cx1, int cy1, int cx2, int cy2,
                       int *out_x, int *out_y, int *out_w, int *out_h)
{
	/* determine intersection of rectangle and clipping region */
	int x1 = max(px1, cx1);
	int y1 = max(py1, cy1);
	int x2 = min(px2, cx2);
	int y2 = min(py2, cy2);

	*out_w = x2 - x1 + 1;
	*out_h = y2 - y1 + 1;
	*out_x = x1 - px1;
	*out_y = y1 - py1;

	return (*out_w > 0) && (*out_h > 0);
}


template <typename PT, int W, int H>
void Icon<PT, W, H>::draw(Canvas *c, int x, int y)
{
	PT *addr = static_cast<PT *>(c->addr());

	if (!addr || (_icon_alpha == 0)) return;

	const int cx1 = c->clip_x1();
	const int cy1 = c->clip_y1();
	const int cx2 = c->clip_x2();
	const int cy2 = c->clip_y2();

	/* determine point positions */
	const int x1 = x + _x;
	const int y1 = y + _y;
	const int x4 = x1 + _w - 1;
	const int y4 = y1 + _h - 1;
	const int x2 = x1 + W/2;
	const int y2 = y1 + H/2;
	const int x3 = max(x4 - W/2, x2);
	const int y3 = max(y4 - H/2, y2);

	const int tx1 = 0;
	const int ty1 = 0;
	const int tx4 = W - 1;
	const int ty4 = H - 1;
	const int tx2 = W/2;
	const int ty2 = H/2;
	const int tx3 = max(tx4 - W/2, tx2);
	const int ty3 = max(ty4 - H/2, ty2);

	PT            *src   = _pixel[0] + W*ty1;
	unsigned char *src_a = _alpha[0] + W*ty1;
	int            dx, dy, w, h;

	/*
	 * top row
	 */

	if (clip(x1, y1, x2 - 1, y2 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_cslice(src + tx1 + dy*W + dx, src_a + tx1 + dy*W + dx, W, _icon_alpha,
                    addr + (y1 + dy)*c->w() + x1 + dx, c->w(), w, h);

	if (clip(x2, y1, x3 - 1, y2 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_hslice(src + tx2 + dy*W + dx, src_a + tx2 + dy*W + dx, W, _icon_alpha,
                    addr + (y1 + dy)*c->w() + x2 + dx, c->w(), w, h);

	if (clip(x3, y1, x4, y2 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_cslice(src + tx3 + dy*W + dx, src_a + tx3 + dy*W + dx, W, _icon_alpha,
                    addr + (y1 + dy)*c->w() + x3 + dx, c->w(), w, h);

	/*
	 * mid row
	 */

	src   = _pixel[0] + W*ty2;
	src_a = _alpha[0] + W*ty2;

	if (clip(x1, y2, x2 - 1, y3 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_vslice(src + tx1 + dx, src_a + tx1 + dx, W, _icon_alpha,
                    addr + (y2 + dy)*c->w() + x1 + dx, c->w(), w, h);

	if (clip(x2, y2, x3 - 1, y3 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_center(src + tx2, src_a + tx2, W, _icon_alpha,
                    addr + (y2 + dy)*c->w() + x2 + dx, c->w(), w, h);

	if (clip(x3, y2, x4, y3 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_vslice(src + tx3 + dx, src_a + tx3 + dx, W, _icon_alpha,
                    addr + (y2 + dy)*c->w() + x3 + dx, c->w(), w, h);

	/*
	 * low row
	 */

	src   = _pixel[0] + W*ty3;
	src_a = _alpha[0] + W*ty3;

	if (clip(x1, y3, x2 - 1, y4, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_cslice(src + tx1 + dy*W + dx, src_a + tx1 + dy*W + dx, W, _icon_alpha,
                    addr + (y3 + dy)*c->w() + x1 + dx, c->w(), w, h);

	if (clip(x2, y3, x3 - 1, y4, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_hslice(src + tx2 + dy*W + dx, src_a + tx2 + dy*W + dx, W, _icon_alpha,
                    addr + (y3 + dy)*c->w() + x2 + dx, c->w(), w, h);

	if (clip(x3, y3, x4, y4, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
		draw_cslice(src + tx3 + dy*W + dx, src_a + tx3 + dy*W + dx, W, _icon_alpha,
                    addr + (y3 + dy)*c->w() + x3 + dx, c->w(), w, h);
}


template <typename PT, int W, int H>
Element *Icon<PT, W, H>::find(int x, int y)
{
	if (!Element::find(x, y)) return 0;

	x -= _x;
	y -= _y;

	/* check icon boundaries (the height is flexible) */
	if ((x < 0) || (x >= W) || (y < 0) || (y >= _h)) return 0;

	/* upper part of the icon */
	if (y <=     H/2) return _alpha[y][x]          ? this : 0;

	/* lower part of the icon */
	if (y > _h - H/2) return _alpha[y - _h + H][x] ? this : 0;

	/* middle part of the icon */
	if (_alpha[H/2][x]) return this;

	return 0;
}

#include "canvas_rgb565.h"
template class Horizontal_shadow<Pixel_rgb565, 40>;
template class Horizontal_shadow<Pixel_rgb565, 160>;
template class Icon<Pixel_rgb565, 16, 16>;
template class Icon<Pixel_rgb565, 32, 32>;
template class Icon<Pixel_rgb565, 64, 64>;
