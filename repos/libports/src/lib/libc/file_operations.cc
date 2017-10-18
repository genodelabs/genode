/*
 * \brief  libc file operations
 * \author Christian Prochaska
 * \author Norman Feske
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <os/path.h>
#include <util/token.h>

/* Genode-specific libc interfaces */
#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>

/* libc includes */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

/* libc-internal includes */
#include "libc_file.h"
#include "libc_mem_alloc.h"
#include "libc_mmap_registry.h"
#include "libc_errno.h"

using namespace Libc;


Libc::Mmap_registry *Libc::mmap_registry()
{
	static Libc::Mmap_registry registry;
	return &registry;
}


/***************
 ** Utilities **
 ***************/

/**
 * Current working directory
 */
static Absolute_path &cwd()
{
	static Absolute_path _cwd("/");
	return _cwd;
}

/**
 * path element token
 */

struct Scanner_policy_path_element
{
	static bool identifier_char(char c, unsigned /* i */)
	{
		return (c != '/') && (c != 0);
	}
};

typedef Genode::Token<Scanner_policy_path_element> Path_element_token;


/**
 * Resolve symbolic links in a given absolute path
 */

/* exception */
class Symlink_resolve_error { };

static void resolve_symlinks(char const *path, Absolute_path &resolved_path)
{
	char path_element[PATH_MAX];
	char symlink_target[PATH_MAX];

	Absolute_path current_iteration_working_path;
	Absolute_path next_iteration_working_path(path, cwd().base());

	enum { FOLLOW_LIMIT = 10 };
	int follow_count = 0;
	bool symlink_resolved_in_this_iteration;
	do {
		if (follow_count++ == FOLLOW_LIMIT) {
			errno = ELOOP;
			throw Symlink_resolve_error();
		}

		current_iteration_working_path = next_iteration_working_path;

		next_iteration_working_path.import("");
		symlink_resolved_in_this_iteration = false;

		Path_element_token t(current_iteration_working_path.base());

		while (t) {
			if (t.type() != Path_element_token::IDENT) {
					t = t.next();
					continue;
			}

			t.string(path_element, sizeof(path_element));

			try {
				next_iteration_working_path.append_element(path_element);
			} catch (Genode::Path_base::Path_too_long) {
				errno = ENAMETOOLONG;
				throw Symlink_resolve_error();
			}

			/*
			 * If a symlink has been resolved in this iteration, the remaining
			 * path elements get added and a new iteration starts.
			 */
			if (!symlink_resolved_in_this_iteration) {
				struct stat stat_buf;
				int res;
				FNAME_FUNC_WRAPPER_GENERIC(res = , stat, next_iteration_working_path.base(), &stat_buf);
				if (res == -1) {
					throw Symlink_resolve_error();
				}
				if (S_ISLNK(stat_buf.st_mode)) {
					FNAME_FUNC_WRAPPER_GENERIC(res = , readlink,
					                           next_iteration_working_path.base(),
					                           symlink_target, sizeof(symlink_target) - 1);
					if (res < 1)
						throw Symlink_resolve_error();

					/* zero terminate target */
					symlink_target[res] = 0;

					if (symlink_target[0] == '/')
						/* absolute target */
						next_iteration_working_path.import(symlink_target, cwd().base());
					else {
						/* relative target */
						next_iteration_working_path.strip_last_element();
						try {
							next_iteration_working_path.append_element(symlink_target);
						} catch (Genode::Path_base::Path_too_long) {
							errno = ENAMETOOLONG;
							throw Symlink_resolve_error();
						}
					}
					symlink_resolved_in_this_iteration = true;
				}
			}

			t = t.next();
		}

	} while (symlink_resolved_in_this_iteration);

	resolved_path = next_iteration_working_path;
	resolved_path.remove_trailing('/');
}


static void resolve_symlinks_except_last_element(char const *path, Absolute_path &resolved_path)
{
	Absolute_path absolute_path_without_last_element(path, cwd().base());
	absolute_path_without_last_element.strip_last_element();

	resolve_symlinks(absolute_path_without_last_element.base(), resolved_path);

	/* append last element to resolved path */
	Absolute_path absolute_path_last_element(path, cwd().base());
	absolute_path_last_element.keep_only_last_element();
	try {
		resolved_path.append_element(absolute_path_last_element.base());
	} catch (Genode::Path_base::Path_too_long) {
		errno = ENAMETOOLONG;
		throw Symlink_resolve_error();
	}
}


