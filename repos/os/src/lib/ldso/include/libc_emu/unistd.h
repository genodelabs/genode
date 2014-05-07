/*
 * \brief  unistd.h prototypes/definitions required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_

#ifdef __cplusplus
extern "C" {
#endif

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

#ifdef __cplusplus
}
#endif

static inline
int issetugid(void) { return 1; }

static inline
int close(int fd) { return 0; }

void *file_phdr(const char *, void *);

enum whence {
	SEEK_SET
};

static inline
off_t lseek(int fd, off_t offset, int whence) { return 0; }

static inline
int fstat(int fd, struct stat *buf)
{
	buf->st_dev = fd + 1;
	buf->st_ino = fd + 1;
	return 0;
}

static inline
int fstatfs(int fd, struct statfs *buf)
{
	buf->f_flags = 0;
	buf->f_mntonname[0] = '\0';
	return 0;
}

enum access_mode {
	F_OK
};

static inline
int access(const char *pathname, int mode)
{
	return 0;
}

#endif //_UNISTD_H_
