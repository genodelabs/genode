/*
 * \brief  TAR file-system node
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2012-08-20
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>

/* local includes */
#include <record.h>

namespace File_system {

	class Node : public Node_base
	{
		protected:

			Record *_record;

		public:

			Node(Record *record) : _record(record) { }

			Record const *record() const { return _record; }

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