/********************
 ** Libc functions **
 ********************/

extern "C" int access(const char *path, int amode)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks(path, resolved_path);
		FNAME_FUNC_WRAPPER(access, resolved_path.base(), amode);
	} catch (Symlink_resolve_error) {
		errno = ENOENT;
		return -1;
	}
}


extern "C" int chdir(const char *path)
{
	struct stat stat_buf;
	if ((stat(path, &stat_buf) == -1) ||
	    (!S_ISDIR(stat_buf.st_mode))) {
		errno = ENOTDIR;
		return -1;
	}
	cwd().import(path, cwd().base());
	return 0;
}


/**
 * Close is called incorrectly enough to justify a silent failure
 */
extern "C" int _close(int libc_fd)
{
	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	return (!fd || !fd->plugin)
		? Libc::Errno(EBADF)
		: fd->plugin->close(fd);
}


extern "C" int close(int libc_fd) { return _close(libc_fd); }


extern "C" int _dup(int libc_fd)
{
	File_descriptor *ret_fd;
	FD_FUNC_WRAPPER_GENERIC(ret_fd =, 0, dup, libc_fd);
	return ret_fd ? ret_fd->libc_fd : INVALID_FD;
}


extern "C" int dup(int libc_fd)
{
	return _dup(libc_fd);
}


extern "C" int _dup2(int libc_fd, int new_libc_fd)
{
	File_descriptor *fd = libc_fd_to_fd(libc_fd, "dup2");
	if (!fd || !fd->plugin) {
		errno = EBADF;
		return INVALID_FD;
	}

	if (libc_fd == new_libc_fd)
		return libc_fd;

	/*
	 * Check if 'new_libc_fd' is already in use. If so, close it before
	 * allocating it again.
	 */
	File_descriptor *new_fd = file_descriptor_allocator()->find_by_libc_fd(new_libc_fd);
	if (new_fd)
		close(new_libc_fd);

	new_fd = file_descriptor_allocator()->alloc(fd->plugin, 0, new_libc_fd);
	new_fd->path(fd->fd_path);
	/* new_fd->context must be assigned by the plugin implementing 'dup2' */
	return fd->plugin->dup2(fd, new_fd);
}


extern "C" int dup2(int libc_fd, int new_libc_fd)
{
	return _dup2(libc_fd, new_libc_fd);
}


extern "C" int _execve(char const *filename, char *const argv[],
                       char *const envp[])
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks(filename, resolved_path);
		FNAME_FUNC_WRAPPER(execve, resolved_path.base(), argv, envp);
	} catch (Symlink_resolve_error) {
		return -1;
	}
}


extern "C" int execve(char const *filename, char *const argv[],
                      char *const envp[])
{
	return _execve(filename, argv, envp);
}


extern "C" int fchdir(int libc_fd)
{
	File_descriptor *fd = libc_fd_to_fd(libc_fd, "fchdir");

	if (!fd) {
		errno = EBADF;
		return INVALID_FD;
	}

	return chdir(fd->fd_path);
}


extern "C" int fcntl(int libc_fd, int cmd, ...)
{
	va_list ap;
	int res;
	va_start(ap, cmd);
	FD_FUNC_WRAPPER_GENERIC(res =, INVALID_FD, fcntl, libc_fd, cmd, va_arg(ap, long));
	va_end(ap);
	return res;
}


extern "C" int _fcntl(int libc_fd, int cmd, long arg) {
	return fcntl(libc_fd, cmd, arg); }


extern "C" int _fstat(int libc_fd, struct stat *buf) {
	FD_FUNC_WRAPPER(fstat, libc_fd, buf); }


extern "C" int fstat(int libc_fd, struct stat *buf)
{
	return _fstat(libc_fd, buf);
}


extern "C" int fstatat(int libc_fd, char const *path, struct stat *buf, int flags)
{
	if (*path == '/') {
		if (flags & AT_SYMLINK_NOFOLLOW)
			return lstat(path, buf);
		return stat(path, buf);
	}

	Libc::Absolute_path abs_path;

	if (libc_fd == AT_FDCWD) {
		abs_path = cwd();
		abs_path.append_element(path);
	} else {
		Libc::File_descriptor *fd =
			Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
		if (!fd) {
			errno = EBADF;
			return -1;
		}
		abs_path.import(path, fd->fd_path);
	}

	return (flags & AT_SYMLINK_NOFOLLOW)
		? lstat(abs_path.base(), buf)
		:  stat(abs_path.base(), buf);
}


