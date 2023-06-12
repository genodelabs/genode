/*
 * \brief  FreeBSD-11 function versions
 * \author Sebastian Sumpf
 * \date   2023-06-12
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>
#define _WANT_FREEBSD11_STAT /* enable freebsd11_stat */
#define _KERNEL              /* disable 'stat' */
#include <sys/stat.h>
#include "libc.h"


static void _to_freebsd11(struct stat *libc_buf, struct freebsd11_stat *buf)
{
	buf->st_dev      = libc_buf->st_dev;
	buf->st_ino      = libc_buf->st_ino;
	buf->st_mode     = libc_buf->st_mode;
	buf->st_nlink    = libc_buf->st_nlink;
	buf->st_uid      = libc_buf->st_uid;
	buf->st_gid      = libc_buf->st_gid;
	buf->st_atim     = libc_buf->st_atim;
	buf->st_mtim     = libc_buf->st_mtim;
	buf->st_ctim     = libc_buf->st_ctim;
	buf->st_size     = libc_buf->st_size;
	buf->st_blksize  = libc_buf->st_blksize;
	buf->st_flags    = libc_buf->st_flags;
	buf->st_gen      = libc_buf->st_gen;
	buf->st_birthtim = libc_buf->st_birthtim;
}


extern "C" int stat(const char *path, struct freebsd11_stat *buf)
{
	struct stat libc_buf { };

	int err = libc_stat(path, &libc_buf);
	if (err) return err;

	_to_freebsd11(&libc_buf, buf);

	return 0;
}


extern "C" int fstat(int fd, struct freebsd11_stat *buf)
{
	struct stat libc_buf { };

	int err = libc_fstat(fd, &libc_buf);
	if (err) return err;

	_to_freebsd11(&libc_buf, buf);

	return 0;
}
