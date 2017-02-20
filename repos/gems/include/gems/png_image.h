/*
 * \brief  Utility for reading PNG images
 * \author Norman Feske
 * \date   2014-08-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__PNG_IMAGE_H_
#define _INCLUDE__GEMS__PNG_IMAGE_H_

/* Genode includes */
#include <os/texture.h>

/* libpng include */
#include <png.h>

/* gems includes */
#include <gems/chunky_texture.h>
#include <gems/texture_utils.h>

class Png_image
{
	public:

		/**
		 * Exception types
		 */
		class Read_struct_failed { };
		class Info_failed { };

	private:

		template <typename EXC, typename T>
		static T _assert_non_null(T && arg)
		{
			if (!arg)
				throw EXC();

			return arg;
		};

		Genode::Ram_session &_ram;
		Genode::Region_map  &_rm;
		Genode::Allocator   &_alloc;

		struct Read_struct
		{
			/* start of PNG data */
			png_bytep const data;

			/* read position, maintained by 'read_callback' */
			unsigned pos = 0;

			static void callback(png_structp png_ptr, png_bytep dst, png_size_t len)
			{
				Read_struct &read_struct = *(Read_struct *)png_get_io_ptr(png_ptr);
				Genode::memcpy(dst, read_struct.data + read_struct.pos, len);
				read_struct.pos += len;
			}

			png_structp png_ptr =
				_assert_non_null<Read_struct_failed>(
					png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0));

			Read_struct(void *data) : data((png_bytep)data)
			{
				png_set_read_fn(png_ptr, this, callback);
			}

			~Read_struct()
			{
				png_destroy_read_struct(&png_ptr, nullptr, nullptr);
			}
		};

		Read_struct _read_struct;

		struct Info
		{
			png_structp png_ptr;
			png_infop   info_ptr;

			int         bit_depth, color_type, interlace_type;
			png_uint_32 img_w, img_h;

			Info(png_structp png_ptr)
			:
				png_ptr(png_ptr),
				info_ptr(_assert_non_null<Info_failed>(png_create_info_struct(png_ptr)))
			{
				png_read_info(png_ptr, info_ptr);

				png_get_IHDR(png_ptr, info_ptr, &img_w, &img_h, &bit_depth, &color_type,
				             &interlace_type, nullptr, nullptr);

				if (color_type == PNG_COLOR_TYPE_PALETTE)
					png_set_palette_to_rgb(png_ptr);

				if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
					png_set_gray_to_rgb(png_ptr);

				if (bit_depth <   8) png_set_packing(png_ptr);
				if (bit_depth == 16) png_set_strip_16(png_ptr);
			}

			~Info()
			{
				png_destroy_info_struct(png_ptr, &info_ptr);
			}

		} _info { _read_struct.png_ptr };

		struct Row
		{
			Genode::Allocator &alloc;
			size_t const row_num_bytes;
			png_bytep const row_ptr;

			Row(Genode::Allocator &alloc, png_structp png_ptr, png_infop info_ptr)
			:
				alloc(alloc),
				row_num_bytes(png_get_rowbytes(png_ptr, info_ptr)*8),
				row_ptr((png_bytep)alloc.alloc(row_num_bytes))
			{ }

			~Row()
			{
				alloc.free(row_ptr, row_num_bytes);
			}
		} _row { _alloc, _read_struct.png_ptr, _info.info_ptr };

	public:

		/**
		 * Constructor
		 *
		 * \throw Read_struct_failed
		 * \throw Info_failed
		 */
		Png_image(Genode::Ram_session &ram, Genode::Region_map &rm,
		          Genode::Allocator &alloc, void *data)
		:
			_ram(ram), _rm(rm), _alloc(alloc), _read_struct(data)
		{ }

		/**
		 * Constructor
		 *
		 * \deprecated
		 * \noapi
		 */
		Png_image(void *data) __attribute__((deprecated))
		:
			_ram(*Genode::env_deprecated()->ram_session()),
			_rm(*Genode::env_deprecated()->rm_session()),
			_alloc(*Genode::env_deprecated()->heap()),
			_read_struct(data)
		{ }

		/**
		 * Return size of PNG image
		 */
		Genode::Surface_base::Area size() const
		{
			return Genode::Surface_base::Area(_info.img_w, _info.img_h);
		}

		/**
		 * Obtain PNG image as texture
		 */
		template <typename PT>
		Genode::Texture<PT> *texture()
		{
			Genode::Texture<PT> *texture = new (_alloc)
				Chunky_texture<PT>(_ram, _rm, size());

			/* fill texture with PNG image data */
			for (unsigned i = 0; i < size().h(); i++) {
				png_read_row(_read_struct.png_ptr, _row.row_ptr, NULL);
				texture->rgba((unsigned char *)_row.row_ptr, size().w()*4, i);
			}

			return texture;
		}

		/**
		 * Free texture obtained via 'texture()'
		 */
		template <typename PT>
		void release_texture(Genode::Texture<PT> *texture)
		{
			Chunky_texture<PT> *chunky_texture =
				static_cast<Chunky_texture<PT> *>(texture);

			Genode::destroy(_alloc, chunky_texture);
		}
};

#endif /* _INCLUDE__GEMS__PNG_IMAGE_H_ */
