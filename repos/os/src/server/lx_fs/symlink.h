/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-11-11
 *
 * FIXME unfinished
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

	public:

		Symlink(char const *name) { Node::name(name); }

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			size_t count = min(len, sizeof(_link_to) + 1);
			Genode::strncpy(dst, _link_to, count);
			return count;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset)
		{
			size_t count = min(len, sizeof(_link_to) + 1);
			Genode::strncpy(_link_to, src, count);
			return count;
		}

		file_size_t length() const { return strlen(_link_to) + 1; }
};

#endif /* _SYMLINK_H_ */
