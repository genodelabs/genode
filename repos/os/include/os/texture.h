/*
 * \brief  Texture representation
 * \author Norman Feske
 * \date   2013-12-30
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__TEXTURE_H_
#define _INCLUDE__OS__TEXTURE_H_

#include <os/surface.h>

namespace Genode {
	class Texture_base;
	template <typename PT> class Texture;
}


class Genode::Texture_base
{
	private:

		Surface_base::Area _size;

	public:

		Texture_base(Surface_base::Area size) : _size(size) { }

		Surface_base::Area size() const { return _size; }
};


template <typename PT>
class Genode::Texture : public Texture_base
{
	private:

		PT            *_pixel;
		unsigned char *_alpha;

	public:

		Texture(PT *pixel, unsigned char *alpha, Surface_base::Area size)
		: Texture_base(size), _pixel(pixel), _alpha(alpha) { }

		PT                  *pixel()       { return _pixel; }
		PT            const *pixel() const { return _pixel; }
		unsigned char       *alpha()       { return _alpha; }
		unsigned char const *alpha() const { return _alpha; }

		/**
		 * Import rgba data line into texture
		 */
		inline void rgba(unsigned char const *rgba, unsigned len, int y);
};

#endif /* _INCLUDE__OS__TEXTURE_H_ */
