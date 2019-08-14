/*
 * \brief  file descriptor allocator interface
 * \author Christian Prochaska 
 * \date   2010-01-21
 *
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_PLUGIN__FD_ALLOC_H_
#define _LIBC_PLUGIN__FD_ALLOC_H_

#include <base/lock.h>
#include <base/log.h>
#include <os/path.h>
#include <base/allocator.h>
#include <base/id_space.h>
#include <util/xml_generator.h>

/* libc includes */
#include <stdlib.h>
#include <string.h>

#include <libc-plugin/plugin.h>

enum { MAX_NUM_FDS = 1024 };

namespace Libc {

	/**
	 * Plugin-specific file-descriptor context
	 */
	struct Plugin_context { virtual ~Plugin_context() { } };

	enum { ANY_FD = -1 };

	struct File_descriptor
	{
		Genode::Lock lock { };

		typedef Genode::Id_space<File_descriptor> Id_space;
		Id_space::Element _elem;

		int const libc_fd = _elem.id().value;

		char const *fd_path = nullptr;  /* for 'fchdir', 'fstat' */

		Plugin         *plugin;
		Plugin_context *context;

		int  flags   = 0;  /* for 'fcntl' */
		bool cloexec = 0;  /* for 'fcntl' */

		File_descriptor(Id_space &id_space, Plugin &plugin, Plugin_context &context)
		: _elem(*this, id_space), plugin(&plugin), context(&context) { }

		File_descriptor(Id_space &id_space, Plugin &plugin, Plugin_context &context,
		                Id_space::Id id)
		: _elem(*this, id_space, id), plugin(&plugin), context(&context) { }

		void path(char const *newpath)
		{
			if (fd_path) { Genode::warning("may leak former FD path memory"); }
			if (newpath) {
				Genode::size_t const path_size = ::strlen(newpath) + 1;
				char *buf = (char*)malloc(path_size);
				if (!buf) {
					Genode::error("could not allocate path buffer for libc_fd ", libc_fd);
					return;
				}
				::memcpy(buf, newpath, path_size);
				fd_path = buf;
			} else
				fd_path = 0;
		}
	};


	class File_descriptor_allocator
	{
		private:

			Genode::Lock _lock;

			Genode::Allocator &_alloc;

			typedef File_descriptor::Id_space Id_space;

			Id_space _id_space;

		public:

			/**
			 * Constructor
			 */
			File_descriptor_allocator(Genode::Allocator &_alloc);

			/**
			 * Allocate file descriptor
			 */
			File_descriptor *alloc(Plugin *plugin, Plugin_context *context, int libc_fd = -1);

			/**
			 * Release file descriptor
			 */
			void free(File_descriptor *fdo);

			/**
			 * Prevent the use of the specified file descriptor
			 */
			void preserve(int libc_fd);

			File_descriptor *find_by_libc_fd(int libc_fd);

			void generate_info(Genode::Xml_generator &);
	};


	/**
	 * Return singleton instance of file-descriptor allocator
	 */
	extern File_descriptor_allocator *file_descriptor_allocator();
}

#endif /* _LIBC_PLUGIN__FD_ALLOC_H_ */
