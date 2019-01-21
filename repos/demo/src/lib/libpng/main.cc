#include <base/log.h>

extern "C" {
#include <stdlib.h>
}

#include <png.h>

using namespace Genode;

static void user_read_data(png_structp, png_bytep, png_size_t) { }

int main(int, char **)
{
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png_ptr) return 1;

	png_set_read_fn(png_ptr, 0, user_read_data);

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
		return 2;
	}

	png_read_info(png_ptr, info_ptr);

	/* get image data chunk */
	int bit_depth, color_type, interlace_type;
	png_uint_32 w, h;
	png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
	             &interlace_type, int_p_NULL, int_p_NULL);
	int _min_w = w;
	int _min_h = h;
	log("png is ", _min_w, " x ", _min_h, ", depth=", bit_depth);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (bit_depth < 8) png_set_packing(png_ptr);
	if (bit_depth == 16) png_set_strip_16(png_ptr);

	/* allocate buffer for decoding */
	png_byte **row_ptrs = (png_byte **)malloc(_min_h * sizeof(png_byte*));

	int needed_row_size = png_get_rowbytes(png_ptr, info_ptr)*8;
	for (int i = 0; i < _min_h; ++i )
		row_ptrs[i] = (png_byte *)malloc(needed_row_size);

	/* fill texture */
	png_read_image(png_ptr, row_ptrs);

	return 0;
}