extern "C" int _fstatfs(int libc_fd, struct statfs *buf) {
	FD_FUNC_WRAPPER(fstatfs, libc_fd, buf); }


extern "C" int fsync(int libc_fd) {
	FD_FUNC_WRAPPER(fsync, libc_fd); }


extern "C" int ftruncate(int libc_fd, ::off_t length) {
	FD_FUNC_WRAPPER(ftruncate, libc_fd, length); }


extern "C" ssize_t _getdirentries(int libc_fd, char *buf, ::size_t nbytes, ::off_t *basep) {
	FD_FUNC_WRAPPER(getdirentries, libc_fd, buf, nbytes, basep); }



extern "C" int ioctl(int libc_fd, int request, char *argp) {
	FD_FUNC_WRAPPER(ioctl, libc_fd, request, argp); }


extern "C" int _ioctl(int libc_fd, int request, char *argp) {
	FD_FUNC_WRAPPER(ioctl, libc_fd, request, argp); }


extern "C" ::off_t lseek(int libc_fd, ::off_t offset, int whence) {
	FD_FUNC_WRAPPER(lseek, libc_fd, offset, whence); }


extern "C" int lstat(const char *path, struct stat *buf)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks_except_last_element(path, resolved_path);
		FNAME_FUNC_WRAPPER(stat, resolved_path.base(), buf);
	} catch (Symlink_resolve_error) {
		return -1;
	}
}


extern "C" int mkdir(const char *path, mode_t mode)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks_except_last_element(path, resolved_path);
		FNAME_FUNC_WRAPPER(mkdir, resolved_path.base(), mode);
	} catch(Symlink_resolve_error) {
		return -1;
	}
}


extern "C" void *mmap(void *addr, ::size_t length, int prot, int flags,
                      int libc_fd, ::off_t offset)
{

	/* handle requests for anonymous memory */
	if (!addr && libc_fd == -1) {
		bool const executable = prot & PROT_EXEC;
		void *start = Libc::mem_alloc(executable)->alloc(length, PAGE_SHIFT);
		mmap_registry()->insert(start, length, 0);
		return start;
	}

	/* lookup plugin responsible for file descriptor */
	File_descriptor *fd = libc_fd_to_fd(libc_fd, "mmap");
	if (!fd || !fd->plugin || !fd->plugin->supports_mmap()) {
		Genode::warning("mmap not supported for file descriptor ", libc_fd);
		errno = EBADF;
		return (void *)INVALID_FD;
	}

	void *start = fd->plugin->mmap(addr, length, prot, flags, fd, offset);
	mmap_registry()->insert(start, length, fd->plugin);
	return start;
}


extern "C" int munmap(void *start, ::size_t length)
{
	if (!mmap_registry()->registered(start)) {
		Genode::warning("munmap: could not lookup plugin for address ", start);
		errno = EINVAL;
		return -1;
	}

	/*
	 * Lookup plugin that was used for mmap
	 *
	 * If the pointer is NULL, 'start' refers to an anonymous mmap.
	 */
	Plugin *plugin = mmap_registry()->lookup_plugin_by_addr(start);

	int ret = 0;
	if (plugin)
		ret = plugin->munmap(start, length);
	else {
		bool const executable = true;
		/* XXX another metadata handling required to track anonymous memory */
		Libc::mem_alloc(!executable)->free(start);
		Libc::mem_alloc(executable)->free(start);
	}

	mmap_registry()->remove(start);
	return ret;
}


extern "C" int msync(void *start, ::size_t len, int flags)
{
	if (!mmap_registry()->registered(start)) {
		Genode::warning("munmap: could not lookup plugin for address ", start);
		errno = EINVAL;
		return -1;
	}

	/*
	 * Lookup plugin that was used for mmap
	 *
	 * If the pointer is NULL, 'start' refers to an anonymous mmap.
	 */
	Plugin *plugin = mmap_registry()->lookup_plugin_by_addr(start);

	int ret = 0;
	if (plugin)
		ret = plugin->msync(start, len, flags);

	return ret;
}



