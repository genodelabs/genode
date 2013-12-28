/*
 * \brief   Font representation
 * \date    2005-10-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__FONT_H_
#define _INCLUDE__NITPICKER_GFX__FONT_H_

#include <base/stdint.h>

class Font
{
	private:

		typedef Genode::int32_t int32_t;

	public:

		unsigned char const *img;            /* font image         */
		int           const  img_w, img_h;   /* size of font image */
		int32_t       const *otab;           /* offset table       */
		int32_t       const *wtab;           /* width table        */

		/**
		 * Construct font from a TFF data block
		 */
		Font(const char *tff)
		:
			img((unsigned char *)(tff + 2056)),

			img_w(*((int32_t *)(tff + 2048))),
			img_h(*((int32_t *)(tff + 2052))),

			otab((int32_t *)(tff)),
			wtab((int32_t *)(tff + 1024))
		{ }

		/**
		 * Calculate width of string when printed with the font
		 */
		int str_w(const char *sstr) const
		{
			const unsigned char *str = (const unsigned char *)sstr;
			int res = 0;
			for (; str && *str; str++) res += wtab[*str];
			return res;
		}

		/**
		 * Calculate height of string when printed with the font
		 */
		int str_h(const char *str) const { return img_h; }
};

#endif /* _INCLUDE__NITPICKER_GFX__FONT_H_ */
