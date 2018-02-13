/**
 * \brief  Rump symlink node
 * \author Sebastian Sumpf
 * \date   2014-01-20
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/* Genode includes */
#include <file_system/util.h>
#include <os/path.h>

#include "node.h"

namespace Rump_fs {
	class Symlink;
}

class Rump_fs::Symlink : public Node
{
	private:

		typedef Genode::Path<MAX_PATH_LEN> Path;

		Path _path;
		bool _create;

	public:

		Symlink(char const *dir, char const *name, bool create)
		: Node(0), _path(name, dir), _create(create)
		{
			Node::name(name);
		}

		Symlink(char const *path)
		: Node(0), _path(path), _create(false)
		{
			Node::name(basename(path));
		}

		size_t write(char const *src, size_t const len, seek_off_t seek_offset) override
		{
			/* Ideal symlink operations are atomic. */
			if (!_create || seek_offset)
				return 0;

			/* src may not be null-terminated */
			Genode::String<MAX_PATH_LEN> target(Genode::Cstring(src, len));

			int ret = rump_sys_symlink(target.string(), _path.base());
			return ret == -1 ? 0 : len;
		}

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			int ret = rump_sys_readlink(_path.base(), dst, len);
			return ret == -1 ? 0 : ret;
		}

		Status status() override
		{
			Status s;
			s.inode = inode();
			s.size  = length();
			s.mode  = File_system::Status::MODE_SYMLINK;

			return s;
		}

		file_size_t length()
		{
			char link_to[MAX_PATH_LEN];
			return read(link_to, MAX_PATH_LEN, 0);
		}
};

#endif /* _SYMLINK_H_ */
