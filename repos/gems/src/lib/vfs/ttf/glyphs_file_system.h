/*
 * \brief  Glyphs file system
 * \author Norman Feske
 * \date   2018-03-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GLYPHS_FILE_SYSTEM_H_
#define _GLYPHS_FILE_SYSTEM_H_

/* Genode includes */
#include <vfs/single_file_system.h>
#include <nitpicker_gfx/text_painter.h>

/* gems includes */
#include <gems/vfs_font.h>

namespace Vfs {

	using namespace Genode;

	class Glyphs_file_system;

	typedef Text_painter::Font     Font;
	typedef Vfs_font::Glyph_header Glyph_header;
}


class Vfs::Glyphs_file_system : public Vfs::Single_file_system
{
	private:

		static constexpr unsigned  UNICODE_MAX = 0x10ffff;

		static constexpr file_size FILE_SIZE = Vfs_font::GLYPH_SLOT_BYTES*(UNICODE_MAX + 1);

		Font const &_font;

		struct Vfs_handle : Single_vfs_handle
		{
			Font const &_font;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Allocator         &alloc,
			           Font        const &font)
			:
				Single_vfs_handle(ds, fs, alloc, 0), _font(font)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				out_count = 0;

				if (seek() > FILE_SIZE)
					return READ_ERR_INVALID;

				Codepoint const codepoint { (uint32_t)(seek() / Vfs_font::GLYPH_SLOT_BYTES) };

				file_size byte_offset = seek() % Vfs_font::GLYPH_SLOT_BYTES;

				_font.apply_glyph(codepoint, [&] (Glyph_painter::Glyph const &glyph) {

					if (byte_offset < sizeof(Glyph_header)) {

						Glyph_header const header(glyph);

						char const * const src = (char const *)&header + byte_offset;
						size_t const len = min(sizeof(header) - (size_t)byte_offset, (size_t)count);
						memcpy(dst, src, len);

						dst         += len;
						byte_offset += len;
						count       -= len;
						out_count   += len;
					}

					/*
					 * Given that 'byte_offset' is at least 'sizeof(header)',
					 * continue working with 'alpha_offset', which is the first
					 * offset of interest within the array of alpha values.
					 */
					size_t const alpha_offset = (size_t)byte_offset - sizeof(Glyph_header);
					size_t const alpha_values_len = 4*glyph.width*glyph.height;

					if (alpha_offset < alpha_values_len) {
						char const * const src = (char const *)glyph.values + alpha_offset;
						size_t const len = min(alpha_values_len - alpha_offset, (size_t)count);
						memcpy(dst, src, len);
						out_count += len;
					}
				});

				return READ_OK;
			}

			Write_result write(char const *, file_size, file_size &) override
			{
				return WRITE_ERR_IO;
			}

			bool read_ready() override { return true; }
		};

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

	public:

		Glyphs_file_system(Font const &font)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type(),
			                   Node_rwx::ro(), Xml_node("<glyphs/>")),
			_font(font)
		{ }

		static char const *type_name() { return "glyphs"; }

		char const *type() override { return type_name(); }


		/**
		 * Propagate font change to watch handlers
		 */
		void trigger_watch_response()
		{
			_handle_registry.for_each([] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc)
					Vfs_handle(*this, *this, alloc, _font);
				return OPEN_OK;
			}
			catch (Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = FILE_SIZE;
			return result;
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}
};

#endif /* _GLYPHS_FILE_SYSTEM_H_ */
