/*
 * \brief  File-system node
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

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <base/lock.h>
#include <base/signal.h>
#include <os/path.h>


namespace Fuse_fs {

	using namespace Genode;
	using namespace File_system;

	typedef Genode::Path<MAX_PATH_LEN> Absolute_path;

	class Node;
}

class Fuse_fs::Node : public Node_base
{
	protected:

		unsigned long _inode;
		Absolute_path _name;

	public:

		Node(char const *name) : _name(name) { }

		char   const *name()  const { return _name.base(); }

		virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
		virtual size_t write(char const *src, size_t len, seek_off_t) = 0;
		virtual Status status() = 0;

		/*
		 * File functionality
		 */
		virtual void truncate(file_size_t size)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-file node");
		}
};

#endif /* _NODE_H_ */
