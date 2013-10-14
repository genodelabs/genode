/*
 * \brief  Backdrop for Nitpicker
 * \author Norman Feske
 * \date   2009-08-28
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* libpng includes */
#include <png.h>

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <nitpicker_view/client.h>
#include <framebuffer_session/client.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <util/misc_math.h>
#include <os/config.h>

using namespace Genode;


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


static inline uint16_t rgb565(int r, int g, int b)
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
                                        uint16_t *dst, int num_pixels, int line)
{
	enum { CHANNEL_MAX = 255 };

	int const *dm = dither_matrix[line & DITHER_MASK];

	for (int i = 0; i < num_pixels; i++) {
		int v = dm[i & DITHER_MASK] >> 5;

		*dst++ = rgb565(min(v + (int)rgba_src[0], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[1], (int)CHANNEL_MAX),
		                min(v + (int)rgba_src[2], (int)CHANNEL_MAX));

		/* we ignore the alpha channel */

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



static void convert_png_to_rgb565(void *png_data,
                                  uint16_t *dst, int dst_w, int dst_h)
{
	Png_stream *stream = new (env()->heap()) Png_stream((char *)png_data);

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
	png_uint_32 img_w, img_h;
	png_get_IHDR(png_ptr, info_ptr, &img_w, &img_h, &bit_depth, &color_type,
	             &interlace_type, int_p_NULL, int_p_NULL);
	printf("png is %d x %d, depth=%d\n", (int)img_w, (int)img_h, bit_depth);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (bit_depth < 8) png_set_packing(png_ptr);
	if (bit_depth == 16) png_set_strip_16(png_ptr);

	/* allocate buffer for decoding a row */
	static png_byte *row_ptr;
	static int curr_row_size;

	int needed_row_size = png_get_rowbytes(png_ptr, info_ptr)*8;

	if (curr_row_size < needed_row_size) {
		if (row_ptr) env()->heap()->free(row_ptr, curr_row_size);
		row_ptr = (png_byte *)env()->heap()->alloc(needed_row_size);
		curr_row_size = needed_row_size;
	}

	/* fill texture */
	int dst_y = 0;
	for (int j = 0; j < min((int)img_h, dst_h); j++, dst_y++) {
		png_read_row(png_ptr, row_ptr, NULL);
		convert_line_rgba_to_rgb565((unsigned char *)row_ptr, dst + dst_y*dst_w,
		                            min(dst_w, (int)img_w), j);
	}
}


/****************************
 ** Configuration handling **
 ****************************/

/**
 * Determine PNG filename of image to be used as background
 *
 * \param dst      destination buffer for storing the filename
 * \param dst_len  size of destination buffer
 * \return         0 on success
 */
static int read_image_filename_from_config(char *dst, Genode::size_t dst_len)
{
	try {
		Xml_node image_xml = config()->xml_node().sub_node("image");
		image_xml.value(dst, dst_len);
		return 0;
	} catch (Xml_node::Nonexistent_sub_node) {
		printf("Error: Configuration has no 'image' declaration.\n");
		return -2;
	}
}


/******************
 ** Main program **
 ******************/

int main(int argc, char **argv)
{
	enum { PNG_NAME_MAX = 128 };
	static char png_name[PNG_NAME_MAX];

	if (read_image_filename_from_config(png_name, sizeof(png_name)) < 0)
		return -1;

	printf("using PNG file \"%s\" as background\n", png_name);

	static void *png_data;
	try {
		static Rom_connection png_rom(png_name);
		png_data = env()->rm_session()->attach(png_rom.dataspace());
	} catch (...) {
		printf("Error: Could not obtain PNG image from ROM service\n");
		return -2;
	}

	static Nitpicker::Connection nitpicker;

	/* obtain physical screen size */
	Framebuffer::Mode const mode = nitpicker.mode();

	/* setup virtual framebuffer mode */
	nitpicker.buffer(mode, false);

	static Framebuffer::Session_client framebuffer(nitpicker.framebuffer_session());
	Nitpicker::View_capability         view_cap = nitpicker.create_view();
	static Nitpicker::View_client      view(view_cap);

	if (mode.format() != Framebuffer::Mode::RGB565) {
		printf("Error: Color mode %d not supported\n", (int)mode.format());
		return -3;
	}

	/* make virtual framebuffer locally accessible */
	uint16_t *fb = env()->rm_session()->attach(framebuffer.dataspace());

	/* fill virtual framebuffer with decoded image data */
	convert_png_to_rgb565(png_data, fb, mode.width(), mode.height());

	/* display view behind all others */
	nitpicker.background(view_cap);
	view.viewport(0, 0, mode.width(), mode.height(), 0, 0, false);
	view.stack(Nitpicker::View_capability(), false, false);
	framebuffer.refresh(0, 0, mode.width(), mode.height());

	sleep_forever();
	return 0;
}
