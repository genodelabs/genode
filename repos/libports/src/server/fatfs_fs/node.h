/*
 * \brief  FATFS file-system node
 * \author Christian Prochaska
 * \date   2012-07-04
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <base/log.h>
#include <os/path.h>

/* fatfs includes */
namespace Fatfs { extern "C" {
#include <fatfs/ff.h>
} }

namespace Fatfs_fs {
	using namespace File_system;

	typedef Genode::Path<FF_MAX_LFN + 1> Absolute_path;

	class Node;
}


class Fatfs_fs::Node : public Node_base
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
			Genode::error("read() called on generic Node object");
			return 0;
		}

		virtual size_t write(char const *src, size_t len, seek_off_t)
		{
			Genode::error("write() called on generic Node object");
			return 0;
		}

		/*
		 * File functionality
		 */
		virtual void truncate(file_size_t size)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-file node");
		}
};

#endif /* _NODE_H_ */
