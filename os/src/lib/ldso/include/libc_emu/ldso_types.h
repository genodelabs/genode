/*
 * \brief  ldso type definitions
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LDSO_TYPES_H_
#define _LDSO_TYPES_H_

#ifndef __ASSEMBLY__
//from gcc (defines size_t)
#include <stddef.h>
#include <base/fixed_stdint.h>

typedef genode_uint16_t    uint16_t;
typedef genode_uint32_t    uint32_t;
typedef genode_uint64_t    uint64_t;

typedef genode_int32_t     int32_t;
typedef genode_int64_t     int64_t;

typedef uint32_t           u_int32_t;
typedef unsigned long      u_long;

typedef char *             caddr_t;
typedef uint32_t           dev_t;
typedef uint32_t           ino_t;
typedef int64_t            off_t;
typedef int64_t            ssize_t;

typedef int                FILE;

typedef unsigned long      uintptr_t; //ARM reloc.c, only
extern int *stdout, *stdin, *stderr, errno;

struct stat {
	dev_t st_dev;
	ino_t st_ino;
};

enum statfs_flags {
	MNT_NOEXEC = 0x4
};

struct statfs {
	uint32_t f_flags;
	char f_mntonname[1];
};

enum {
	STDOUT_FILENO,
};
#endif //__ASSEMBLY__
#endif //_LDSO_TYPES_H_


