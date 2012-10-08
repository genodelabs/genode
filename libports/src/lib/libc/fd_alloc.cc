/*
 * \brief  File-descriptor allocator implementation
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>

/* libc plugin interface */
#include <libc-plugin/fd_alloc.h>

namespace Libc {

	File_descriptor_allocator *file_descriptor_allocator()
	{
		static File_descriptor_allocator _file_descriptor_allocator;
		return &_file_descriptor_allocator;
	}
}


using namespace Libc;
using namespace Genode;


File_descriptor_allocator::File_descriptor_allocator()
: Allocator_avl_tpl<File_descriptor>(Genode::env()->heap())
{
	add_range(0, MAX_NUM_FDS);
}


File_descriptor *File_descriptor_allocator::alloc(Plugin *plugin,
                                                  Plugin_context *context,
                                                  int libc_fd)
{
	/* we use addresses returned by the allocator as file descriptors */
	addr_t addr = (libc_fd == ANY_FD ? ANY_FD : libc_fd);

	/* allocate fresh fd if the default value for 'libc_fd' was specified */
	bool alloc_ok = false;
	if (addr == ANY_FD) 
		alloc_ok = Allocator_avl_base::alloc(1, reinterpret_cast<void**>(&addr));
	else
		alloc_ok = (Allocator_avl_base::alloc_addr(1, addr) == ALLOC_OK);

	if (!alloc_ok) {
		PERR("could not allocate libc_fd %d%s",
		     libc_fd, libc_fd == ANY_FD ? " (any)" : "");
		return 0;
	}

	File_descriptor *fdo = metadata((void*)addr);
	fdo->libc_fd = (int)addr;
	fdo->fd_path = 0;
	fdo->plugin  = plugin;
	fdo->context = context;
	return fdo;
}


void File_descriptor_allocator::free(File_descriptor *fdo)
{
	::free(fdo->fd_path);
	Allocator_avl_base::free(reinterpret_cast<void*>(fdo->libc_fd));
}


File_descriptor *File_descriptor_allocator::find_by_libc_fd(int libc_fd)
{
	return metadata(reinterpret_cast<void*>(libc_fd));
}
