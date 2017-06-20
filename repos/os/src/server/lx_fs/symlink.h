/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-11-11
 *
 * FIXME unfinished
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
#include <lx_util.h>


namespace File_system {
	class Symlink;
}


class File_system::Symlink : public Node
{
	private:

		char _link_to[MAX_PATH_LEN];

		file_size_t _length() const { return strlen(_link_to) + 1; }

	public:

		Symlink(char const *name) { Node::name(name); }

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			size_t count = min(len, sizeof(_link_to) + 1);
			Genode::strncpy(dst, _link_to, count);
			return count;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			/* Ideal symlink operations are atomic. */
			if (seek_offset) return 0;

			size_t count = min(len, sizeof(_link_to) + 1);
			Genode::strncpy(_link_to, src, count);
			return count;
		}

		Status status() override
		{
			Status s;
			s.inode = inode();
			s.size = _length();
			s.mode = File_system::Status::MODE_SYMLINK;
			return s;
		}
};

#endif /* _SYMLINK_H_ */
