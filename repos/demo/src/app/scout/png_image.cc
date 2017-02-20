/*
 * \brief   PNG image element
 * \date    2005-11-07
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <png.h>
#include <scout/misc_math.h>
#include <mini_c/init.h>

#include "elements.h"

using namespace Scout;


static Genode::Allocator *_alloc_ptr;
static Genode::Allocator &alloc()
{
	if (_alloc_ptr)
		return *_alloc_ptr;

	Genode::error("missing call of Png_image::init");
	class Png_image_init_missing { };
	throw Png_image_init_missing();
}


class Png_stream
{
	private:

		char *_addr;

	public:

		/**
		 * Constructor
		 */
		Png_stream(char *addr) { _addr = addr; }

		/**
		 * Read from png stream
		 */
		void read(char *dst, int len)
		{
			Scout::memcpy(dst, _addr, len);
			_addr += len;
		}
};


/**
 * PNG read callback
 */
static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t len)
{
	Png_stream *stream = (Png_stream *)png_get_io_ptr(png_ptr);

	stream->read((char *)data, len);
}


/**
 * Dummy to make libl4png happy
 */
extern "C" int l4libpng_fread(void *buf, int size, int nmemb, void *stream)
{
	printf("l4libpng_fread called - function not implemented\n");
	return 0;
}


void Png_image::init(Genode::Allocator &alloc)
{
	_alloc_ptr = &alloc;
	mini_c_init(alloc);
}


/***********************
 ** Element interface **
 ***********************/

void Png_image::fill_cache(Canvas_base &canvas)
{
	if (_texture) return;

	Png_stream *stream = new (alloc()) Png_stream((char *)_png_data);

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png_ptr) return;

	png_set_read_fn(png_ptr, stream, user_read_data);

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
		return;
	}

	png_read_info(png_ptr, info_ptr);

	/* get image data chunk */
	int bit_depth, color_type, interlace_type;
	png_uint_32 w, h;
	png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
	             &interlace_type, int_p_NULL, int_p_NULL);

	_min_size = Scout::Area(w, h);
	printf("png is %d x %d, depth=%d\n", _min_size.w(), _min_size.h(), bit_depth);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (bit_depth < 8) png_set_packing(png_ptr);
	if (bit_depth == 16) png_set_strip_16(png_ptr);

	_texture = canvas.alloc_texture(_min_size, true);

	/* allocate buffer for decoding a row */
	static png_byte *row_ptr;
	static int curr_row_size;

	int needed_row_size = png_get_rowbytes(png_ptr, info_ptr)*8;

	if (curr_row_size < needed_row_size) {
		if (row_ptr) alloc().free(row_ptr, curr_row_size);
		row_ptr = (png_byte *)alloc().alloc(needed_row_size);
		curr_row_size = needed_row_size;
	}

	/* fill texture */
	for (unsigned j = 0; j < _min_size.h(); j++) {
		png_read_row(png_ptr, row_ptr, NULL);
		canvas.set_rgba_texture(_texture, (unsigned char *)row_ptr, _min_size.w(), j);
	}
}


void Png_image::flush_cache(Canvas_base &canvas)
{
	canvas.free_texture(_texture);
	_texture = 0;
}


void Png_image::draw(Canvas_base &canvas, Point abs_position)
{
	/* if texture is not ready, try to initialize it */
	if (!_texture) fill_cache(canvas);

	/* draw texture */
	if (_texture)
		canvas.draw_texture(abs_position + _position, *_texture);
}
