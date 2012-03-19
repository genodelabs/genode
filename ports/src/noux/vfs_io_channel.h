/*
 * \brief  I/O channel for files opened via virtual directory service
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__VFS_IO_CHANNEL_H_
#define _NOUX__VFS_IO_CHANNEL_H_

/* Noux includes */
#include <io_channel.h>
#include <file_system.h>
#include <pwd.h>

namespace Noux {

	struct Vfs_io_channel : public Io_channel
	{
		Vfs        *_vfs;
		Vfs_handle *_fh;

		Absolute_path _path;

		Vfs_io_channel(char const *path, Vfs *vfs, Vfs_handle *vfs_handle)
		: _vfs(vfs), _fh(vfs_handle), _path(path) { }

		~Vfs_io_channel() { _vfs->close(_fh); }

		bool write(Sysio *sysio) { return _fh->fs()->write(sysio, _fh); }

		bool read(Sysio *sysio)
		{
			if (!_fh->fs()->read(sysio, _fh))
				return false;

			_fh->_seek += sysio->read_out.count;
			return true;
		}

		bool fstat(Sysio *sysio) { return _fh->ds()->stat(sysio, _path.base()); }

		bool fcntl(Sysio *sysio)
		{
			switch (sysio->fcntl_in.cmd) {

			case Sysio::FCNTL_CMD_SET_FD_FLAGS:

				return true;

			case Sysio::FCNTL_CMD_GET_FILE_STATUS_FLAGS:

				sysio->fcntl_out.result = _fh->status_flags();
				return true;

			default:

				PWRN("invalid fcntl command %d", sysio->fcntl_in.cmd);
				sysio->error.fcntl = Sysio::FCNTL_ERR_CMD_INVALID;
				return false;
			};
		}

		bool fchdir(Sysio *sysio, Pwd *pwd)
		{
			pwd->pwd(_path.base());
			return true;
		}

		bool dirent(Sysio *sysio)
		{
			/*
			 * Return artificial dir entries for "." and ".."
			 */
			unsigned const index = sysio->dirent_in.index;
			if (index < 2) {
				sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_DIRECTORY;
				Genode::strncpy(sysio->dirent_out.entry.name,
				                index ? ".." : ".",
				                sizeof(sysio->dirent_out.entry.name));

				sysio->dirent_out.entry.fileno = 1;
				return true;
			}

			/*
			 * Delegate remaining dir-entry request to the actual file
			 * system.
			 */
			sysio->dirent_in.index -= 2;
			return _fh->ds()->dirent(sysio, _path.base());
		}

		bool check_unblock(bool rd, bool wr, bool ex) const
		{
			/*
			 * XXX For now, we use the TAR fs only, which never blocks.
			 *     However, real file systems may block.
			 */
			return true;
		}
	};
}

#endif /* _NOUX__VFS_IO_CHANNEL_H_ */
