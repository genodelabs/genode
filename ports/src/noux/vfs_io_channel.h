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
#include <dir_file_system.h>
#include <pwd.h>

namespace Noux {

	struct Vfs_io_channel : public Io_channel
	{
		Vfs_handle *_fh;

		Absolute_path _path;
		Absolute_path _leaf_path;

		Vfs_io_channel(char const *path, char const *leaf_path,
		               Dir_file_system *root_dir, Vfs_handle *vfs_handle)
		: _fh(vfs_handle), _path(path), _leaf_path(leaf_path) { }

		~Vfs_io_channel() { destroy(env()->heap(), _fh); }

		bool write(Sysio *sysio, size_t &count)
		{
			if (!_fh->fs()->write(sysio, _fh))
				return false;

			count = sysio->write_out.count;
			_fh->_seek += count;
			return true;
		}

		bool read(Sysio *sysio)
		{
			if (!_fh->fs()->read(sysio, _fh))
				return false;

			_fh->_seek += sysio->read_out.count;
			return true;
		}

		bool fstat(Sysio *sysio)
		{
			/*
			 * 'sysio.stat_in' is not used in '_fh->ds()->stat()',
			 * so no 'sysio' member translation is needed here
			 */
			bool result = _fh->ds()->stat(sysio, _leaf_path.base());
			sysio->fstat_out.st = sysio->stat_out.st;
			return result;
		}

		bool ftruncate(Sysio *sysio)
		{
			return _fh->fs()->ftruncate(sysio, _fh);
		}

		bool fcntl(Sysio *sysio)
		{
			switch (sysio->fcntl_in.cmd) {

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
			sysio->fstat_in.fd = sysio->fchdir_in.fd;

			fstat(sysio);

			if ((sysio->fstat_out.st.mode & Sysio::STAT_MODE_DIRECTORY) !=
				Sysio::STAT_MODE_DIRECTORY) {
				sysio->error.fchdir = Sysio::FCHDIR_ERR_NOT_DIR;
				return false;
			}

			pwd->pwd(_path.base());
			return true;
		}

		/*
		 * The 'dirent' function for the root directory only (the
		 * 'Dir_file_system::open()' function handles all requests referring
		 * to directories). Hence, '_path' is the absolute path of the
		 * directory to inspect.
		 */
		bool dirent(Sysio *sysio)
		{
			/*
			 * Return artificial dir entries for "." and ".."
			 */
			unsigned const index = _fh->seek() / sizeof(Sysio::Dirent);
			if (index < 2) {
				sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_DIRECTORY;
				strncpy(sysio->dirent_out.entry.name,
				        index ? ".." : ".",
				        sizeof(sysio->dirent_out.entry.name));

				sysio->dirent_out.entry.fileno = 1;
				_fh->_seek += sizeof(Sysio::Dirent);
				return true;
			}

			/*
			 * Delegate remaining dir-entry request to the actual file system.
			 * Align index range to zero when calling the directory service.
			 */

			if (!_fh->ds()->dirent(sysio, _path.base(), index - 2))
				return false;

			_fh->_seek += sizeof(Sysio::Dirent);
			return true;
		}

		/**
		 * Return size of file that the I/O channel refers to
		 *
		 * Note that this function overwrites the 'sysio' argument. Do not
		 * call it prior saving all input arguments from the original sysio
		 * structure.
		 */
		size_t size(Sysio *sysio)
		{
			if (fstat(sysio))
				return sysio->fstat_out.st.size;

			return 0;
		}

		bool lseek(Sysio *sysio)
		{
			switch (sysio->lseek_in.whence) {
			case Sysio::LSEEK_SET: _fh->_seek = sysio->lseek_in.offset; break;
			case Sysio::LSEEK_CUR: _fh->_seek += sysio->lseek_in.offset; break;
			case Sysio::LSEEK_END:
				off_t offset = sysio->lseek_in.offset;
				sysio->fstat_in.fd = sysio->lseek_in.fd;
				_fh->_seek = size(sysio) + offset;
				break;
			}
			sysio->lseek_out.offset = _fh->_seek;
			return true;
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
