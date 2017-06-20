/**
 * \brief  File-system directory node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2013-11-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

/* Genode include */
#include <os/path.h>
#include <file_system/util.h>
#include <base/log.h>

/* local includes */
#include "node.h"
#include "file.h"
#include "symlink.h"

namespace Rump_fs {
	class Directory;
}

class Rump_fs::Directory : public Node
{
	private:

		enum { BUFFER_SIZE = 4096 };

		typedef Genode::Path<MAX_PATH_LEN> Path;

		int        _fd;
		Path       _path;
		Allocator &_alloc;

		unsigned long _inode(char const *path, bool create)
		{
			int ret;

			if (create) {
				mode_t ugo = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
				ret = rump_sys_mkdir(path, ugo);
				if (ret == -1)
					switch (errno) {
					case ENAMETOOLONG: throw Name_too_long();
					case EACCES:       throw Permission_denied();
					case ENOENT:       throw Lookup_failed();
					case EEXIST:       throw Node_already_exists();
					case ENOSPC:
					default:           throw No_space();
					}
			}

			struct stat s;

			ret = rump_sys_lstat(path, &s);
			if (ret == -1)
				throw Lookup_failed();
			return s.st_ino;
		}

		int _open(char const *path)
		{
			struct stat s;
			int ret = rump_sys_lstat(path, &s);
			if (ret == -1 || !S_ISDIR(s.st_mode))
				throw Lookup_failed();

			int fd = rump_sys_open(path, O_RDONLY);
			if (fd == -1)
				throw Lookup_failed();

			return fd;
		}

		static char *_buffer()
		{
			/* buffer for directory entries */
			static char buf[BUFFER_SIZE];
			return buf;
		}

	public:

		Directory(Allocator &alloc, char const *path, bool create)
		:
			Node(_inode(path, create)),
			_fd(_open(path)),
			_path(path, "./"),
			_alloc(alloc)
		{
			Node::name(basename(path));
		}

		virtual ~Directory()
		{
			rump_sys_close(_fd);
		}

		int fd() const override { return _fd; }

		File * file(char const *name, Mode mode, bool create) override
		{
			return new (&_alloc) File(_fd, name, mode, create);
		}

		Symlink * symlink(char const *name, bool create) override
		{
			return new (&_alloc) Symlink(_path.base(), name, create);
		}

		Directory * subdir(char const *path, bool create)
		{
			Path dir_path(path, _path.base());
			Directory *dir = new (&_alloc) Directory(_alloc, dir_path.base(), create);
			return dir;
		}

		Node * node(char const *path)
		{
			Path node_path(path, _path.base());

			struct stat s;
			int ret = rump_sys_lstat(node_path.base(), &s);
			if (ret == -1)
				throw Lookup_failed();

			Node *node = 0;

			if (S_ISDIR(s.st_mode))
				node = new (&_alloc) Directory(_alloc, node_path.base(), false);
			else if (S_ISREG(s.st_mode))
				node = new (&_alloc) File(node_path.base(), STAT_ONLY);
			else if (S_ISLNK(s.st_mode))
				node = new (&_alloc)Symlink(node_path.base());
			else
				throw Lookup_failed();

			return node;
		}

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			if (len < sizeof(Directory_entry)) {
				Genode::error("read buffer too small for directory entry");
				return 0;
			}

			if (seek_offset % sizeof(Directory_entry)) {
				Genode::error("seek offset not aligned to sizeof(Directory_entry)");
				return 0;
			}

			seek_off_t index = seek_offset / sizeof(Directory_entry);

			int bytes;
			rump_sys_lseek(_fd, 0, SEEK_SET);

			struct dirent *dent = 0;
			seek_off_t        i = 0;
			char *buf = _buffer();
			do {
				bytes = rump_sys_getdents(_fd, buf, BUFFER_SIZE);
				void *current, *end;
				for (current = buf, end = &buf[bytes];
				     current < end;
				     current = _DIRENT_NEXT((dirent *)current))
				{
					struct ::dirent *d = (dirent*)current;
					if (strcmp(".", d->d_name) && strcmp("..", d->d_name)) {
						if (i == index) {
							dent = d;
							break;
						}
						++i;
					}
				}
			} while(bytes && !dent);

			if (!dent)
				return 0;

			/*
			 * Build absolute path, this becomes necessary as our 'Path' class strips
			 * trailing dots, which will not work for '.' and  '..' directories.
			 */
			size_t base_len = strlen(_path.base());
			char   path[dent->d_namlen + base_len + 2];

			memcpy(path, _path.base(), base_len);
			path[base_len] = '/';
			strncpy(path + base_len + 1, dent->d_name, dent->d_namlen + 1);

			/*
			 * We cannot use 'd_type' member of 'dirent' here since the EXT2
			 * implementation sets the type to unkown. Hence we use stat.
			 */
			struct stat s;
			rump_sys_lstat(path, &s);

			Directory_entry *e = (Directory_entry *)(dst);
			if (S_ISDIR(s.st_mode))
				e->type = Directory_entry::TYPE_DIRECTORY;
			else if (S_ISREG(s.st_mode))
				e->type = Directory_entry::TYPE_FILE;
			else if (S_ISLNK(s.st_mode))
				e->type = Directory_entry::TYPE_SYMLINK;
			else
				return 0;

			e->inode = s.st_ino;
			strncpy(e->name, dent->d_name, dent->d_namlen + 1);
			return sizeof(Directory_entry);
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			/* writing to directory nodes is not supported */
			return 0;
		}

		Status status() override
		{
			Status s;
			s.inode = inode();
			s.size  = num_entries() * sizeof (Directory_entry);
			s.mode  = File_system::Status::MODE_DIRECTORY;

			return s;
		}

		size_t num_entries() const
		{
			int bytes = 0;
			int count = 0;

			rump_sys_lseek(_fd, 0, SEEK_SET);

			char *buf = _buffer();
			do {
				bytes = rump_sys_getdents(_fd, buf, BUFFER_SIZE);
				void *current, *end;
				for (current = buf, end = &buf[bytes];
				     current < end;
				     current = _DIRENT_NEXT((dirent *)current), count++) { }
			} while(bytes);

			return count;
		}

		void unlink(char const *path) override
		{
			Path node_path(path, _path.base());

			struct stat s;
			int ret = rump_sys_lstat(node_path.base(), &s);
			if (ret == -1)
				throw Lookup_failed();

			if (S_ISDIR(s.st_mode))
				ret = rump_sys_rmdir(node_path.base());
			else if (S_ISREG(s.st_mode) || S_ISLNK(s.st_mode))
				ret = rump_sys_unlink(node_path.base());
			else
				throw Lookup_failed();

			if (ret == -1)
				Genode::error("error during unlink of ", node_path);
		}
};

#endif /* _DIRECTORY_H_ */