extern "C" int _open(const char *pathname, int flags, ::mode_t mode)
{
	Absolute_path resolved_path;

	Plugin *plugin;
	File_descriptor *new_fdo;

	try {
		resolve_symlinks_except_last_element(pathname, resolved_path);
	} catch (Symlink_resolve_error) {
		return -1;
	}

	if (!(flags & O_NOFOLLOW)) {
		/* resolve last element */
		try {
			resolve_symlinks(resolved_path.base(), resolved_path);
		} catch (Symlink_resolve_error) {
			if (errno == ENOENT) {
				if (!(flags & O_CREAT))
					return -1;
			} else
				return -1;
		}
	}

	plugin = plugin_registry()->get_plugin_for_open(resolved_path.base(), flags);

	if (!plugin) {
		Genode::error("no plugin found for open(\"", pathname, "\", ", flags, ")");
		return -1;
	}

	new_fdo = plugin->open(resolved_path.base(), flags);
	if (!new_fdo) {
		Genode::error("plugin()->open(\"", pathname, "\") failed");
		return -1;
	}
	new_fdo->path(resolved_path.base());

	return new_fdo->libc_fd;
}


extern "C" int open(const char *pathname, int flags, ...)
{
	va_list ap;
	va_start(ap, flags);
	int res = _open(pathname, flags, va_arg(ap, unsigned));
	va_end(ap);
	return res;
}


extern "C" int pipe(int pipefd[2])
{
	Plugin *plugin;
	File_descriptor *pipefdo[2];

	plugin = plugin_registry()->get_plugin_for_pipe();

	if (!plugin) {
		Genode::error("no plugin found for pipe()");
		return -1;
	}

	if (plugin->pipe(pipefdo) == -1) {
		Genode::error("plugin()->pipe() failed");
		return -1;
	}

	pipefd[0] = pipefdo[0]->libc_fd;
	pipefd[1] = pipefdo[1]->libc_fd;

	return 0;
}


extern "C" ssize_t _read(int libc_fd, void *buf, ::size_t count)
{
	FD_FUNC_WRAPPER(read, libc_fd, buf, count);
}


extern "C" ssize_t read(int libc_fd, void *buf, ::size_t count)
{
	return _read(libc_fd, buf, count);
}


extern "C" ssize_t readlink(const char *path, char *buf, ::size_t bufsiz)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks_except_last_element(path, resolved_path);
		FNAME_FUNC_WRAPPER(readlink, resolved_path.base(), buf, bufsiz);
	} catch(Symlink_resolve_error) {
		return -1;
	}
}


extern "C" int rename(const char *oldpath, const char *newpath)
{
	try {
		Absolute_path resolved_oldpath, resolved_newpath;
		resolve_symlinks_except_last_element(oldpath, resolved_oldpath);
		resolve_symlinks_except_last_element(newpath, resolved_newpath);
		FNAME_FUNC_WRAPPER(rename, resolved_oldpath.base(), resolved_newpath.base());
	} catch(Symlink_resolve_error) {
		return -1;
	}
}


extern "C" int rmdir(const char *path)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks_except_last_element(path, resolved_path);
		FNAME_FUNC_WRAPPER(rmdir, resolved_path.base());
	} catch(Symlink_resolve_error) {
		return -1;
	}
}


extern "C" int stat(const char *path, struct stat *buf)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks(path, resolved_path);
		FNAME_FUNC_WRAPPER(stat, resolved_path.base(), buf);
	} catch(Symlink_resolve_error) {
		return -1;
	}
}


extern "C" int symlink(const char *oldpath, const char *newpath)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks_except_last_element(newpath, resolved_path);
		FNAME_FUNC_WRAPPER(symlink, oldpath, resolved_path.base());
	} catch(Symlink_resolve_error) {
		return -1;
	}
}


extern "C" int unlink(const char *path)
{
	try {
		Absolute_path resolved_path;
		resolve_symlinks_except_last_element(path, resolved_path);
		FNAME_FUNC_WRAPPER(unlink, resolved_path.base());
	} catch(Symlink_resolve_error) {
		return -1;
	}
}


extern "C" ssize_t _write(int libc_fd, const void *buf, ::size_t count)
{
	int flags = fcntl(libc_fd, F_GETFL);

	if ((flags != -1) && (flags & O_APPEND))
		lseek(libc_fd, 0, SEEK_END);

	FD_FUNC_WRAPPER(write, libc_fd, buf, count);
}


extern "C" ssize_t write(int libc_fd, const void *buf, ::size_t count) {
	return _write(libc_fd, buf, count); }


extern "C" int __getcwd(char *dst, ::size_t dst_size)
{
	Genode::strncpy(dst, cwd().base(), dst_size);
	return 0;
}
