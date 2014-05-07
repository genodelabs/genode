/*
 * \brief  DL interface to the dynamic linker (since we don't rely on libc)
 *
 *         These "weak" function should never be reached, because they are
 *         intercepted by ouer dynamic linker, if you see an error message than
 *         you program might not be a dynamic one.
 *
 * \author Sebastian Sumpf
 * \date   2013-13-13
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _DL_INTERFACE_H_
#define _DL_INTERFACE_H_

#include <base/printf.h>

extern "C" {

/**
 * dlopen
 */
enum Open_mode { RTLD_LAZY = 1, RTLD_NOW = 2 };

void  __attribute__((weak))
*dlopen(const char *name, int mode)
{
	PERR("dlopen: Local function called");
	return 0;
}

/**
 * dlinfo
 */
enum Reqeust { RTLD_DI_LINKMAP = 2 };

struct link_map {
	unsigned long l_addr;             /* Base Address of library */
	const char   *l_name;             /* Absolute Path to Library */
	const void   *l_ld;               /* Pointer to .dynamic in memory */
	struct link_map *l_next, *l_prev; /* linked list of of mapped libs */
};

int __attribute__((weak))
dlinfo(void *handle, int request, void *p)
{
	PERR("dlinfo: Local function called");
	return 0;
}

/**
 * dlsym
 */
#define RTLD_DEFAULT ((void *)-2)

void __attribute__((weak))
*dlsym(void *handle, const char *name)
{
	PERR("dlsym: Local function called");
	return 0;
}

} /* extern "C" */

#endif /* _DL_INTERFACE_H_ */
