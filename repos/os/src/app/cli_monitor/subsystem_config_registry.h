/*
 * \brief  Registry of subsystem configuration
 * \author Norman Feske
 * \date   2015-01-27
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SUBSYSTEM_CONFIG_REGISTRY_H_
#define _SUBSYSTEM_CONFIG_REGISTRY_H_

/* Genode includes */
#include <vfs/file_system.h>
#include <vfs/vfs_handle.h>

namespace Cli_monitor { class Subsystem_config_registry; }


class Cli_monitor::Subsystem_config_registry
{
	public:

		/**
		 * Exception type
		 */
		class Nonexistent_subsystem_config { };

	private:

		Vfs::File_system  &_fs;
		Genode::Allocator &_alloc;

		enum { CONFIG_BUF_SIZE = 32*1024 };
		char _config_buf[CONFIG_BUF_SIZE];

		char const *_subsystems_path() { return "/subsystems"; }
		char const *_subsystem_suffix() { return ".subsystem"; }

		/**
		 * Return index of ".subsystem" suffix in dirent name
		 *
		 * \return index, or 0 if no matching suffix could be found
		 */
		unsigned _subsystem_suffix(Vfs::Directory_service::Dirent const &dirent)
		{
			unsigned found = 0;
			for (unsigned i = 0; i < sizeof(dirent.name) && dirent.name[i]; i++)
				if (Genode::strcmp(_subsystem_suffix(), &dirent.name[i]) == 0)
					found = i;

			return found;
		}


	public:

		/**
		 * Constructor
		 */
		Subsystem_config_registry(Vfs::File_system &fs, Genode::Allocator &alloc)
		:
			_fs(fs), _alloc(alloc)
		{ }

		/**
		 * Execute functor 'fn' for specified subsystem name
		 *
		 * The functor is called with the subsystem XML node as argument
		 *
		 * \throw Nonexistent_subsystem_config
		 */
		template <typename FN>
		void for_config(char const *name, FN const &fn)
		{
			using Genode::error;

			/*
			 * Load subsystem configuration
			 */

			Genode::Path<256> path(_subsystems_path());
			path.append("/");
			path.append(name);
			path.append(_subsystem_suffix());

			Vfs::Vfs_handle *handle = nullptr;

			Vfs::Directory_service::Open_result const open_result =
				_fs.open(path.base(),
				         Vfs::Directory_service::OPEN_MODE_RDONLY,
				         &handle, _alloc);

			Vfs::Vfs_handle::Guard handle_guard(handle);

			if (open_result != Vfs::Directory_service::OPEN_OK) {
				error("could not open '", path, "', err=", (int)open_result);
				throw Nonexistent_subsystem_config();
			}

			Vfs::file_size out_count = 0;
			Vfs::File_io_service::Read_result read_result =
				handle->fs().read(handle, _config_buf, sizeof(_config_buf), out_count);

			if (read_result != Vfs::File_io_service::READ_OK) {
				error("could not read '", path, "', err=", (int)read_result);
				throw Nonexistent_subsystem_config();
			}

			try {
				Genode::Xml_node subsystem_node(_config_buf, out_count);
				fn(subsystem_node);

			} catch (Genode::Xml_node::Invalid_syntax) {
				error("subsystem configuration has invalid syntax");
				throw Nonexistent_subsystem_config();

			} catch (Genode::Xml_node::Nonexistent_sub_node) {
				error("invalid subsystem configuration");
				throw Nonexistent_subsystem_config();
			}
		}

		/**
		 * Call specified functor for each subsystem config
		 */
		template <typename FN>
		void for_each_config(FN const &fn)
		{
			using Genode::error;

			/* iterate over the directory entries */
			for (unsigned i = 0;; i++) {

				Vfs::Directory_service::Dirent dirent;

				Vfs::Directory_service::Dirent_result dirent_result =
					_fs.dirent(_subsystems_path(), i, dirent);

				if (dirent_result != Vfs::Directory_service::DIRENT_OK) {
					error("could not access directory '", _subsystems_path(), "'");
					return;
				}

				if (dirent.type == Vfs::Directory_service::DIRENT_TYPE_END)
					return;

				unsigned const subsystem_suffix = _subsystem_suffix(dirent);

				/* if file has a matching suffix, apply 'fn' */
				if (subsystem_suffix) {

					/* subsystem name is file name without the suffix */
					char name[sizeof(dirent.name)];
					Genode::strncpy(name, dirent.name, subsystem_suffix + 1);

					try {
						for_config(name, fn);
					} catch (Nonexistent_subsystem_config) { }
				}
			}
		}
};

#endif /* _PROCESS_ARG_REGISTRY_H_ */
