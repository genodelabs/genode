/*
 * \brief   Implementation of refracted icons
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * A refracted icon is a icon that refracts its background
 * using a distortion map.
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "config.h"
#include "miscmath.h"
#include "refracted_icon.h"


/***************
 ** Utilities **
 ***************/

/**
 * Backup original (background) pixel data into back buffer
 */
template <typename PT>
static void filter_src_to_backbuf(PT *src, int src_w,
                                  PT *dst, int dst_w, int dst_h, int width)
{
	for (int j = 0; j < (dst_h>>1); j++, src += src_w, dst += 2*dst_w) {
		for (int i = 0; i < width; i++) {
			dst[2*i] = src[i];
			dst[2*i + 1] = PT::avr(src[i], src[i + 1]);
			dst[2*i + dst_w] = PT::avr(src[i], src[i + src_w]);
			dst[2*i + dst_w + 1] = PT::avr(dst[2*i + dst_w], dst[2*i + 1]);
		}
	}
}


/**
 * Backup original (background) pixel data into back buffer
 */
template <typename PT>
static void copy_src_to_backbuf(PT *src, int src_w,
                                PT *dst, int dst_w, int dst_h, int width)
{
	for (int j = 0; j < (dst_h>>1); j++, src += src_w, dst += 2*dst_w)
		for (int i = 0; i < width; i++)
			dst[2*i] = dst[2*i + 1] = dst[2*i + dst_w] = dst[2*i + dst_w + 1] = src[i];
}


/**
 * Copy and distort back-buffer pixels to front buffer
 */
template <typename PT, typename DT>
void distort(PT src[], DT  distmap[], int distmap_w, int distmap_h,
                    PT  fg[], unsigned char alpha[],
                    PT dst[], int dst_w, int width)
{
	int line_offset = (distmap_w>>1) - width;
	width <<= 1;

	for (int j = 0; j < distmap_h; j += 2, dst += dst_w) {

		PT *d = dst;

		for (int i = 0; i < width; i += 2, src += 2, distmap += 2) {

			/* fetch distorted pixel from back buffer */
			PT v = PT::avr(src[distmap[0]],
			               src[distmap[1] + 1],
			               src[distmap[distmap_w] + distmap_w],
			               src[distmap[distmap_w + 1] + distmap_w + 1]);

			/* mix back-buffer pixel with foreground */
			*d++ = PT::mix(v, *fg++, *alpha++);
		}

		fg      += line_offset;
		alpha   += line_offset;
		src     += line_offset*2 + distmap_w;   /* skip one line in back buffer */
		distmap += line_offset*2 + distmap_w;   /* skip one line in distmap     */
	}
}


/**
 * Copy and distort back-buffer pixels to front buffer
 */
template <typename PT>
void copy(PT src[], int src_w, PT dst[], int dst_w, int w, int h)
{
	for (int j = 0; j < h; j ++, src += src_w, dst += dst_w)
		memcpy(dst, src, w*sizeof(PT));
}


/******************************
 ** Refracted icon interface **
 ******************************/

template <typename PT, typename DT>
void Refracted_icon<PT, DT>::scratch(int jitter)
{
	PT ref_color = _fg[0];
	for (int j = 0; j < _distmap_h; j++) for (int i = 0; i < _distmap_w; i++) {

		int fg_offset = (j>>1)*(_distmap_w>>1) + (i>>1);

		int dr = _fg[fg_offset].r() - ref_color.r();
		int dg = _fg[fg_offset].g() - ref_color.g();
		int db = _fg[fg_offset].b() - ref_color.b();

		if (dr < 0) dr = -dr;
		if (dg < 0) dg = -dg;
		if (db < 0) db = -db;

		static const int limit = 20;
		if (dr > limit || dg > limit || db > limit) continue;

		int dx, dy;

		do {
			dx = jitter ? ((random()%jitter) - (jitter>>1)) : 0;
			dy = jitter ? ((random()%jitter) - (jitter>>1)) : 0;
		} while ((dx < -i) || (dx > _distmap_w - 2 - i)
		      || (dy < -j) || (dy > _distmap_h - 2 - j));

		_distmap[j*_distmap_w + i] += dy*_distmap_w + dx;
	}
}


/***********************
 ** Element interface **
 ***********************/

template <typename PT, typename DT>
void Refracted_icon<PT, DT>::draw(Canvas *c, int x, int y)
{
	PT *addr = static_cast<PT *>(c->addr());

	if (!addr || !_backbuf || !_fg || !_fg_alpha) return;

	/*
	 * NOTE: There is no support for clipping.
	 *       Use this code with caution!
	 */

	addr += c->w()*(y + _y) + x + _x;

	int fg_w = _distmap_w>>1;

	for (int i = 0; i < _w; i += fg_w, addr += fg_w) {

		int curr_w = min(fg_w, _w - i);

		if (Config::iconbar_detail == 0) {
			copy(_fg, _distmap_w>>1, addr, c->w(), curr_w, _distmap_h>>1);
			continue;
		}

		/* backup old canvas pixels */
		if (_filter_backbuf)
			filter_src_to_backbuf(addr, c->w(), _backbuf, _distmap_w,
			                      _distmap_h, fg_w);
		else
			copy_src_to_backbuf(addr, c->w(), _backbuf, _distmap_w,
			                    _distmap_h, fg_w);

		/* draw distorted pixels back to canvas */
		distort<PT, DT>(_backbuf, _distmap, _distmap_w, _distmap_h,
		                _fg, _fg_alpha, addr, c->w(), curr_w);
	}
}


#include "canvas_rgb565.h"
template class Refracted_icon<Pixel_rgb565, short>;
