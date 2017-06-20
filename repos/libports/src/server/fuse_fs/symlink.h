/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \date   2013-11-26
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/* local includes */
#include <node.h>


namespace Fuse_fs {
	class Symlink;
}


class Fuse_fs::Symlink : public Node
{
	private:

		Node *_parent;
		typedef Genode::Path<MAX_PATH_LEN> Path;
		Path                   _path;

		size_t _length() const
		{
			struct stat s;
			int res = Fuse::fuse()->op.getattr(_path.base(), &s);
			if (res != 0)
				return 0;

			return s.st_size;
		}

	public:

		Symlink(Node *parent, char const *name, bool create = false)
		:
			Node(name),
			_parent(parent),
			_path(name, parent->name())
		{ }

		Status status() override
		{
			struct stat s;
			int res = Fuse::fuse()->op.getattr(_path.base(), &s);
			if (res != 0)
				return Status();

			Status status;
			status.inode = s.st_ino ? s.st_ino : 1;
			status.size = s.st_size;
			status.mode = File_system::Status::MODE_FILE;
			return status;
		}

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			int res = Fuse::fuse()->op.readlink(_path.base(), dst, len);
			if (res != 0)
				return 0;

			return Genode::strlen(dst);
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			/* Ideal symlink operations are atomic. */
			if (seek_offset) return 0;

			int res = Fuse::fuse()->op.symlink(src, _path.base());
			if (res != 0)
				return 0;

			return len;
		}

		file_size_t length() const { return _length(); }
};

#endif /* _SYMLINK_H_ */
