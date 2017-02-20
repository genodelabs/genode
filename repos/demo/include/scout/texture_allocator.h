/*
 * \brief   Generic interface of texture allocator
 * \date    2013-12-31
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__TEXTURE_ALLOCATOR_H_
#define _INCLUDE__SCOUT__TEXTURE_ALLOCATOR_H_

#include <os/texture.h>
#include <scout/types.h>


namespace Scout {
	struct Texture_allocator;

	using Genode::Texture;
	using Genode::Texture_base;

	struct Texture_allocator;
}


struct Scout::Texture_allocator
{
	virtual Texture_base *alloc_texture(Area size, bool alpha) = 0;

	virtual void free_texture(Texture_base *) = 0;

	virtual void set_rgba_texture(Texture_base *texture,
	                              unsigned char const *rgba,
	                              unsigned len, int y) = 0;
};

#endif /* _INCLUDE__SCOUT__TEXTURE_ALLOCATOR_H_ */
