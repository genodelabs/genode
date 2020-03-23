/*
 * \brief  File node
 * \author Norman Feske
 * \author Christian Helmuth
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

/* local includes */
#include <node.h>
#include <lx_util.h>


namespace Lx_fs {
	using namespace File_system;
	class File;
}


class Lx_fs::File : public Node
{
	private:

		int _fd;

		unsigned long _inode(int dir, char const *name, bool create)
		{
			int ret;

			if (create) {
				mode_t ugo = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
				ret = mknodat(dir, name, S_IFREG | ugo, 0);
				if (ret == -1 && errno != EEXIST)
					throw No_space();
			}

			struct stat s;

			ret = fstatat(dir, name, &s, 0);
			if (ret == -1)
				throw Lookup_failed();

			return s.st_ino;
		}

		unsigned long _inode_path(char const *path)
		{
			int         ret;
			struct stat s;

			ret = stat(path, &s);
			if (ret == -1)
				throw Lookup_failed();

			return s.st_ino;
		}

		int _open(int dir, char const *name, Mode mode)
		{
			int fd = openat(dir, name, access_mode(mode));
			if (fd == -1)
				throw Lookup_failed();

			return fd;
		}

		int _open_path(char const *path, Mode mode)
		{
			int fd = open(path, access_mode(mode));
			if (fd == -1)
				throw Lookup_failed();

			return fd;
		}

	public:

		File(int         dir,
		     char const *name,
		     Mode        mode,
		     bool        create)
		:
			Node(_inode(dir, name, create)),
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

		~File()
		{
			close(_fd);
		}

		void update_modification_time(Timestamp const time) override
		{
			struct timespec ts[2] = {
				{ .tv_sec = (time_t)0,          .tv_nsec = 0 },
				{ .tv_sec = (time_t)time.value, .tv_nsec = 0 }
			};

			/* silently ignore errors */
			futimens(_fd, (const timespec*)&ts);
		}

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			int ret = pread(_fd, dst, len, seek_offset);

			return ret == -1 ? 0 : ret;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			/* should we append? */
			if (seek_offset == ~0ULL) {
				::off_t off = lseek(_fd, 0, SEEK_END);
				if (off == -1)
					return 0;
				seek_offset = off;
			}

			int ret = pwrite(_fd, src, len, seek_offset);

			return ret == -1 ? 0 : ret;
		}

		bool sync() override
		{
			int ret = fsync(_fd);
			return ret ? false : true;
		}

		Status status() override
		{
			struct stat st { };

			if (fstat(_fd, &st) < 0) {
				st.st_size  = 0;
				st.st_mtime = 0;
			}

			return {
				.size  = (file_size_t)st.st_size,
				.type  = File_system::Node_type::CONTINUOUS_FILE,
				.rwx   = { .readable   = (st.st_mode & S_IRUSR),
				           .writeable  = (st.st_mode & S_IWUSR),
				           .executable = (st.st_mode & S_IXUSR) },
				.inode = inode(),
				.modification_time = { st.st_mtime }
			};
		}

		void truncate(file_size_t size) override
		{
			if (ftruncate(_fd, size)) /* nothing */ { }

			mark_as_updated();
		}
};

#endif /* _FILE_H_ */
