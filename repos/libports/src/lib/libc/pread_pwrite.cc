/*
 * \brief  'pread()' and 'pwrite()' implementations
 * \author Christian Prochaska
 * \date   2012-07-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/lock.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

/* libc-internal includes */
#include <internal/types.h>


struct Read
{
	ssize_t operator()(int fd, void *buf, size_t count)
	{
		return read(fd, buf, count);
	}
};


struct Write
{
	ssize_t operator()(int fd, const void *buf, size_t count)
	{
		return write(fd, buf, count);
	}
};


using namespace Libc;


template <typename Rw_func, typename Buf_type>
static ssize_t pread_pwrite_impl(Rw_func rw_func, int fd, Buf_type buf, ::size_t count, ::off_t offset)
{
	File_descriptor *fdesc = file_descriptor_allocator()->find_by_libc_fd(fd);
	if (fdesc == 0)
		return -1;

	Lock_guard<Lock> rw_lock_guard(fdesc->lock);

	::off_t old_offset = lseek(fd, 0, SEEK_CUR);

	if (old_offset == -1)
		return -1;

	if (lseek(fd, offset, SEEK_SET) == -1)
		return -1;

	ssize_t result = rw_func(fd, buf, count);

	if (lseek(fd, old_offset, SEEK_SET) == -1)
		return -1;

	return result;
}


extern "C" ssize_t pread(int fd, void *buf, ::size_t count, ::off_t offset)
{
	return pread_pwrite_impl(Read(), fd, buf, count, offset);
}

extern "C" __attribute__((alias("pread")))
ssize_t __sys_pread(int fd, void *buf, ::size_t count, ::off_t offset);


extern "C" ssize_t pwrite(int fd, const void *buf, ::size_t count, ::off_t offset)
{
	return pread_pwrite_impl(Write(), fd, buf, count, offset);
}

extern "C" __attribute__((alias("pwrite")))
ssize_t __sys_pwrite(int fd, const void *buf, ::size_t count, ::off_t offset);
