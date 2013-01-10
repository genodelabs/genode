/*
 * \brief   Sky texture interface
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SKY_TEXTURE_H_
#define _SKY_TEXTURE_H_

#include "widgets.h"


/**
 * \param PT   pixel type (compatible to Pixel_rgba)
 * \param TW   tile width
 * \param TH   tile height
 */
template <typename PT, int TW, int TH>
class Sky_texture : public Texture
{
	private:

		short _bufs[3][TH][TW];
		short _buf[TH][TW];
		short _tmp[TH][TW];
		PT    _coltab[16*16*16];
		PT    _fallback[TH][TW];  /* fallback texture */

	public:

		Sky_texture();

		void draw(Canvas *c, int px, int py);
};

#endif /* _SKY_TEXTURE_H_ */
