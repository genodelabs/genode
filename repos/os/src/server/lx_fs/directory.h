/*
 * \brief  File-system directory node
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

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

/* Genode include */
#include <file_system/util.h>
#include <os/path.h>

/* local includes */
#include <node.h>
#include <file.h>

#include <lx_util.h>
#include <stdio.h>


namespace Lx_fs {
	using namespace Genode;
	using namespace File_system;
	class Directory;
}


class Lx_fs::Directory : public Node
{
	private:

		/*
		 * Noncopyable
		 */
		Directory(Directory const &);
		Directory &operator = (Directory const &);

		typedef Genode::Path<MAX_PATH_LEN> Path;

		DIR       *_fd;
		Path       _path;
		Allocator &_alloc;

		unsigned long _inode(char const *path, bool create)
		{
			int ret;

			if (create) {
				mode_t ugo = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
				ret = mkdir(path, ugo);
				if (ret == -1) {
					switch (errno) {
						case ENAMETOOLONG: throw Name_too_long();
						case EACCES:       throw Permission_denied();
						case ENOENT:       throw Lookup_failed();
						case EEXIST:       throw Node_already_exists();
						case ENOSPC:
						default:           throw No_space();
					}
				}
			}

			struct stat s { };

			ret = lstat(path, &s);
			if (ret == -1)
				throw Lookup_failed();

			return s.st_ino;
		}

		DIR *_open(char const *path)
		{
			DIR *fd = opendir(path);
			if (!fd)
				throw Lookup_failed();

			return fd;
		}

		size_t _num_entries() const
		{
			unsigned num = 0;

			rewinddir(_fd);
			while (readdir(_fd)) ++num;

			return num;
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
			closedir(_fd);
		}

		void update_modification_time(Timestamp const time) override
		{
			struct timespec ts[2] = {
				{ .tv_sec = (time_t)0,          .tv_nsec = 0 },
				{ .tv_sec = (time_t)time.value, .tv_nsec = 0 }
			};

			/* silently ignore errors */
			futimens(dirfd(_fd), (const timespec*)&ts);
		}

		void rename(Directory &dir_to, char const *name_from, char const *name_to)
		{
			int ret = renameat(dirfd(_fd), name_from,
			                   dirfd(dir_to._fd), name_to);
			if (ret != 0)
				throw Unavailable();
		}

		/* FIXME returned file node must be locked */
		File * file(char const *name, Mode mode, bool create) override
		{
			File *file = new (&_alloc) File(dirfd(_fd), name, mode, create);

			return file;
		}

		/* FIXME returned directory node must be locked */
		Directory * subdir(char const *path, bool create)
		{
			Path dir_path(path, _path.base());

			Directory *dir = new (&_alloc) Directory(_alloc, dir_path.base(), create);

			return dir;
		}

		Node * node(char const *path)
		{
			Path node_path(path, _path.base());

			/*
			 * XXX Currently, symlinks are transparently dereferenced by the
			 *     use of stat(). For symlink detection we would need lstat()
			 *     and implement special handling of the root, which may be a
			 *     link!
			 */

			struct stat s;
			int ret = stat(node_path.base(), &s);
			if (ret == -1)
				throw Lookup_failed();

			Node *node = 0;

			if (S_ISDIR(s.st_mode))
				node = new (&_alloc) Directory(_alloc, node_path.base(), false);
			else if (S_ISREG(s.st_mode))
				node = new (&_alloc) File(node_path.base(), STAT_ONLY);
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

			/* seek to index and read entry */
			struct dirent *dent;
			rewinddir(_fd);
			for (unsigned i = 0; i <= index; ++i) {
				dent = readdir(_fd);
			}

			if (!dent)
				return 0;

			Path dent_path(dent->d_name, _path.base());

			struct stat st { };
			lstat(dent_path.base(), &st);

			auto type = [] (unsigned char type)
			{
				switch (type) {
				case DT_REG: return Node_type::CONTINUOUS_FILE;
				case DT_DIR: return Node_type::DIRECTORY;
				case DT_LNK: return Node_type::SYMLINK;
				default:     return Node_type::CONTINUOUS_FILE;
				}
			};

			Directory_entry &e = *(Directory_entry *)(dst);
			e = {
				.inode = (unsigned long)dent->d_ino,
				.type  = type(dent->d_type),
				.rwx   = { .readable   = (st.st_mode & S_IRUSR),
				           .writeable  = (st.st_mode & S_IWUSR),
				           .executable = (st.st_mode & S_IXUSR) },
				.name  = { dent->d_name }
			};

			return sizeof(Directory_entry);
		}

		size_t write(char const *, size_t, seek_off_t) override
		{
			/* writing to directory nodes is not supported */
			return 0;
		}

		bool sync() override
		{
			int ret = fsync(dirfd(_fd));
			return ret ? false : true;
		}

		Status status() override
		{
			struct stat st { };

			int fd = dirfd(_fd);
			if (fd == -1 || fstat(fd, &st) < 0)
				st.st_mtime = 0;

			return {
				.size  = _num_entries() * sizeof(File_system::Directory_entry),
				.type  = Node_type::DIRECTORY,
				.rwx   = { .readable   = (st.st_mode & S_IRUSR),
				           .writeable  = (st.st_mode & S_IWUSR),
				           .executable = (st.st_mode & S_IXUSR) },
				.inode = inode(),
				.modification_time = { st.st_mtime }
			};
		}
};

#endif /* _DIRECTORY_H_ */
