/*
 * \brief  File-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2013-11-11
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <util/list.h>
#include <base/signal.h>


namespace File_system {
	class Node;
}

class File_system::Node : public Node_base, public List<Node>::Element
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
};

#endif /* _NODE_H_ */
