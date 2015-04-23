/*
 * \brief  FFAT file-system node
 * \author Christian Prochaska
 * \date   2012-07-04
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <os/path.h>

/* ffat includes */
namespace Ffat { extern "C" {
#include <ffat/ff.h>
} }

namespace File_system {

	typedef Genode::Path<_MAX_LFN + 1> Absolute_path;

	class Node : public Node_base
	{
		protected:

			Absolute_path _name;

		public:

			Node(const char *name) : _name(name) { }

			char const *name() { return _name.base(); }

			/*
			 * A generic Node object can be created to represent a file or
			 * directory by its name without opening it, so the functions
			 * of this class must not be abstract.
			 */

			virtual size_t read(char *dst, size_t len, seek_off_t)
			{
				PERR("read() called on generic Node object");
				return 0;
			}

			virtual size_t write(char const *src, size_t len, seek_off_t)
			{
				PERR("write() called on generic Node object");
				return 0;
			}
	};

}

#endif /* _NODE_H_ */
