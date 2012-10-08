/*
 * \brief  null filesystem
 * \author Josef Soentgen
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__NULL_FILE_SYSTEM_H_
#define _NOUX__NULL_FILE_SYSTEM_H_

/* Genode includes */
#include <base/printf.h>
#include <util/string.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include "file_system.h"


namespace Noux {

	class Null_file_system : public File_system
	{
		private:

			const char *_filename() { return "null"; }

			bool _is_root(const char *path)
			{
				return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
			}

			bool _is_null_file(const char *path)
			{
				return (strlen(path) == (strlen(_filename()) + 1)) &&
				        (strcmp(&path[1], _filename()) == 0);
			}

		public:

			Null_file_system() { }


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			Dataspace_capability dataspace(char const *path)
			{
				/* not supported */
				return Dataspace_capability();
			}

			void release(char const *path, Dataspace_capability ds_cap)
			{
				/* not supported */
			}

			bool stat(Sysio *sysio, char const *path)
			{
				Sysio::Stat st = { 0, 0, 0, 0, 0, 0 };

				if (_is_root(path))
					st.mode = Sysio::STAT_MODE_DIRECTORY;
				else if (_is_null_file(path)) {
					st.mode = Sysio::STAT_MODE_CHARDEV;
				} else {
					sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
					return false;
				}

				sysio->stat_out.st = st;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
			{
				if (_is_root(path)) {
					if (index == 0) {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_CHARDEV;
						strncpy(sysio->dirent_out.entry.name,
						        _filename(),
						        sizeof(sysio->dirent_out.entry.name));
					} else {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
					}

					return true;
				}

				return false;
			}

			size_t num_dirent(char const *path)
			{
				if (_is_root(path))
					return 1;
				else
					return 0;
			}

			bool is_directory(char const *path)
			{
				if (_is_root(path))
					return true;

				return false;
			}

			char const *leaf_path(char const *path)
			{
				return path;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				if (!_is_null_file(path)) {
					sysio->error.open = Sysio::OPEN_ERR_UNACCESSIBLE;
					return 0;
				}

				return new (env()->heap()) Vfs_handle(this, this, 0);
			}

			bool unlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool readlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool rename(Sysio *sysio, char const *from_path, char const *to_path)
			{
				/* not supported */
				return false;
			}

			bool mkdir(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool symlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			/***************************
			 ** File_system interface **
			 ***************************/

			char const *name() const { return "null"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *handle)
			{
				sysio->write_out.count = sysio->write_in.count;

				return true;
			}

			bool read(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				sysio->read_out.count = 0;

				return true;
			}

			bool ftruncate(Sysio *sysio, Vfs_handle *vfs_handle) { return true; }
	};
}

#endif /* _NOUX__NULL_FILE_SYSTEM_H_ */
