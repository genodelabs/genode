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
#include <base/registry.h>
#include <vfs/file_system.h>

namespace Vfs { class Rom_file_system; }


class Vfs::Rom_file_system : public Single_file_system
{
	private:

		enum Rom_type { ROM_TEXT, ROM_BINARY };

		Genode::Env &_env;

		Vfs::Env::User &_vfs_user;

		using Label = String<64>;

		Label const _label;

		bool const _binary;

		Genode::Attached_rom_dataspace _rom { _env, _label.string() };

		size_t _init_content_size()
		{
			if (!_binary)
				for (size_t pos = 0; pos < _rom.size(); pos++)
					if (_rom.local_addr<char>()[pos] == 0x00)
						return pos;

			return _rom.size();
		}

		size_t _content_size = _init_content_size();

		void _update()
		{
			_rom.update();
			_content_size = _init_content_size();
		}

		class Rom_vfs_handle : public Single_vfs_handle
		{
			private:

				Genode::Attached_rom_dataspace &_rom;

				size_t const &_content_size;

			public:

				Rom_vfs_handle(Directory_service              &ds,
				               File_io_service                &fs,
				               Genode::Allocator              &alloc,
				               Genode::Attached_rom_dataspace &rom,
				               size_t                   const &content_size)
				:
					Single_vfs_handle(ds, fs, alloc, 0),
					_rom(rom), _content_size(content_size)
				{ }

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					/* file read limit is the size of the dataspace */
					size_t const max_size = _content_size;

					/* current read offset */
					size_t const read_offset = size_t(seek());

					/* maximum read offset, clamped to dataspace size */
					size_t const end_offset = min(dst.num_bytes + read_offset, max_size);

					/* check if end of file is reached */
					if (read_offset >= end_offset) {
						out_count = 0;
						return READ_OK;
					}

					/* source address within the dataspace */
					char const *src = _rom.local_addr<char>() + read_offset;

					/* copy-out bytes from ROM dataspace */
					size_t const num_bytes = end_offset - read_offset;

					memcpy(dst.start, src, num_bytes);

					out_count = num_bytes;
					return READ_OK;
				}

				Write_result write(Const_byte_range_ptr const &, size_t &out_count) override
				{
					out_count = 0;
					return WRITE_ERR_INVALID;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return false; }
		};

		using Registered_watch_handle = Genode::Registered<Vfs_watch_handle>;
		using Watch_handle_registry   = Genode::Registry<Registered_watch_handle>;

		Watch_handle_registry _handle_registry { };

		void _handle_rom_changed()
		{
			_handle_registry.for_each([] (Registered_watch_handle &handle) {
				handle.watch_response(); });

			_vfs_user.wakeup_vfs_user();
		}

		Genode::Constructible<Genode::Io_signal_handler<Rom_file_system>>
			_rom_changed_handler { };

	public:

		Rom_file_system(Vfs::Env &env, Genode::Xml_node const &config)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, name(),
			                   Node_rwx::ro(), config),
			_env(env.env()), _vfs_user(env.user()),

			/* use 'label' attribute if present, fall back to 'name' if not */
			_label(config.attribute_value("label",
			                              config.attribute_value("name", Label()))),

			_binary(config.attribute_value("binary", true))
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

			_update();

			try {
				*out_handle = new (alloc)
					Rom_vfs_handle(*this, *this, alloc, _rom, _content_size);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Dataspace_capability dataspace(char const *path) override
		{
			if (!_single_file(path))
				return Genode::Dataspace_capability();

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
			if (out.type == Node_type::CONTINUOUS_FILE) {
				_update();
				out.size = _content_size;
				out.rwx  = { .readable   = true,
				             .writeable  = false,
				             .executable = true };
			}

			return result;
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			if (!_rom_changed_handler.constructed()) {
				_rom_changed_handler.construct(_env.ep(), *this,
				                               &Rom_file_system::_handle_rom_changed);
				_rom.sigh(*_rom_changed_handler);
			}

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);
				return WATCH_OK;
			}
			catch (Genode::Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			Genode::destroy(handle->alloc(),
			                static_cast<Registered_watch_handle *>(handle));
		}
};

#endif /* _INCLUDE__VFS__ROM_FILE_SYSTEM_H_ */
