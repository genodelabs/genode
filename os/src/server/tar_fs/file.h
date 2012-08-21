/*
 * \brief  TAR file-system file node
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2012-08-20
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FILE_H_
#define _FILE_H_

/* local includes */
#include <node.h>


namespace File_system {

	class File : public Node
	{
		public:

			File(Record *record) : Node(record) { }

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				bool verbose = false;

				if (verbose)
					PDBG("len = %zu, seek_offset = %llu", len, seek_offset);

				size_t const record_size = _record->size();

				size_t const record_bytes_left = record_size >= seek_offset
				                               ? record_size  - seek_offset : 0;

				size_t const count = min(record_bytes_left, len);

				char const *data = (char *)_record->data() + seek_offset;

				memcpy(dst, data, count);

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

#endif /* _FILE_H_ */
