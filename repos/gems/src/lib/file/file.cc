/*
 * \brief  Utility for loading a file
 * \author Norman Feske
 * \date   2014-08-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

/* gems includes */
#include <gems/file.h>

/* libc includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


static Genode::size_t file_size(char const *name)
{
	struct stat s;
	s.st_size = 0;
	stat(name, &s);
	return s.st_size;
}


File::File(char const *name, Genode::Allocator &alloc)
:
	_alloc(alloc),
	_file_size(file_size(name)),
	_data(alloc.alloc(_file_size))
{
	int const fd = open(name, O_RDONLY);
	if (read(fd, _data, _file_size) < 0) {
		Genode::error("reading from file \"", name, "\" failed (error ", errno, ")");
		throw Reading_failed();
	}
	close(fd);
}


File::~File()
{
	_alloc.free(_data, _file_size);
}
