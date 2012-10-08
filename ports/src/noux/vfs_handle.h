/*
 * \brief  Representation of an open file
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__VFS_HANDLE_H_
#define _NOUX__VFS_HANDLE_H_

/* Genode includes */
#include <base/printf.h>

/* Noux includes */
#include <file_io_service.h>
#include <directory_service.h>

namespace Noux {

	class Sysio;

	class Vfs_io_channel;

	class Vfs_handle
	{
		private:

			Directory_service *_ds;
			File_io_service   *_fs;
			int                _status_flags;
			size_t             _seek;

			friend class Vfs_io_channel; /* for modifying '_seek' */

			static Directory_service *_pseudo_directory_service()
			{
				struct Pseudo_directory_service : public Directory_service
				{
					static bool _msg(char const *sc) {
						PERR("%s not supported by directory service", sc); return false; }

					Dataspace_capability dataspace(char const *)
					{
						_msg("dataspace");
						return Dataspace_capability();
					}

					void release(char const *, Dataspace_capability) { }

					bool        stat(Sysio *, char const *)                 { return _msg("stat"); }
					Vfs_handle *open(Sysio *, char const *)                 { _msg("open"); return 0; }
					bool        dirent(Sysio *, char const *, off_t)        { return _msg("dirent"); }
					bool        unlink(Sysio *, char const *)               { return _msg("unlink"); }
					bool        readlink(Sysio *, char const *)             { return _msg("readlink"); }
					bool        rename(Sysio *, char const *, char const *) { return _msg("rename"); }
					bool        mkdir(Sysio *, char const *)                { return _msg("mkdir"); }
					bool        symlink(Sysio *, char const *)              { return _msg("symlink"); }
					size_t      num_dirent(char const *)                    { return 0; }
					bool        is_directory(char const *)                  { return false; }
					char const *leaf_path(char const *path)                 { return 0; }
				};
				static Pseudo_directory_service ds;
				return &ds;
			}

			static File_io_service *_pseudo_file_io_service()
			{
				struct Pseudo_file_io_service : public File_io_service
				{
					static bool _msg(char const *sc) {
						PERR("%s not supported by file system", sc); return false; }

					bool     write(Sysio *sysio, Vfs_handle *handle) { return _msg("write"); }
					bool      read(Sysio *sysio, Vfs_handle *handle) { return _msg("read"); }
					bool ftruncate(Sysio *sysio, Vfs_handle *handle) { return _msg("ftruncate"); }
				};
				static Pseudo_file_io_service fs;
				return &fs;
			}

		public:

			enum { STATUS_RDONLY = 0, STATUS_WRONLY = 1, STATUS_RDWR = 2 };

			Vfs_handle(Directory_service *ds, File_io_service *fs, int status_flags)
			:
				_ds(ds ? ds : _pseudo_directory_service()),
				_fs(fs ? fs : _pseudo_file_io_service()),
				_status_flags(status_flags),
				_seek(0)
			{ }

			virtual ~Vfs_handle() { }

			Directory_service *ds() { return _ds; }
			File_io_service   *fs() { return _fs; }

			int status_flags() { return _status_flags; }

			size_t seek() const { return _seek; }
	};
}

#endif /* _NOUX__VFS_HANDLE_H_ */
