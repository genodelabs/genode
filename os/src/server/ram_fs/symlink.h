/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/* local includes */
#include <node.h>

namespace File_system {

	class Symlink : public Node
	{
		private:

			Name _link_to;

		public:

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				PDBG("not implemented");
				return 0;
			}

			size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				PDBG("not implemented");
				return 0;
			}
	};
}

#endif /* _SYMLINK_H_ */
