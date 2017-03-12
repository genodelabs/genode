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

#include <base/allocator_avl.h>
#include <base/lock.h>
#include <base/log.h>
#include <os/path.h>

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
		int             libc_fd = -1;
		char     const *fd_path = 0;  /* for 'fchdir', 'fstat' */
		Plugin         *plugin  = 0;
		Plugin_context *context = 0;
		int             flags   = 0;  /* for 'fcntl' */
		bool            cloexec = 0;  /* for 'fcntl' */
		Genode::Lock    lock;

		void path(char const *newpath)
		{
			if (fd_path) { Genode::warning("may leak former FD path memory"); }
			if (newpath) {
				Genode::size_t const path_size = ::strlen(newpath) + 1;
				char *buf = (char*)malloc(path_size);
				if (!buf) {
					Genode::error("could not allocate path buffer for libc_fd ",
					              libc_fd, libc_fd == ANY_FD ? " (any)" : "");
					return;
				}
				::memcpy(buf, newpath, path_size);
				fd_path = buf;
			} else
				fd_path = 0;
		}
	};


	class File_descriptor_allocator : Allocator_avl_tpl<File_descriptor>
	{
		public:

			/**
			 * Constructor
			 */
			File_descriptor_allocator(Genode::Allocator &md_alloc);

			/**
			 * Allocate file descriptor
			 */
			File_descriptor *alloc(Plugin *plugin, Plugin_context *context, int libc_fd = -1);

			/**
			 * Release file descriptor
			 */
			void free(File_descriptor *fdo);

			File_descriptor *find_by_libc_fd(int libc_fd);
	};


	/**
	 * Return singleton instance of file-descriptor allocator
	 */
	extern File_descriptor_allocator *file_descriptor_allocator();
}

#endif /* _LIBC_PLUGIN__FD_ALLOC_H_ */
