/*
 * \brief  File-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \date   2013-11-11
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>
#include <os/path.h>


namespace File_system {

	typedef Genode::Path<MAX_PATH_LEN> Absolute_path;

	class Node : public Node_base, public List<Node>::Element
	{
		protected:

			unsigned long _inode;
			Absolute_path _name;

		public:

			Node(char const *name) : _name(name) { }

			char   const *name()  const { return _name.base(); }

			virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
			virtual size_t write(char const *src, size_t len, seek_off_t) = 0;
	};

}

#endif /* _NODE_H_ */
