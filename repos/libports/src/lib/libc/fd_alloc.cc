/*
 * \brief  File-descriptor allocator implementation
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/env.h>
#include <base/log.h>
#include <libc/allocator.h>

/* libc plugin interface */
#include <libc-plugin/fd_alloc.h>

using namespace Libc;
using namespace Genode;


File_descriptor_allocator *Libc::file_descriptor_allocator()
{
	static bool constructed = false;
	static char placeholder[sizeof(File_descriptor_allocator)];
	static Libc::Allocator md_alloc;

	if (!constructed) {
		Genode::construct_at<File_descriptor_allocator>(placeholder, md_alloc);
		constructed = true;
	}

	return reinterpret_cast<File_descriptor_allocator *>(placeholder);
}


File_descriptor_allocator::File_descriptor_allocator(Genode::Allocator &md_alloc)
: Allocator_avl_tpl<File_descriptor>(&md_alloc)
{
	add_range(0, MAX_NUM_FDS);
}


File_descriptor *File_descriptor_allocator::alloc(Plugin *plugin,
                                                  Plugin_context *context,
                                                  int libc_fd)
{
	Lock::Guard guard(_lock);

	/* we use addresses returned by the allocator as file descriptors */
	addr_t addr = (libc_fd <= ANY_FD ? ANY_FD : libc_fd);

	/* allocate fresh fd if the default value for 'libc_fd' was specified */
	bool alloc_ok = false;
	if (libc_fd <= ANY_FD)
		alloc_ok = Allocator_avl_base::alloc(1, reinterpret_cast<void**>(&addr));
	else
		alloc_ok = (Allocator_avl_base::alloc_addr(1, addr).ok());

	if (!alloc_ok) {
		error("could not allocate libc_fd ", libc_fd,
		      libc_fd <= ANY_FD ? " (any)" : "");
		return 0;
	}

	File_descriptor *fdo = metadata((void*)addr);
	fdo->libc_fd = (int)addr;
	fdo->fd_path = 0;
	fdo->plugin  = plugin;
	fdo->context = context;
	fdo->lock    = Lock(Lock::UNLOCKED);
	return fdo;
}


void File_descriptor_allocator::free(File_descriptor *fdo)
{
	Lock::Guard guard(_lock);
	::free((void *)fdo->fd_path);
	Allocator_avl_base::free(reinterpret_cast<void*>(fdo->libc_fd));
}


File_descriptor *File_descriptor_allocator::find_by_libc_fd(int libc_fd)
{
	Lock::Guard guard(_lock);
	return metadata(reinterpret_cast<void*>(libc_fd));
}


/********************
 ** Libc functions **
 ********************/

extern "C" int __attribute__((weak)) getdtablesize(void) { return MAX_NUM_FDS; }
