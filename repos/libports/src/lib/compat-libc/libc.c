/*
 * \brief  Interface real libc
 * \author Sebastian Sumpf
 * \date   2023-06-12
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/stat.h>
#include <dlfcn.h>

static void* libc_handle(void)
{
	static void *handle = NULL;
	if (!handle) handle = dlopen("libc.lib.so", RTLD_LAZY);
	return handle;
}


int (*_stat)(char const*, struct stat*);
int (*_fstat)(int, struct stat *);

int libc_stat(const char *path, struct stat *buf)
{
	if (!_stat)
		_stat = dlsym(libc_handle(), "stat");

	return _stat(path, buf);
}


int libc_fstat(int fd, struct stat *buf)
{
	if (!_fstat)
		_fstat = dlsym(libc_handle(), "fstat");

	return _fstat(fd, buf);
}
