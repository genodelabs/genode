/*
 * \brief   GUI elements
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <scout/misc_math.h>

#include "widgets.h"

using namespace Scout;


/*************
 ** Docview **
 *************/

void Docview::format_fixed_width(int w)
{
	_min_size = Area(0, 0);

	if (_cont) {
		_cont->format_fixed_width(w - 2*_padx - _right_pad);
		_min_size = Area(w, _voffset + _cont->min_size().h());
	}

	if (_bg)
		_bg->geometry(Rect(Point(0, 0), _min_size));
}


void Docview::draw(Canvas_base &canvas, Point abs_position)
{
	if (_bg)     _bg->draw(canvas, _position + abs_position);
	if (_cont) _cont->draw(canvas, _position + abs_position);
}


Element *Docview::find(Point position)
{
	if (!Element::find(position)) return 0;
	Element *res = _cont ? _cont->find(position - _position) : 0;
	return res ? res : this;
}


void Docview::geometry(Rect rect)
{
	Element::geometry(rect);

	if (_cont)
		_cont->geometry(Rect(Point(_padx, _voffset),
		                     Area(_cont->min_size().w(), rect.h() - _voffset)));
}


/***********************
 ** Horizontal shadow **
 ***********************/

template <typename PT, int INTENSITY>
void Horizontal_shadow<PT, INTENSITY>::draw(Canvas_base &canvas, Point abs_position)
{
	canvas.draw_horizontal_shadow(Rect(abs_position + _position, _size), INTENSITY);
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


template <typename PT, int W, int H>
void Icon<PT, W, H>::draw(Canvas_base &canvas, Point abs_position)
{
	Texture<PT> texture(_pixel[0], _alpha[0], Area(W, H));
	canvas.draw_icon(Rect(abs_position + _position, _size), texture, _icon_alpha);
}


template <typename PT, int W, int H>
Element *Icon<PT, W, H>::find(Point position)
{
	if (!Element::find(position)) return 0;

	position = position - _position;

	/* check icon boundaries (the height is flexible) */
	if ((position.x() < 0) || (position.x() >= W)
	 || (position.y() < 0) || (position.y() >= (int)_size.h())) return 0;

	/* upper part of the icon */
	if (position.y() <= H/2) return _alpha[position.y()][position.x()] ? this : 0;

	/* lower part of the icon */
	if (position.y() > (int)_size.h() - H/2)
		return _alpha[position.y() - _size.h() + H][position.x()] ? this : 0;

	/* middle part of the icon */
	if (_alpha[H/2][position.x()]) return this;

	return 0;
}

template class Horizontal_shadow<Genode::Pixel_rgb565, 40>;
template class Horizontal_shadow<Genode::Pixel_rgb565, 160>;
template class Icon<Genode::Pixel_rgb565, 16, 16>;
template class Icon<Genode::Pixel_rgb565, 32, 32>;
template class Icon<Genode::Pixel_rgb565, 64, 64>;
