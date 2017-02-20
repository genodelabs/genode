/*
 * \brief  Accessor for textures
 * \author Norman Feske
 * \date   2015-09-16
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <gems/chunky_texture.h>
#include <base/env.h>
#include <os/pixel_rgb565.h>
#include <os/texture_rgb565.h>

/* local includes */
#include "canvas.h"

using namespace Genode;


template <typename PT>
class Icon_texture : public Chunky_texture<PT>
{
	public:

		/**
		 * Known dimensions of the statically linked RGBA pixel data
		 */
		enum { WIDTH = 14, HEIGHT = 14 };

		Icon_texture(Ram_session &ram, Region_map &rm,
		             unsigned char rgba_data[])
		:
			Chunky_texture<PT>(ram, rm, Surface_base::Area(WIDTH, HEIGHT))
		{
			unsigned char  const *src            = rgba_data;
			size_t const          src_line_bytes = WIDTH*4;

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
Texture_base const &
Decorator::texture_by_id(Texture_id id, Ram_session &ram, Region_map &rm)
{
	static Icon_texture<Pixel_rgb565> const icons[4] {
		{ ram, rm, _binary_closer_rgba_start },
		{ ram, rm, _binary_minimize_rgba_start },
		{ ram, rm, _binary_maximize_rgba_start },
		{ ram, rm, _binary_windowed_rgba_start } };

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

