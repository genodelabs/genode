/*
 * \brief  ROM filesystem
 * \author Norman Feske
 * \date   2014-04-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__ROM_FILE_SYSTEM_H_
#define _INCLUDE__VFS__ROM_FILE_SYSTEM_H_

#include <base/attached_rom_dataspace.h>
#include <vfs/file_system.h>

namespace Vfs { class Rom_file_system; }


class Vfs::Rom_file_system : public Single_file_system
{
	private:

		enum Rom_type { ROM_TEXT, ROM_BINARY };

		struct Label
		{
			enum { LABEL_MAX_LEN = 64 };
			char string[LABEL_MAX_LEN];
			bool const binary;

			Label(Xml_node config)
			:
				binary(config.attribute_value("binary", true))
			{
				/* obtain label from config */
				string[0] = 0;
				try { config.attribute("label").value(string, sizeof(string)); }
				catch (...)
				{
					/* use VFS node name if label was not provided */
					string[0] = 0;
					try { config.attribute("name").value(string, sizeof(string)); }
					catch (...) { }
				}
			}
		} _label;

		Genode::Attached_rom_dataspace _rom;

		class Rom_vfs_handle : public Single_vfs_handle
		{
			private:

				Genode::Attached_rom_dataspace &_rom;

				file_size const _content_size;

				file_size _init_content_size(Rom_type type)
				{
					if (type == ROM_TEXT) {
						for (file_size pos = 0; pos < _rom.size(); pos++) 
							if (_rom.local_addr<char>()[pos] == 0x00)
								return pos;
					}

					return _rom.size();
				}

			public:

				Rom_vfs_handle(Directory_service              &ds,
				               File_io_service                &fs,
				               Genode::Allocator              &alloc,
				               Genode::Attached_rom_dataspace &rom,
				               Rom_type                        type)
				: Single_vfs_handle(ds, fs, alloc, 0), _rom(rom), _content_size(_init_content_size(type)) { }

				Read_result read(char *dst, file_size count,
				                 file_size &out_count) override
				{
					/* file read limit is the size of the dataspace */
					file_size const max_size = _content_size;

					/* current read offset */
					file_size const read_offset = seek();

					/* maximum read offset, clamped to dataspace size */
					file_size const end_offset = min(count + read_offset, max_size);

					/* source address within the dataspace */
					char const *src = _rom.local_addr<char>() + read_offset;

					/* check if end of file is reached */
					if (read_offset >= end_offset) {
						out_count = 0;
						return READ_OK;
					}

					/* copy-out bytes from ROM dataspace */
					file_size const num_bytes = end_offset - read_offset;

					memcpy(dst, src, num_bytes);

					out_count = num_bytes;
					return READ_OK;
				}

				Write_result write(char const *, file_size,
				                   file_size &out_count) override
				{
					out_count = 0;
					return WRITE_ERR_INVALID;
				}

				bool read_ready() { return true; }
		};

	public:

		Rom_file_system(Vfs::Env &env,
		                Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_FILE, name(), config),
			_label(config),
			_rom(env.env(), _label.string)
		{ }

		static char const *name()   { return "rom"; }
		char const *type() override { return "rom"; }

		/*********************************
		 ** Directory-service interface **
		 ********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			_rom.update();

			try {
				*out_handle = new (alloc)
					Rom_vfs_handle(*this, *this, alloc, _rom, _label.binary ? ROM_BINARY : ROM_TEXT);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Dataspace_capability dataspace(char const *) override
		{
			return _rom.cap();
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result const result = Single_file_system::stat(path, out);

			/*
			 * If the stat call refers to our node ('Single_file_system::stat'
			 * found a file), obtain the size of the most current ROM module
			 * version.
			 */
			if (out.mode == STAT_MODE_FILE) {
				_rom.update();
				out.size = _rom.valid() ? _rom.size() : 0;
				out.mode |= 0555;
			}

			return result;
		}
};

#endif /* _INCLUDE__VFS__ROM_FILE_SYSTEM_H_ */
