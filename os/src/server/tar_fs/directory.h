/*
 * \brief  TAR file-system directory node
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

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

/* local includes */
#include <lookup.h>
#include <node.h>


namespace File_system {

	class Directory : public Node
	{
		public:

			Directory(Record *record) : Node(record) { }

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				bool verbose = false;

				if (verbose)
					PDBG("len = %zu, seek_offset = %llu", len, seek_offset);

				if (len < sizeof(Directory_entry)) {
					PERR("read buffer too small for directory entry");
					return 0;
				}

				if (seek_offset % sizeof(Directory_entry)) {
					PERR("seek offset not alighed to sizeof(Directory_entry)");
					return 0;
				}

				int64_t index = seek_offset / sizeof(Directory_entry);

				Lookup_member_of_path lookup_criterion(_record->name(), index);
				Record *record = _lookup(&lookup_criterion);
				if (!record)
					return 0;

				Absolute_path absolute_path(record->name());
				absolute_path.keep_only_last_element();
				absolute_path.remove_trailing('/');

				Directory_entry *e = (Directory_entry *)(dst);

				strncpy(e->name, absolute_path.base(), sizeof(e->name));

				switch (record->type()) {
					case Record::TYPE_DIR:     e->type = Directory_entry::TYPE_DIRECTORY; break;
					case Record::TYPE_FILE:    e->type = Directory_entry::TYPE_FILE; break;
					case Record::TYPE_SYMLINK: e->type = Directory_entry::TYPE_SYMLINK; break;
 					default:
						if (verbose)
							PDBG("unhandled record type %d", record->type());
				}

				if (verbose)
					PDBG("found dir entry: %s", e->name);

				return sizeof(Directory_entry);
			}

			size_t write(char const *src, size_t len, seek_off_t)
			{
				/* writing to directory nodes is not supported */
				return 0;
			}

	};
}

#endif /* _DIRECTORY_H_ */
