/*
 * \brief   Interface of refracted icon
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REFRACTED_ICON_H_
#define _REFRACTED_ICON_H_

#include <scout_gfx/random.h>

#include "widgets.h"

namespace Scout { template <typename PT, typename DT> class Refracted_icon; }


/**
 * \param PT  pixel type (must be Pixel_rgba compatible)
 * \param DT  distortion map entry type
 */
template <typename PT, typename DT>
class Scout::Refracted_icon : public Element
{
	private:

		bool           _detailed = true;                /* level of detail         */
		PT            *_backbuf = nullptr;              /* pixel back buffer       */
		int            _filter_backbuf = 0;             /* backbuf filtering flag  */
		DT            *_distmap = nullptr;              /* distortion table        */
		int            _distmap_w = 0, _distmap_h = 0;  /* size of distmap         */
		PT            *_fg = nullptr;                   /* foreground pixels       */
		unsigned char *_fg_alpha = nullptr;             /* foreground alpha values */

	public:

		void detailed(bool detailed) { _detailed = detailed; }

		/**
		 * Define pixel back buffer for the icon.  This buffer is used for the
		 * draw operation.  It should have the same number of pixels as the
		 * distortion map.
		 */
		void backbuf(PT *backbuf, int filter_backbuf = 0)
		{
			_backbuf        = backbuf;
			_filter_backbuf = filter_backbuf;
		}

		/**
		 * Scratch refraction map
		 */
		void scratch(int jitter)
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
					using Scout::random;
					dx = jitter ? ((random()%jitter) - (jitter>>1)) : 0;
					dy = jitter ? ((random()%jitter) - (jitter>>1)) : 0;
				} while ((dx < -i) || (dx > _distmap_w - 2 - i)
				      || (dy < -j) || (dy > _distmap_h - 2 - j));

				_distmap[j*_distmap_w + i] += dy*_distmap_w + dx;
			}
		}

		/**
		 * Define distortion map for the icon
		 */
		void distmap(DT *distmap, int distmap_w, int distmap_h)
		{
			_distmap   = distmap;
			_distmap_w = distmap_w;
			_distmap_h = distmap_h;
		}

		/**
		 * Define foreground pixels
		 */
		void foreground(PT *fg, unsigned char *fg_alpha)
		{
			_fg       = fg;
			_fg_alpha = fg_alpha;
		}

		void draw(Canvas_base &canvas, Point abs_position)
		{
			Scout::Refracted_icon_painter::Distmap<short>
				distmap(_distmap, Area(_distmap_w, _distmap_h));

			Texture<PT> tmp(_backbuf, 0, Area(_distmap_w, _distmap_h));
			Texture<PT> fg(_fg, _fg_alpha, Area(_distmap_w/2, _distmap_h/2));

			canvas.draw_refracted_icon(_position + abs_position, distmap, tmp, fg,
			                           _detailed, _filter_backbuf);
		}
};

#endif /* _REFRACTED_H_ */
