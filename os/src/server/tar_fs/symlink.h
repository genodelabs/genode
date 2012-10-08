/*
 * \brief  TAR file-system symlink node
 * \author Christian Prochaska
 * \date   2012-08-24
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/* local includes */
#include <node.h>


namespace File_system {

	class Symlink : public Node
	{
		public:

			Symlink(Record *record) : Node(record) { }

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				bool verbose = false;

				if (verbose)
					PDBG("len = %zu, seek_offset = %llu", len, seek_offset);

				size_t count = min(len, (size_t)100);
				memcpy(dst, _record->linked_name(), count);
				return count;
			}

			size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				bool verbose = false;

				if (verbose)
					PDBG("len = %zu, seek_offset = %llu", len, seek_offset);

				return -1;
			}

	};
}

#endif /* _SYMLINK_H_ */
