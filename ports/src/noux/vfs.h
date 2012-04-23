/*
 * \brief  Management of file systems within virtual directory tree
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__VFS_H_
#define _NOUX__VFS_H_

/* Genode includes */
#include <util/list.h>
#include <dataspace/capability.h>

/* Noux includes */
#include <path.h>
#include <pwd.h>
#include <vfs_handle.h>
#include <file_system.h>
#include <noux_session/sysio.h>

namespace Noux {

	class Vfs
	{
		private:

			List<File_system> _file_systems;

		public:

			Dataspace_capability dataspace_from_file(char const *filename)
			{
				for (File_system *fs = _file_systems.first(); fs; fs = fs->next()) {
					char const *fs_local_path = fs->local_path(filename);
					Dataspace_capability ds_cap;
					if (fs_local_path && (ds_cap = fs->dataspace(fs_local_path)).valid())
						return ds_cap;
				}
				return Dataspace_capability();
			}

			void release_dataspace_for_file(char const *filename,
			                                Dataspace_capability ds_cap)
			{
				for (File_system *fs = _file_systems.first(); fs; fs = fs->next()) {
					char const *fs_local_path = fs->local_path(filename);
					if (fs_local_path)
						fs->release(ds_cap);
				}
			}

			void add_file_system(File_system *file_system)
			{
				_file_systems.insert(file_system);
			}

			bool stat(Sysio *sysio, char const *path)
			{
				for (File_system *fs = _file_systems.first(); fs; fs = fs->next()) {
					char const *fs_local_path = fs->local_path(path);
					if (fs_local_path && fs->stat(sysio, fs_local_path))
						return true;
				}

				sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
				return false;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				Vfs_handle *handle;
				for (File_system *fs = _file_systems.first(); fs; fs = fs->next()) {
					char const *fs_local_path = fs->local_path(path);
					if (fs_local_path && (handle = fs->open(sysio, fs_local_path)))
						return handle;
				}

				PWRN("no file system for \"%s\"", path);
				sysio->error.open = Sysio::OPEN_ERR_UNACCESSIBLE;
				return 0;
			}

			void close(Vfs_handle *handle)
			{
				handle->ds()->close(handle);
			}
	};
}

#endif /* _NOUX__VIRTUAL_DIRECTORY_SERVICE_H_ */
