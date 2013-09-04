/*
 * \brief  Texture handling
 * \author Norman Feske
 * \date   2010-09-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TEXTURE_H_
#define _TEXTURE_H_

/* libpng includes */
#include <png.h>

/***************
 ** Dithering **
 ***************/

enum { DITHER_SIZE = 16, DITHER_MASK = DITHER_SIZE - 1 };

static const int dither_matrix[DITHER_SIZE][DITHER_SIZE] = {
  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
};


static inline Genode::uint16_t rgb565(int r, int g, int b)
{
	enum {
		R_MASK = 0xf800, R_LSHIFT = 8,
		G_MASK = 0x07e0, G_LSHIFT = 3,
		B_MASK = 0x001f, B_RSHIFT = 3
	};
	return ((r << R_LSHIFT) & R_MASK)
	     | ((g << G_LSHIFT) & G_MASK)
	     | ((b >> B_RSHIFT) & B_MASK);
}


static void convert_line_rgba_to_rgb565(const unsigned char *rgba_src,
                                        Genode::uint16_t *dst,
                                        unsigned char *dst_alpha,
                                        int num_pixels, int line)
{
	using namespace Genode;
	enum { CHANNEL_MAX = 255 };

	int const *dm = dither_matrix[line & DITHER_MASK];

	for (int i = 0; i < num_pixels; i++) {
		int v = dm[i & DITHER_MASK] >> 5;

		*dst++ = rgb565(min(v + (int)rgba_src[0], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[1], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[2], (int)CHANNEL_MAX));

		/* use higher grain for low alpha channel values (i.e., drop shadows) */
		int grain = 2;
		if (rgba_src[3] < 50)
			grain = 6;
		*dst_alpha++ = max(0, min(v*grain + (int)(100*rgba_src[3])/95 - grain*8, (int)CHANNEL_MAX));

		rgba_src += 4; /* next pixel */
	}
}


/************************
 ** PNG image decoding **
 ************************/

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
			Genode::memcpy(dst, _addr, len);
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


class Png_image
{
	private:

		Png_stream _stream;
		png_structp _png_ptr;
		png_infop   _info_ptr;
		int _bit_depth, _color_type, _interlace_type;
		png_uint_32 _img_w, _img_h;

		png_structp _init_png_ptr()
		{
			png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
			png_set_read_fn(png_ptr, &_stream, user_read_data);
			return png_ptr;
		}

	public:

		Png_image(void *png_data)
		:
			_stream((char *)png_data),
			_png_ptr(_init_png_ptr()),
			_info_ptr(png_create_info_struct(_png_ptr)),
			_img_w(0), _img_h(0)
		{
			if (!_info_ptr) {
				png_destroy_read_struct(&_png_ptr, NULL, NULL);
				return;
			}

			png_read_info(_png_ptr, _info_ptr);

			/* get image data chunk */
			png_get_IHDR(_png_ptr, _info_ptr, &_img_w, &_img_h, &_bit_depth, &_color_type,
			             &_interlace_type, NULL, NULL);
		}

		void convert_to_rgb565(Genode::uint16_t *dst,
                               unsigned char *dst_alpha,
                               int dst_w, int dst_h)
		{
			if (_color_type == PNG_COLOR_TYPE_PALETTE)
				png_set_palette_to_rgb(_png_ptr);

			if (_color_type == PNG_COLOR_TYPE_GRAY
			 || _color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
				png_set_gray_to_rgb(_png_ptr);

			if (_bit_depth < 8)   png_set_packing(_png_ptr);
			if (_bit_depth == 16) png_set_strip_16(_png_ptr);

			/* allocate buffer for decoding a row */
			static png_byte *row_ptr;
			static int curr_row_size;

			int needed_row_size = png_get_rowbytes(_png_ptr, _info_ptr)*8;

			if (curr_row_size < needed_row_size) {
				if (row_ptr) Genode::env()->heap()->free(row_ptr, curr_row_size);
				row_ptr = (png_byte *)Genode::env()->heap()->alloc(needed_row_size);
				curr_row_size = needed_row_size;
			}

			/* fill texture */
			int dst_y = ((int)dst_h - (int)_img_h) / 2;
			for (int j = 0; j < (int)_img_h; j++, dst_y++) {
				png_read_row(_png_ptr, row_ptr, NULL);
				if (dst_y < 0 || dst_y >= (int)dst_h - 1) continue;
				convert_line_rgba_to_rgb565((unsigned char *)row_ptr, dst + dst_y*dst_w,
				                            dst_alpha + dst_y*dst_w,
				                            Genode::min(dst_w, (int)_img_w), j);
			}
		}

		unsigned width()  const { return _img_w; }
		unsigned height() const { return _img_h; }
};

#endif /* _TEXTURE_H_ */
