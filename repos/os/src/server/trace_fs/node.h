/*
 * \brief  File-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>

namespace Trace_fs {
	using namespace File_system;
	using namespace Genode;
	class Node;
}

class Trace_fs::Node : public Node_base, public Weak_object<Node>,
                       public List<Node>::Element
{
	public:

		typedef char Name[128];

	private:

		Name                _name;
		unsigned long const _inode;

		/**
		 * Generate unique inode number
		 */
		static unsigned long _unique_inode()
		{
			static unsigned long inode_count;
			return ++inode_count;
		}

	public:

		Node()
		: _inode(_unique_inode())
		{ _name[0] = 0; }

		virtual ~Node() { lock_for_destruction(); }

		unsigned long inode() const { return _inode; }
		char   const *name()  const { return _name; }

		/**
		 * Assign name
		 */
		void name(char const *name) { strncpy(_name, name, sizeof(_name)); }

		virtual Status status() const = 0;

		virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
		virtual size_t write(char const *src, size_t len, seek_off_t) = 0;

		/*
		 * Directory functionality
		 */
		virtual Node *lookup(char const *path, bool return_parent = false)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return nullptr;
		}

		/*
		 * File functionality
		 */
		virtual void truncate(file_size_t size)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-file node");
		}
};

#endif /* _NODE_H_ */
