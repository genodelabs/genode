/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RAM_FS__SYMLINK_H_
#define _INCLUDE__RAM_FS__SYMLINK_H_

/* local includes */
#include <ram_fs/node.h>

namespace File_system { class Symlink; }


class File_system::Symlink : public Node
{
	private:

		char   _link_to[MAX_PATH_LEN];
		size_t _len;

	public:

		Symlink(char const *name): _len(0) { Node::name(name); }

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			size_t count = min(len, _len-seek_offset);
			Genode::memcpy(dst, _link_to+seek_offset, count);
			return count;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset)
		{
			/* Ideal symlink operations are atomic. */
			if (seek_offset) return 0;

			_len = min(len, sizeof(_link_to));
			Genode::memcpy(_link_to, src, _len);
			return _len;
		}

		file_size_t length() const { return _len; }
};

#endif /* _INCLUDE__RAM_FS__SYMLINK_H_ */
