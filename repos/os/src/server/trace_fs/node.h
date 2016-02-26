/*
 * \brief  File-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>

namespace File_system {

	using namespace Genode;

	class Node : public Node_base, public List<Node>::Element
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

			unsigned long inode() const { return _inode; }
			char   const *name()  const { return _name; }

			/**
			 * Assign name
			 */
			void name(char const *name) { strncpy(_name, name, sizeof(_name)); }

			virtual Status status() const = 0;

			virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
			virtual size_t write(char const *src, size_t len, seek_off_t) = 0;
	};
}

#endif /* _NODE_H_ */
