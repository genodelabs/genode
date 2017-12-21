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

#ifndef _INCLUDE__RAM_FS__NODE_H_
#define _INCLUDE__RAM_FS__NODE_H_

/* Genode includes */
#include <file_system/listener.h>
#include <file_system/node.h>
#include <util/list.h>

namespace Ram_fs {
	using namespace Genode;
	using File_system::seek_off_t;
	using File_system::Status;
	class Node;
	class File;
	class Symlink;
}


class Ram_fs::Node : public  File_system::Node_base,
                     private Weak_object<Node>,
                     private List<Node>::Element
{
	public:

		typedef char Name[128];

		using List<Node>::Element::next;
		using Weak_object<Node>::weak_ptr;

	private:

		friend class List<Node>;
		friend class Locked_ptr<Node>;

		int                 _ref_count;
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
		: _ref_count(0), _inode(_unique_inode())
		{ _name[0] = 0; }

		virtual ~Node() { lock_for_destruction(); }

		unsigned long inode() const { return _inode; }
		char   const *name()  const { return _name; }

		/**
		 * Assign name
		 */
		void name(char const *name) { strncpy(_name, name, sizeof(_name)); }

		virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
		virtual size_t write(char const *src, size_t len, seek_off_t) = 0;

		virtual Status status() = 0;


		/* File functionality */

		virtual void truncate(File_system::file_size_t)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-file node");
		}


		/* Directory functionality  */

		virtual bool has_sub_node_unsynchronized(char const *) const
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return false;
		}

		virtual void adopt_unsynchronized(Node *)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
		}

		virtual File *lookup_file(char const *)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return nullptr;
		}

		virtual Symlink *lookup_symlink(char const *)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return nullptr;
		}

		virtual Node *lookup(char const *, bool = false)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return nullptr;
		}

		virtual void discard(Node *)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
		}


};

#endif /* _INCLUDE__RAM_FS__NODE_H_ */
