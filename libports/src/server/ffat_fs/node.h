/*
 * \brief  FFAT file-system node
 * \author Christian Prochaska
 * \date   2012-07-04
 */

#ifndef _NODE_H_
#define _NODE_H_

/* ffat includes */
namespace Ffat { extern "C" {
#include <ffat/ff.h>
} }

namespace File_system {

	class Node
	{
		protected:

			char _name[_MAX_LFN + 1];

		public:

			Node(const char *name)
			{
				strncpy(_name, name, sizeof(_name));

				/* remove any trailing slashes, except for "/" */
				size_t index = strlen(_name) - 1;
				while ((index > 0) && (_name[index] == '/'))
					_name[index--] = 0;
			}

			char const *name() const { return _name; }

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
