/**
 * \brief  File node
 * \author Norman Feske
 * \author Christian Helmuth
 * \auhtor Sebastian Sumpf
 * \date   2013-11-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_H_
#define _FILE_H_

/* Genode includes */
#include <file_system/util.h>

#include "node.h"

namespace File_system {
	class File;
}

class File_system::File : public Node
{
	private:

		int _fd;

		int _access_mode(Mode const &mode)
		{
			switch (mode) {
				case STAT_ONLY:
				case READ_ONLY:  return O_RDONLY;
				case WRITE_ONLY: return O_WRONLY;
				case READ_WRITE: return O_RDWR;
				default:         return O_RDONLY;
			}
		}

		unsigned long _inode(int dir, char const *name, bool create)
		{
			int ret;

			if (create) {
				mode_t ugo = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
				ret = rump_sys_mknodat(dir, name, S_IFREG | ugo, 0);
				if (ret == -1 && errno != EEXIST)
					throw No_space();
			}

			struct stat s;

			ret = rump_sys_fstatat(dir, name, &s, 0);
			if (ret == -1)
				throw Lookup_failed();

			return s.st_ino;
		}

		unsigned long _inode_path(char const *path)
		{
			int         ret;
			struct stat s;

			ret = rump_sys_stat(path, &s);
			if (ret == -1)
				throw Lookup_failed();

			return s.st_ino;
		}

		int _open(int dir, char const *name, Mode mode)
		{
			int fd = rump_sys_openat(dir, name, _access_mode(mode));
			if (fd == -1)
				throw Lookup_failed();

			return fd;
		}

		int _open_path(char const *path, Mode mode)
		{
			int fd = rump_sys_open(path, _access_mode(mode));
			if (fd == -1)
				throw Lookup_failed();

			return fd;
		}

	public:

		File(int         dir,
		     char const *name,
		     Mode        mode,
		     bool        create)
		: Node(_inode(dir, name, create)),
		 _fd(_open(dir, name, mode))
		{
			Node::name(name);
		}

		File(char const *path, Mode mode)
		:
			Node(_inode_path(path)),
			_fd(_open_path(path, mode))
		{
			Node::name(basename(path));
		}

		virtual ~File() { rump_sys_close(_fd); }

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			ssize_t ret;

			if (seek_offset == SEEK_TAIL)
				ret = rump_sys_lseek(_fd, -len, SEEK_END) != -1 ?
					rump_sys_read(_fd, dst, len) : 0;
			else
				ret = rump_sys_pread(_fd, dst, len, seek_offset);

			return ret == -1 ? 0 : ret;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset)
		{
			ssize_t ret;

			if (seek_offset == SEEK_TAIL)
				ret = rump_sys_lseek(_fd, 0, SEEK_END) != -1 ?
					rump_sys_write(_fd, src, len) : 0;
			else
				ret = rump_sys_pwrite(_fd, src, len, seek_offset);

			return ret == -1 ? 0 : ret;
		}

		file_size_t length() const
		{
			struct stat s;

			if(rump_sys_fstat(_fd, &s) < 0)
				return 0;

			return s.st_size;
		}

		void truncate(file_size_t size)
		{
			rump_sys_ftruncate(_fd, size);

			mark_as_updated();
		}
};

#endif /* _FILE_H_ */
