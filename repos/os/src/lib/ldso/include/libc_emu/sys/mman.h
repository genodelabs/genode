/*
 * \brief  mman.h prototypes/definitions required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef MMAN_H
#define MMAN_H

#include <ldso_types.h>

enum prot {
	PROT_NONE,
	PROT_READ,
	PROT_WRITE,
	PROT_EXEC = 0x4,
};

enum map {
	MAP_PRIVATE = 0x2,
	MAP_FIXED   = 0x10,
	MAP_ANON    = 0x1000,
	MAP_NOCORE  = 0x20000,
	MAP_LDSO    = 0x80000000, /* special flag if LDSO has not been relocated, yet */
};

#define MAP_FAILED ((void *)-1)

#ifdef __cplusplus
extern "C" {
#endif

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   munmap(void *addr, size_t length);

#ifdef __cplusplus
}
#endif


//dummies

static inline
int mprotect(const void *addr, size_t len, int prot) { return 0; }

#endif //MMAN_H
