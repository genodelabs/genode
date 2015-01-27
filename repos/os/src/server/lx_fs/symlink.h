/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2013-11-11
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

		typedef Genode::Path<MAX_PATH_LEN> Path;

		Path _path;
		bool _create;

	public:

		Symlink(char const *dir, char const *name, const bool create)
		: Node(0), _path(name, dir), _create(create)
		{
			Node::name(name);
		}

		Symlink(char const *name, const bool create)
		: Node(0), _path(name), _create(create)
		{
			Node::name(basename(name));
		}

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			int ret = readlink(_path.base(), dst, len);
			return ret == -1 ? 0 : ret;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset)
		{
			if (!_create)
				return 0;

			int ret = symlink(src, _path.base());
			return ret == -1 ? 0 : ret;
		}

		file_size_t length()
		{
			char link_to[MAX_PATH_LEN];
			return read(link_to, MAX_PATH_LEN, 0);
		}
};

#endif /* _SYMLINK_H_ */
