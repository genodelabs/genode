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
#include "node.h"

namespace Ram_fs { class Symlink; }


class Ram_fs::Symlink : public Node
{
	private:

		char   _link_to[File_system::MAX_PATH_LEN];
		size_t _len;

	public:

		Symlink(char const *name): _len(0) { Node::name(name); }

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			size_t count = min(len, _len-seek_offset);
			Genode::memcpy(dst, _link_to+seek_offset, count);
			return count;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			/* Ideal symlink operations are atomic. */
			if (seek_offset) return 0;

			for (size_t i = 0; i < len; ++i) {
				if (src[i] == '\0') {
					len = i;
					break;
				}
			}

			/*
			 * if the target is too long return a
			 * short result to indicate the error
			 */
			if (len > sizeof(_link_to))
				return len >> 1;

			Genode::memcpy(_link_to, src, len);
			_len = len;
			return len;
		}

		Status status() override
		{
			Status s;
			s.inode = inode();
			s.size = _len;
			s.mode = File_system::Status::MODE_SYMLINK;
			return s;
		}
};

#endif /* _INCLUDE__RAM_FS__SYMLINK_H_ */
