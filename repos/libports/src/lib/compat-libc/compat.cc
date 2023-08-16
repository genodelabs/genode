/*
 * \brief  FreeBSD-11 function versions
 * \author Sebastian Sumpf
 * \author Benjamin Lamowski
 * \date   2023-06-12
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>
#define _WANT_FREEBSD11_STAT /* enable freebsd11_stat and freebsd11_dirent */
#define _KERNEL              /* disable 'stat' */
#include <sys/stat.h>

#include <string.h>  /* needed for memset() in dirent.h */
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>

extern "C" int libc_stat(const char *, struct stat*);
extern "C" int libc_fstat(int, struct stat *);
extern "C" int libc_lstat(const char *, struct stat*);

extern "C" int libc_readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);

static void _to_freebsd11_stat(struct stat *libc_buf, struct freebsd11_stat *buf)
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


static bool _to_freebsd11_dirent(struct dirent *libc_buf, struct freebsd11_dirent *buf)
{
	buf->d_fileno = libc_buf->d_fileno;
	buf->d_type   = libc_buf->d_type;
	buf->d_namlen = libc_buf->d_namlen;
	/* Calculation taken from FreeBSD's FREEBSD11_DIRSIZ macro */
	buf->d_reclen = sizeof(struct freebsd11_dirent)
		        - sizeof(buf->d_name)
			+ ((buf->d_namlen + 1 + 3) &~ 3);
	if (libc_buf->d_namlen >= sizeof(buf->d_name))
		return false;

	strncpy(buf->d_name, libc_buf->d_name, sizeof(buf->d_name));

	return true;
}


static void* libc_handle(void)
{
	static void *handle = NULL;
	if (!handle) handle = dlopen("libc.lib.so", RTLD_LAZY);
	return handle;
}


int (*_stat)(char const*, struct stat*);
int (*_fstat)(int, struct stat *);
int (*_lstat)(char const*, struct stat*);
int (*_readdir_r)(DIR *dirp, struct dirent *entry, struct dirent **result);


int libc_stat(const char *path, struct stat *buf)
{
	if (!_stat)
		_stat = (int (*)(const char*, struct stat*)) dlsym(libc_handle(), "stat");

	return _stat(path, buf);
}


int libc_fstat(int fd, struct stat *buf)
{
	if (!_fstat)
		_fstat = (int (*)(int, struct stat*)) dlsym(libc_handle(), "fstat");

	return _fstat(fd, buf);
}


int libc_lstat(const char *path, struct stat *buf)
{
	if (!_lstat)
		_lstat = (int (*)(const char*, struct stat*)) dlsym(libc_handle(), "lstat");

	return _lstat(path, buf);
}

int libc_readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
	if (!_readdir_r)
		_readdir_r = (int (*)(DIR *dirp, struct dirent *entry, struct dirent **result)) dlsym(libc_handle(), "readdir_r");

	return _readdir_r(dirp, entry, result);
}


extern "C" int freebsd11_stat(const char *path, struct freebsd11_stat *buf)
{
	struct stat libc_buf { };

	int err = libc_stat(path, &libc_buf);
	if (err) return err;

	_to_freebsd11_stat(&libc_buf, buf);

	return 0;
}


extern "C" int freebsd11_fstat(int fd, struct freebsd11_stat *buf)
{
	struct stat libc_buf { };

	int err = libc_fstat(fd, &libc_buf);
	if (err) return err;

	_to_freebsd11_stat(&libc_buf, buf);

	return 0;
}

extern "C" int freebsd11_lstat(const char *path, struct freebsd11_stat *buf)
{
	struct stat libc_buf { };

	int err = libc_lstat(path, &libc_buf);
	if (err) return err;

	_to_freebsd11_stat(&libc_buf, buf);

	return 0;
}


extern "C" int freebsd11_readdir_r(DIR *dirp,
                                   struct freebsd11_dirent *entry,
                                   struct freebsd11_dirent**result)
{
	struct dirent libc_entry { };
	struct dirent * libc_result;

	int err = libc_readdir_r(dirp, &libc_entry, &libc_result);
	if (err) return err;

	if (libc_result != NULL) {
		if (!_to_freebsd11_dirent(&libc_entry, entry)) {
			*result = NULL;
			return ENAMETOOLONG;
		} else {
			*result = entry;
		}
	}

	return 0;
}


__sym_compat(fstat, freebsd11_fstat, FBSD_1.0);
__sym_compat(stat, freebsd11_stat, FBSD_1.0);
__sym_compat(lstat, freebsd11_lstat, FBSD_1.0);
__sym_compat(readdir_r, freebsd11_readdir_r, FBSD_1.0);
