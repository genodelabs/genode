/*
 * \brief  File system for providing a read-only value as a file
 * \author Norman Feske
 * \date   2018-03-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__READONLY_VALUE_FILE_SYSTEM_H_
#define _INCLUDE__VFS__READONLY_VALUE_FILE_SYSTEM_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <vfs/single_file_system.h>

namespace Vfs {
	template <typename, unsigned BUF_SIZE = 128>
	class Readonly_value_file_system;
}


template <typename T, unsigned BUF_SIZE>
class Vfs::Readonly_value_file_system : public Vfs::Single_file_system
{
	public:

		typedef Genode::String<64> Name;

	private:

		typedef Genode::String<BUF_SIZE + 1> Buffer;

		Name const _file_name;

		Buffer _buffer { };

		struct Vfs_handle : Single_vfs_handle
		{
			Buffer const &_buffer;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Allocator         &alloc,
			           Buffer      const &buffer)
			:
				Single_vfs_handle(ds, fs, alloc, 0), _buffer(buffer)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				out_count = 0;

				if (seek() > _buffer.length())
					return READ_ERR_INVALID;

				char const *   const src = _buffer.string() + seek();
				Genode::size_t const len = min(_buffer.length() - seek(), count);
				Genode::memcpy(dst, src, len);

				out_count = len;
				return READ_OK;
			}

			Write_result write(char const *, file_size, file_size &) override
			{
				return WRITE_ERR_IO;
			}

			bool read_ready() override { return true; }
		};

		typedef Genode::String<200> Config;
		Config _config(Name const &name) const
		{
			char buf[Config::capacity()] { };
			Genode::Xml_generator xml(buf, sizeof(buf), type_name(), [&] () {
				xml.attribute("name", name); });
			return Config(Genode::Cstring(buf));
		}

		typedef Genode::Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Genode::Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

	public:

		Readonly_value_file_system(Name const &name, T const &initial_value)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type(),
			                   Node_rwx::ro(), Xml_node(_config(name).string())),
			_file_name(name)
		{
			value(initial_value);
		}

		static char const *type_name() { return "readonly_value"; }

		char const *type() override { return type_name(); }

		void value(T const &value)
		{
			_buffer = Buffer(value);

			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		bool matches(Xml_node node) const
		{
			return node.has_type(type_name()) &&
			       node.attribute_value("name", Name()) == _file_name;
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
					Vfs_handle(*this, *this, alloc, _buffer);

				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = _buffer.length();
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
			catch (Genode::Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			Genode::destroy(handle->alloc(),
			                static_cast<Registered_watch_handle *>(handle));
		}
};

#endif /* _INCLUDE__VFS__READONLY_VALUE_FILE_SYSTEM_H_ */
