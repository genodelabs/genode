/*
 * \brief  Plugin implementation
 * \author Christian Prochaska
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <internal/fd_alloc.h>
#include <internal/plugin_registry.h>
#include <internal/plugin.h>
#include <internal/init.h>
#include <internal/resume.h>

using namespace Libc;


static Resume *_resume_ptr;


void Libc::init_plugin(Resume &resume)
{
	_resume_ptr = &resume;
}


void Plugin::resume_all()
{
	struct Missing_call_of_init_plugin : Exception { };
	if (!_resume_ptr)
		throw Missing_call_of_init_plugin();

	_resume_ptr->resume_all();
}


Plugin::Plugin(int priority)
: _priority(priority)
{
	plugin_registry()->insert(this);
}


Plugin::~Plugin()
{
	plugin_registry()->remove(this);
}


int Plugin::priority()
{
	return _priority;
}


bool Plugin::supports_access(char const *path, int amode)
{
	return false;
}


bool Plugin::supports_mkdir(const char *, mode_t)
{
	return false;
}


bool Plugin::supports_open(const char *, int)
{
	return false;
}


bool Plugin::supports_pipe()
{
	return false;
}


bool Plugin::supports_poll()
{
	return false;
}


bool Plugin::supports_readlink(const char *path, char *buf, ::size_t bufsiz)
{
	return false;
}


bool Plugin::supports_rename(const char *, const char *)
{
	return false;
}


bool Plugin::supports_rmdir(const char*)
{
	return false;
}


bool Plugin::supports_socket(int, int, int)
{
	return false;
}


bool Plugin::supports_stat(const char*)
{
	return false;
}


bool Plugin::supports_symlink(const char*, const char *)
{
	return false;
}


bool Plugin::supports_unlink(const char*)
{
	return false;
}


bool Plugin::supports_mmap()
{
	return false;
}


/**
 * Generate dummy member function of Plugin class
 */
#define DUMMY(ret_type, ret_val, name, args) \
ret_type Plugin::name args \
{ \
	return ret_val; \
}


/*
 * Functions returning a 'File_descriptor'
 */
DUMMY(File_descriptor *, 0, dup,    (File_descriptor *));
DUMMY(File_descriptor *, 0, open,   (const char *, int));
DUMMY(File_descriptor *, 0, socket, (int, int, int));
DUMMY(File_descriptor *, 0, accept, (File_descriptor *, struct sockaddr *, socklen_t *));


/*
 * Functions taking a file descriptor as first argument
 */
DUMMY(int,     -1, bind,          (File_descriptor *, const struct sockaddr *, socklen_t));
DUMMY(int,     -1, close,         (File_descriptor *));
DUMMY(int,     -1, connect,       (File_descriptor *, const struct sockaddr *, socklen_t));
DUMMY(int,     -1, dup2,          (File_descriptor *, File_descriptor *new_fd));
DUMMY(int,     -1, fstatfs,       (File_descriptor *, struct statfs *));
DUMMY(int,     -1, fcntl,         (File_descriptor *, int cmd, long arg));
DUMMY(int,     -1, fstat,         (File_descriptor *, struct stat *));
DUMMY(int,     -1, fsync,         (File_descriptor *));
DUMMY(int,     -1, ftruncate,     (File_descriptor *, ::off_t));
DUMMY(ssize_t, -1, getdirentries, (File_descriptor *, char *, ::size_t, ::off_t *));
DUMMY(int,     -1, getpeername,   (File_descriptor *, struct sockaddr *, socklen_t *));
DUMMY(int,     -1, getsockname,   (File_descriptor *, struct sockaddr *, socklen_t *));
DUMMY(int,     -1, getsockopt,    (File_descriptor *, int, int, void *, socklen_t *));
DUMMY(int,     -1, ioctl,         (File_descriptor *, unsigned long, char*));
DUMMY(int,     -1, listen,        (File_descriptor *, int));
DUMMY(::off_t, -1, lseek,         (File_descriptor *, ::off_t, int));
DUMMY(ssize_t, -1, read,          (File_descriptor *, void *, ::size_t));
DUMMY(ssize_t, -1, recv,          (File_descriptor *, void *, ::size_t, int));
DUMMY(ssize_t, -1, recvfrom,      (File_descriptor *, void *, ::size_t, int, struct sockaddr *, socklen_t *));
DUMMY(ssize_t, -1, recvmsg,       (File_descriptor *, struct msghdr *, int));
DUMMY(ssize_t, -1, send,          (File_descriptor *, const void *, ::size_t, int));
DUMMY(ssize_t, -1, sendto,        (File_descriptor *, const void *, ::size_t, int, const struct sockaddr *, socklen_t));
DUMMY(int,     -1, setsockopt,    (File_descriptor *, int, int, const void *, socklen_t));
DUMMY(int,     -1, shutdown,      (File_descriptor *, int));
DUMMY(ssize_t, -1, write,         (File_descriptor *, const void *, ::size_t));


/*
 * Misc
 */
DUMMY(int, -1, access,       (char const *, int));
DUMMY(int, -1, mkdir,        (const char*, mode_t));
DUMMY(void *, (void *)(-1), mmap, (void *addr, ::size_t length, int prot, int flags,
                                   File_descriptor *, ::off_t offset));
DUMMY(int, -1, munmap,       (void *, ::size_t));
DUMMY(int, -1, msync,        (void *addr, ::size_t len, int flags));
DUMMY(int, -1, pipe,         (File_descriptor*[2]));
DUMMY(int, -1, poll,         (Pollfd[], int));
DUMMY(ssize_t, -1, readlink, (const char *, char *, ::size_t));
DUMMY(int, -1, rename,       (const char *, const char *));
DUMMY(int, -1, rmdir,        (const char*));
DUMMY(int, -1, stat,         (const char*, struct stat*));
DUMMY(int, -1, symlink,      (const char*, const char*));
DUMMY(int, -1, unlink,       (const char*));
