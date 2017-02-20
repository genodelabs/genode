/*
 * \brief  Texture with backing store for pixels and alpha channel
 * \author Norman Feske
 * \date   2014-08-22
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__CHUNKY_TEXTURE_H_
#define _INCLUDE__GEMS__CHUNKY_TEXTURE_H_

#include <os/surface.h>
#include <os/texture.h>
#include <base/attached_ram_dataspace.h>

template <typename PT>
class Chunky_texture : Genode::Attached_ram_dataspace, public Genode::Texture<PT>
{
	private:

		typedef Genode::Surface_base::Area Area;

		/**
		 * Calculate memory needed to store the texture
		 */
		static Genode::size_t _num_bytes(Area size)
		{
			/* account for pixel size + 1 byte per alpha value */
			return size.count()*(sizeof(PT) + 1);
		}

		/**
		 * Return base of pixel buffer
		 */
		PT *_pixel()
		{
			return local_addr<PT>();
		}

		/**
		 * Return base of alpha buffer
		 */
		unsigned char *_alpha(Area size)
		{
			/* alpha buffer follows pixel buffer */
			return (unsigned char *)(local_addr<PT>() + size.count());
		}

	public:

		Chunky_texture(Genode::Ram_session &ram, Genode::Region_map &rm,
		               Genode::Surface_base::Area size)
		:
			Genode::Attached_ram_dataspace(ram, rm, _num_bytes(size)),
			Genode::Texture<PT>(_pixel(), _alpha(size), size)
		{ }

		/**
		 * Constructor
		 *
		 * \deprecated
		 * \noapi
		 *
		 * This variant is solely meant to be used by deprecated functions.
		 * It will be removed if those functions are gone.
		 */
		Chunky_texture(Genode::Ram_session &ram, Genode::Surface_base::Area size) __attribute__((deprecated))
		:
			Genode::Attached_ram_dataspace(ram, *Genode::env_deprecated()->rm_session(), _num_bytes(size)),
			Genode::Texture<PT>(_pixel(), _alpha(size), size)
		{ }
};

#endif /* _INCLUDE__GEMS__CHUNKY_TEXTURE_H_ */
