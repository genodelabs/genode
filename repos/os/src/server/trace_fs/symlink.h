/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/* local includes */
#include <file.h>

namespace Trace_fs {
	class Symlink;
}

class Trace_fs::Symlink : public File
{
	public:

		Symlink(char const *name) : File(name) { }

		size_t read(char *dst, size_t len, seek_off_t seek_offset) {
			return 0; }

		size_t write(char const *src, size_t len, seek_off_t seek_offset) {
			return 0; }

		file_size_t length() const { return 0; }

		void truncate(file_size_t) { }
};

#endif /* _SYMLINK_H_ */
