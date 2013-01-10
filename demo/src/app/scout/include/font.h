/*
 * \brief   Font representation
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FONT_H_
#define _FONT_H_

#include "scout_types.h"

class Font
{
	private:

		typedef scout_int32_t int32_t;

	public:

		unsigned char *img;              /* font image         */
		int            img_w, img_h;     /* size of font image */
		int32_t       *wtab;             /* width table        */
		int32_t       *otab;             /* offset table       */

		/**
		 * Construct font from a TFF data block
		 */
		explicit Font(const char *tff)
		{
			otab  =   (int32_t       *)(tff);
			wtab  =   (int32_t       *)(tff + 1024);
			img_w = *((int32_t       *)(tff + 2048));
			img_h = *((int32_t       *)(tff + 2052));
			img   =   (unsigned char *)(tff + 2056);
		}

		/**
		 * Calculate width of string when printed with the font
		 */
		int str_w(const char *sstr, int len)
		{
			const unsigned char *str = (const unsigned char *)sstr;
			int res = 0;
			for (; str && *str && len; len--, str++) res += wtab[*str];
			return res;
		}

		/**
		 * Calculate height of string when printed with the font
		 */
		int str_h(const char *str, int len) { return img_h; }
};


#endif /* _FONT_H_ */
