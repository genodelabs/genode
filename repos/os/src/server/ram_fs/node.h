/*
 * \brief  File-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/listener.h>
#include <file_system/node.h>
#include <util/list.h>

namespace File_system {

	class Node : public Node_base, public List<Node>::Element
	{
		public:

			typedef char Name[128];

		private:

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

			unsigned long inode() const { return _inode; }
			char   const *name()  const { return _name; }

			/**
			 * Assign name
			 */
			void name(char const *name) { strncpy(_name, name, sizeof(_name)); }

			virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
			virtual size_t write(char const *src, size_t len, seek_off_t) = 0;
	};

}

#endif /* _NODE_H_ */
