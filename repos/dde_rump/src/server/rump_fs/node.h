/*
 * \brief  File-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
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
#include <base/signal.h>


namespace Rump_fs {
	using namespace File_system;
	using namespace Genode;
	using Genode::size_t;
	class Node;
	class File;
	class Directory;
	class Symlink;
}

class Rump_fs::Node : public Node_base
{
	public:

		typedef char Name[128];

	private:

		Name                _name;
		unsigned long const _inode;

	public:

		Node(unsigned long inode) : _inode(inode) { _name[0] = 0; }

		unsigned long inode() const { return _inode; }
		char   const *name()  const { return _name; }

		/**
		 * Assign name
		 */
		void name(char const *name) { strncpy(_name, name, sizeof(_name)); }

		virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
		virtual size_t write(char const *src, size_t len, seek_off_t) = 0;
		virtual Status status() = 0;

		/*
		 * Directory functionality
		 */
		virtual int fd() const
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return -1;
		}

		virtual	File *file(char const *name, Mode mode, bool create)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return nullptr;
		}

		virtual Symlink *symlink(char const *name, bool create)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return nullptr;
		}

		virtual void unlink(char const *path)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
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
