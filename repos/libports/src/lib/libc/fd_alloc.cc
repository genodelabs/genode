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


File_descriptor_allocator::File_descriptor_allocator(Genode::Allocator &alloc)
: _alloc(alloc)
{ }


File_descriptor *File_descriptor_allocator::alloc(Plugin *plugin,
                                                  Plugin_context *context,
                                                  int libc_fd)
{
	Lock::Guard guard(_lock);

	bool const any_fd = (libc_fd < 0);
	if (any_fd)
		return new (_alloc) File_descriptor(_id_space, *plugin, *context);

	Id_space::Id const id {(unsigned)libc_fd};
	return new (_alloc) File_descriptor(_id_space, *plugin, *context, id);
}


void File_descriptor_allocator::free(File_descriptor *fdo)
{
	Lock::Guard guard(_lock);

	::free((void *)fdo->fd_path);

	Genode::destroy(_alloc, fdo);
}


void File_descriptor_allocator::preserve(int fd)
{
	if (!find_by_libc_fd(fd))
		alloc(nullptr, nullptr, fd);
}


File_descriptor *File_descriptor_allocator::find_by_libc_fd(int libc_fd)
{
	Lock::Guard guard(_lock);

	if (libc_fd < 0)
		return nullptr;

	File_descriptor *result = nullptr;

	try {
		Id_space::Id const id {(unsigned)libc_fd};
		_id_space.apply<File_descriptor>(id, [&] (File_descriptor &fd) {
			result = &fd; });
	} catch (Id_space::Unknown_id) { }

	return result;
}


/********************
 ** Libc functions **
 ********************/

extern "C" int __attribute__((weak)) getdtablesize(void) { return MAX_NUM_FDS; }
