/*
 * \brief   Sky texture interface
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SKY_TEXTURE_H_
#define _SKY_TEXTURE_H_

#include <scout_gfx/sky_texture_painter.h>

#include "widgets.h"
#include "config.h"

namespace Scout { template <typename, int, int> class Sky_texture; }


/**
 * \param PT   pixel type (compatible to Pixel_rgba)
 * \param TW   tile width
 * \param TH   tile height
 */
template <typename PT, int TW, int TH>
class Scout::Sky_texture : public Element
{
	private:

		bool const _detailed;

		Sky_texture_painter::Static_sky_texture<PT, TW, TH> _sky_texture { };

	public:

		Sky_texture(bool detailed = true) : _detailed(detailed) { }

		void draw(Canvas_base &canvas, Point abs_position) override
		{
			canvas.draw_sky_texture(abs_position.y(), _sky_texture,
			                        _detailed);
		}
};

#endif /* _SKY_TEXTURE_H_ */
