/*
 * \brief  File-system directory node
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-11-11
 */

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

/* Genode include */
#include <os/path.h>

/* local includes */
#include <node.h>
#include <util.h>
#include <file.h>
#include <symlink.h>

#include <lx_util.h>


namespace File_system {
	class Directory;
}


class File_system::Directory : public Node
{
	private:

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
				if (ret == -1)
					throw No_space();
			}

			struct stat s;

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

		File * file(char const *name, Mode mode, bool create)
		{
			File *file = new (&_alloc) File(dirfd(_fd), name, mode, create);

			file->lock();
			return file;
		}

		Symlink * symlink(char const *name, bool create)
		{
			Symlink *link = new (&_alloc) Symlink(_path.base(), name, create);

			link->lock();
			return link;
		}

		Directory * subdir(char const *path, bool create)
		{
			Path dir_path(path, _path.base());

			Directory *dir = new (&_alloc) Directory(_alloc, dir_path.base(), create);

			dir->lock();
			return dir;
		}

		Node * node(char const *path)
		{
			Path node_path(path, _path.base());

			struct stat s;
			int ret = lstat(node_path.base(), &s);
			if (ret == -1)
				throw Lookup_failed();

			Node *node = 0;

			if (S_ISDIR(s.st_mode))
				node = new (&_alloc) Directory(_alloc, node_path.base(), false);
			else if (S_ISREG(s.st_mode))
				node = new (&_alloc) File(node_path.base(), STAT_ONLY);
			else if (S_ISLNK(s.st_mode))
				node = new (&_alloc) Symlink(node_path.base(), false);
			else
				throw Lookup_failed();

			node->lock();
			return node;
		}

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			if (len < sizeof(Directory_entry)) {
				PERR("read buffer too small for directory entry");
				return 0;
			}

			if (seek_offset % sizeof(Directory_entry)) {
				PERR("seek offset not aligned to sizeof(Directory_entry)");
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

			Directory_entry *e = (Directory_entry *)(dst);

			switch (dent->d_type) {
			case DT_REG: e->type = Directory_entry::TYPE_FILE;      break;
			case DT_DIR: e->type = Directory_entry::TYPE_DIRECTORY; break;
			case DT_LNK: e->type = Directory_entry::TYPE_SYMLINK;   break;
			default:
				return 0;
			}

			strncpy(e->name, dent->d_name, sizeof(e->name));

			return sizeof(Directory_entry);
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset)
		{
			/* writing to directory nodes is not supported */
			return 0;
		}

		size_t num_entries() const
		{
			unsigned num = 0;

			rewinddir(_fd);
			while (readdir(_fd)) ++num;

			return num;
		}

		void unlink(char const *path)
		{
			Path node_path(path, _path.base());

			struct stat s;
			int ret = lstat(node_path.base(), &s);
			if (ret == -1)
				throw Lookup_failed();

			if (S_ISDIR(s.st_mode))
				ret = rmdir(node_path.base());
			else if (S_ISREG(s.st_mode) || S_ISLNK(s.st_mode))
				ret = ::unlink(node_path.base());
			else
				throw Lookup_failed();

			if (ret == -1) {
				if (errno == EACCES)
					throw Permission_denied();
				PERR("Error during unlink of %s", node_path.base());
			}
		};
};

#endif /* _DIRECTORY_H_ */
