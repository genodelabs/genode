/*
 * \brief  Accessor for the default font
 * \author Norman Feske
 * \date   2015-09-16
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <gems/chunky_texture.h>
#include <base/env.h>
#include <os/pixel_rgb565.h>
#include <os/texture_rgb565.h>

/* local includes */
#include "canvas.h"


template <typename PT>
class Icon_texture : public Chunky_texture<PT>
{
	public:

		/**
		 * Known dimensions of the statically linked RGBA pixel data
		 */
		enum { WIDTH = 14, HEIGHT = 14 };

		Icon_texture(Genode::Ram_session &ram, unsigned char rgba_data[])
		:
			Chunky_texture<PT>(ram, Genode::Surface_base::Area(WIDTH, HEIGHT))
		{
			unsigned char  const *src            = rgba_data;
			Genode::size_t const  src_line_bytes = WIDTH*4;

			for (unsigned y = 0; y < HEIGHT; y++, src += src_line_bytes)
				Chunky_texture<PT>::rgba(src, WIDTH, y);
		}
};


/**
 * Statically linked binary data
 */
extern unsigned char _binary_closer_rgba_start[];
extern unsigned char _binary_minimize_rgba_start[];
extern unsigned char _binary_maximize_rgba_start[];
extern unsigned char _binary_windowed_rgba_start[];


/**
 * Return texture for the specified texture ID
 */
Genode::Texture_base const &Decorator::texture_by_id(Texture_id id)
{
	Genode::Ram_session &ram = *Genode::env()->ram_session();

	static Icon_texture<Genode::Pixel_rgb565> const icons[4] {
		{ ram, _binary_closer_rgba_start },
		{ ram, _binary_minimize_rgba_start },
		{ ram, _binary_maximize_rgba_start },
		{ ram, _binary_windowed_rgba_start } };

	switch (id) {

	case TEXTURE_ID_CLOSER:    /* fall through... */
	case TEXTURE_ID_MINIMIZE:
	case TEXTURE_ID_MAXIMIZE:
	case TEXTURE_ID_WINDOWED:
		return icons[id];

	default:
		break;
	};

	struct Invalid_texture_id { };
	throw Invalid_texture_id();
}

