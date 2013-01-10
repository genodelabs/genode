/*
 * \brief   Interface of refracted icon
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _REFRACTED_ICON_H_
#define _REFRACTED_ICON_H_

#include "widgets.h"

/**
 * \param PT  pixel type (must be Pixel_rgba compatible)
 * \param DT  distortion map entry type
 */
template <typename PT, typename DT>
class Refracted_icon : public Element
{
	private:

		PT            *_backbuf;                 /* pixel back buffer       */
		int            _filter_backbuf;          /* backbuf filtering flag  */
		DT            *_distmap;                 /* distortion table        */
		int            _distmap_w, _distmap_h;   /* size of distmap         */
		PT            *_fg;                      /* foreground pixels       */
		unsigned char *_fg_alpha;                /* foreground alpha values */

	public:

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
		void scratch(int jitter);

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

		void draw(Canvas *c, int px, int py);
};


#endif /* _REFRACTED_H_ */
