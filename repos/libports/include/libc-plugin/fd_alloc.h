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

/* Genode includes */
#include <base/mutex.h>
#include <base/log.h>
#include <os/path.h>
#include <base/allocator.h>
#include <base/id_space.h>
#include <util/bit_allocator.h>
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
		Genode::Mutex mutex { };

		typedef Genode::Id_space<File_descriptor> Id_space;
		Id_space::Element _elem;

		int const libc_fd = _elem.id().value;

		char const *fd_path = nullptr;  /* for 'fchdir', 'fstat' */

		Plugin         *plugin;
		Plugin_context *context;

		int  flags    = 0;  /* for 'fcntl' */
		bool cloexec  = 0;  /* for 'fcntl' */
		bool modified = false;

		File_descriptor(Id_space &id_space, Plugin &plugin, Plugin_context &context,
		                Id_space::Id id)
		: _elem(*this, id_space, id), plugin(&plugin), context(&context) { }

		void path(char const *newpath);
	};


	class File_descriptor_allocator
	{
		private:

			Genode::Mutex _mutex;

			Genode::Allocator &_alloc;

			typedef File_descriptor::Id_space Id_space;

			Id_space _id_space;

			Genode::Bit_allocator<MAX_NUM_FDS> _id_allocator;

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

			/**
			 * Return any file descriptor with close-on-execve flag set
			 *
			 * \return pointer to file descriptor, or
			 *         nullptr is no such file descriptor exists
			 */
			File_descriptor *any_cloexec_libc_fd();

			/**
			 * Return file-descriptor ID of any open file, or -1 if no file is
			 * open
			 */
			int any_open_fd();

			void generate_info(Genode::Xml_generator &);
	};


	/**
	 * Return singleton instance of file-descriptor allocator
	 */
	extern File_descriptor_allocator *file_descriptor_allocator();
}

#endif /* _LIBC_PLUGIN__FD_ALLOC_H_ */
