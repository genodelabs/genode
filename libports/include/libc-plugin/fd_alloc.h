/*
 * \brief  file descriptor allocator interface
 * \author Christian Prochaska 
 * \date   2010-01-21
 *
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_PLUGIN__FD_ALLOC_H_
#define _LIBC_PLUGIN__FD_ALLOC_H_

#include <base/allocator_avl.h>
#include <base/printf.h>
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
	class Plugin_context { };

	enum { ANY_FD = -1 };

	struct File_descriptor
	{
		int             libc_fd;
		char           *fd_path;    /* for 'fchdir()' */
		Plugin         *plugin;
		Plugin_context *context;

		void path(char const *newpath)
		{
			if (newpath) {
				size_t path_size = ::strlen(newpath) + 1;
				fd_path = (char*)malloc(path_size);
				if (!fd_path) {
					PERR("could not allocate path buffer for libc_fd %d%s",
					     libc_fd, libc_fd == ANY_FD ? " (any)" : "");
					return;
				}
				::memcpy(fd_path, newpath, path_size);
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
			File_descriptor_allocator();

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
