/*
 * \brief  File-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \author Sid Hussmann
 * \author Pirmin Duss
 * \date   2013-11-11
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>


namespace Lx_fs {

	using namespace File_system;

	enum { MAX_ABSOLUTE_PATH_LEN = 2048 };

	using Absolute_path = Genode::Path<MAX_ABSOLUTE_PATH_LEN>;

	using uint64_t = Genode::uint64_t;

	class Node;
	class File;
}

class Lx_fs::Node : public File_system::Node_base
{
	public:

		using Path = Genode::Path<MAX_PATH_LEN>;
		typedef char Name[128];

	private:

		Name _name;

		uint64_t const _inode;

	public:

		Node(uint64_t inode) : _inode { inode }
		{
			_name[0] = 0;
		}

		uint64_t inode()   const { return _inode; }
		char const *name() const { return _name; }

		virtual bool type_directory() const { return false; }

		/**
		 * Assign name
		 */
		void name(char const *name) { Genode::copy_cstring(_name, name, sizeof(_name)); }

		virtual void update_modification_time(Timestamp const) = 0;

		virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
		virtual size_t write(char const *src, size_t len, seek_off_t) = 0;

		virtual bool sync() { return true; }

		virtual Status status() = 0;

		virtual unsigned num_entries() { return 0; }

		/*
		 * File functionality
		 */
		virtual void truncate(file_size_t)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-file node");
		}

		/*
		 * Directory functionality
		 */
		virtual File *file(char const * /* name */, Mode, bool /* create */)
		{
			Genode::error(__PRETTY_FUNCTION__, " called on a non-directory node");
			return nullptr;
		}

		virtual Path path() const  { return Path { }; }
};

#endif /* _NODE_H_ */
