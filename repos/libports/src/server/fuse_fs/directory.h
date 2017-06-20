/*
 * \brief  File-system directory node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Josef Soentgen
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

/* Genode includes */
#include <os/path.h>
#include <util/string.h>

/* libc includes */
#include <sys/dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* local includes */
#include <file.h>
#include <mode_util.h>
#include <node.h>
#include <util.h>
#include <symlink.h>

#include <fuse.h>
#include <fuse_private.h>


namespace Fuse_fs {
	class Directory;
}


class Fuse_fs::Directory : public Node
{
	private:

		typedef Genode::Path<MAX_PATH_LEN> Path;

		struct fuse_file_info  _file_info;
		Path                   _path;
		Allocator             &_alloc;

		/**
		 * Check if the given path points to a directory
		 */
		bool _is_dir(char const *path)
		{
			struct stat s;
			if (Fuse::fuse()->op.getattr(path, &s) != 0 || ! S_ISDIR(s.st_mode))
				return false;

			return true;
		}

		void _open_path(char const *path, bool create)
		{
			int res;

			if (create) {

				res = Fuse::fuse()->op.mkdir(path, 0755);

				if (res < 0) {
					int err = -res;
					switch (err) {
						case EACCES:
							Genode::error("op.mkdir() permission denied");
							throw Permission_denied();
						case EEXIST:
							throw Node_already_exists();
						case EIO:
							Genode::error("op.mkdir() I/O error occurred");
							throw Lookup_failed();
						case ENOENT:
							throw Lookup_failed();
						case ENOTDIR:
							throw Lookup_failed();
						case ENOSPC:
							Genode::error("op.mkdir() error while expanding directory");
							throw Lookup_failed();
						case EROFS:
							throw Permission_denied();
						default:
							Genode::error("op.mkdir() returned unexpected error code: ", res);
							throw Lookup_failed();
					}
				}
			}

			res = Fuse::fuse()->op.opendir(path, &_file_info);

			if (res < 0) {
				int err = -res;
				switch (err) {
					case EACCES:
						Genode::error("op.opendir() permission denied");
						throw Permission_denied();
					case EIO:
						Genode::error("op.opendir() I/O error occurred");
						throw Lookup_failed();
					case ENOENT:
						throw Lookup_failed();
					case ENOTDIR:
						throw Lookup_failed();
					case ENOSPC:
						Genode::error("op.opendir() error while expanding directory");
						throw Lookup_failed();
					case EROFS:
						throw Permission_denied();
					default:
						Genode::error("op.opendir() returned unexpected error code: ", res);
						throw Lookup_failed();
				}
			}
		}

		size_t _num_entries()
		{
			char buf[4096];

			Genode::memset(buf, 0, sizeof (buf));

			struct fuse_dirhandle dh = {
				.filler = Fuse::fuse()->filler,
				.buf    = buf,
				.size   = sizeof (buf),
				.offset = 0,
			};

			int res = Fuse::fuse()->op.readdir(_path.base(), &dh,
					Fuse::fuse()->filler, 0,
					&_file_info);

			if (res != 0)
				return 0;

			return dh.offset / sizeof (struct dirent);
		}

	public:

		Directory(Allocator &alloc, char const *path, bool create)
		:
			Node(path),
			_path(path),
			_alloc(alloc)
		{
			if (!create && !_is_dir(path))
				throw Lookup_failed();

			_open_path(path, create);
		}

		virtual ~Directory()
		{
			Fuse::fuse()->op.release(_path.base(), &_file_info);
		}

		Node *node(char const *path)
		{
			Path node_path(path, _path.base());

			struct stat s;
			int res = Fuse::fuse()->op.getattr(node_path.base(), &s);
			if (res != 0)
				throw Lookup_failed();

			Node *node = 0;

			if (S_ISDIR(s.st_mode))
				node = new (&_alloc) Directory(_alloc, node_path.base(), false);
			else if (S_ISREG(s.st_mode))
				node = new (&_alloc) File(this, path, STAT_ONLY);
			else if (S_ISLNK(s.st_mode))
				node = new (&_alloc) Symlink(this, path, false);
			else
				throw Lookup_failed();

			return node;
		}

		struct fuse_file_info *file_info() { return &_file_info; }

		Status status() override
		{
			struct stat s;
			int res = Fuse::fuse()->op.getattr(_path.base(), &s);
			if (res != 0)
				return Status();

			Status status;
			status.inode = s.st_ino ? s.st_ino : 1;
			status.size  = _num_entries() * sizeof(Directory_entry);
			status.mode  = File_system::Status::MODE_DIRECTORY;

			return status;
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

			char buf[4096];

			Genode::memset(buf, 0, sizeof (buf));
			struct dirent *de = (struct dirent *)buf;

			struct fuse_dirhandle dh = {
				.filler = Fuse::fuse()->filler,
				.buf    = buf,
				.size   = sizeof (buf),
				.offset = 0,
			};

			int res = Fuse::fuse()->op.readdir(_path.base(), &dh,
					Fuse::fuse()->filler, 0,
					&_file_info);
			if (res != 0)
				return 0;

			if (index > (seek_off_t)(dh.offset / sizeof (struct dirent)))
				return 0;

			struct dirent *dent = de + index;
			if (!dent)
				return 0;

			Directory_entry *e = (Directory_entry *)(dst);

			switch (dent->d_type) {
			case DT_REG: e->type = Directory_entry::TYPE_FILE;      break;
			case DT_DIR: e->type = Directory_entry::TYPE_DIRECTORY; break;
			case DT_LNK: e->type = Directory_entry::TYPE_SYMLINK;   break;
			/**
			 * There are FUSE file system implementations that do not fill-out
			 * d_type when calling readdir(). We mark these entries by setting
			 * their type to DT_UNKNOWN in our libfuse implementation. Afterwards
			 * we call getattr() on each entry that, hopefully, will yield proper
			 * results.
			 */
			case DT_UNKNOWN:
			{
				Genode::Path<4096> path(dent->d_name, _path.base());
				struct stat sbuf;
				res = Fuse::fuse()->op.getattr(path.base(), &sbuf);
				if (res == 0) {
					switch (IFTODT(sbuf.st_mode)) {
					case DT_REG: e->type = Directory_entry::TYPE_FILE;      break;
					case DT_DIR: e->type = Directory_entry::TYPE_DIRECTORY; break;
					}
					/* break outer switch */
					break;
				}
			}
			default:
				return 0;
			}

			strncpy(e->name, dent->d_name, sizeof(e->name));

			return sizeof(Directory_entry);
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			/* writing to directory nodes is not supported */
			return 0;
		}
};

#endif /* _DIRECTORY_H_ */
