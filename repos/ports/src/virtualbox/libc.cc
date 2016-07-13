/*
 * \brief  VirtualBox runtime (RT)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>
#include <rtc_session/connection.h>

/* libc includes */
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/mount.h>    /* statfs */
#include <sys/statvfs.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>        /* open */

#include "libc_errno.h"


/* libc memory allocator */
#include <libc_mem_alloc.h>

/* VirtualBox includes */
#include <iprt/mem.h>
#include <iprt/assert.h>

static const bool verbose = false;

using Genode::size_t;

/*
 * We cannot use the libc's version of malloc because it does not satisfies
 * the alignment constraints asserted by 'Runtime/r3/alloc.cpp'.
 */

extern "C" void *malloc(size_t size)
{
	return Libc::mem_alloc()->alloc(size, Genode::log2(RTMEM_ALIGNMENT));
}


extern "C" void *calloc(size_t nmemb, size_t size)
{
	void *ret = malloc(nmemb*size);
	if (ret)
		Genode::memset(ret, 0, nmemb*size);
	return ret;
}


extern "C" void free(void *ptr)
{
	Libc::mem_alloc()->free(ptr);
}


extern "C" void *realloc(void *ptr, Genode::size_t size)
{
	if (!ptr)
		return malloc(size);

	if (!size) {
		free(ptr);
		return 0;
	}

	/* determine size of old block content (without header) */
	unsigned long old_size = Libc::mem_alloc()->size_at(ptr);

	/* do not reallocate if new size is less than the current size */
	if (size <= old_size)
		return ptr;

	/* allocate new block */
	void *new_addr = malloc(size);

	/* copy content from old block into new block */
	if (new_addr)
		Genode::memcpy(new_addr, ptr, Genode::min(old_size, (unsigned long)size));

	/* free old block */
	free(ptr);

	return new_addr;
}


extern "C" char *getenv(const char *name)
{
	/*
	 * Logging to the pseudo file '/log' is done via the libc plugin provided
	 * by 'logging.cc'.
	 */
	if (Genode::strcmp(name, "VBOX_LOG_DEST") == 0 ||
	    Genode::strcmp(name, "VBOX_RELEASE_LOG_DEST") == 0)
		return (char *)"file=log";

	if (Genode::strcmp(name, "VBOX_LOG") == 0 ||
	    Genode::strcmp(name, "VBOX_RELEASE_LOG") == 0) 
		return (char *)"+rem_disas.e.l.f"
		               "+rem_printf.e.l.f"
//		               "+rem_run.e.l.f"
//		               "+pgm.e.l.f"
		               "+pdm"
//		               "+cpum.e.l.f"
//		               "+dev_pcnet.e.l.f"
//		               "+dev_pic.e.l.f"
//		               "+dev_apic.e.l.f"
		               "+dev_vmm.e"
//		               "+usb_mouse.e.l.f"
//		               "+main.e.l.f"
//		               "+hgcm.e.l.f"
//		               "+shared_folders.e.l.f"
//		               "+drv_host_serial.e.l.f"
//		               "+dev_audio.e.l.f"
		               ;

	if (Genode::strcmp(name, "VBOX_LOG_FLAGS") == 0 ||
	    Genode::strcmp(name, "VBOX_RELEASE_LOG_FLAGS") == 0)
		return (char *)"thread";

	Genode::warning("getenv called for non-existent variable \"", name, "\"");
	return 0;
}


extern "C" int sigaction(int signum, const struct sigaction *act,
                         struct sigaction *oldact)
{
	/*
	 * Break infinite loop at 'VBox/Runtime/r3/init.cpp' :451
	 */
	if (oldact)
		oldact->sa_flags = SA_SIGINFO;

	return 0;
}


/* our libc provides a _nanosleep function */
extern "C" int _nanosleep(const struct timespec *req, struct timespec *rem);
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem)
{
	Assert(req);

	return _nanosleep(req, rem);
}



/* Some dummy implementation for LibC functions */

extern "C" pid_t getpid(void)
{
	if (verbose)
		Genode::log(__func__, " called - rip ", __builtin_return_address(0));

	return 1345;
}

extern "C" int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	if (verbose)
		Genode::log(__func__, " called - rip ", __builtin_return_address(0));

	return -1;
}

extern "C" int _sigaction(int, const struct sigaction *, struct sigaction *)
{
	if (verbose)
		Genode::log(__func__, " called - rip ", __builtin_return_address(0));

	return -1;
}

extern "C" int futimes(int fd, const struct timeval tv[2])
{
	Genode::log("%s called - rip %p", __func__, __builtin_return_address(0));
	return 0;
}

extern "C" int lutimes(const char *filename, const struct timeval tv[2])
{
	Genode::log(__func__, ": called - file '", filename, "' - rip ",
	            __builtin_return_address(0));
	return 0;
}

extern "C" int _sigprocmask()
{
	if (verbose)
		Genode::log("%s called - rip %p", __func__, __builtin_return_address(0));

	return 0;
}



/**
 * Used by Shared Folders Guest additions
 */
extern "C" int statfs(const char *path, struct statfs *buf)
{
	if (!buf)
		return Libc::Errno(EFAULT);

	int fd = open(path, 0);

	if (fd < 0)
		return fd;

	struct statvfs result;
	int res = fstatvfs(fd, &result);

	close(fd);

	if (res)
		return res;

	Genode::memset(buf, 0, sizeof(*buf));

	buf->f_bavail = result.f_bavail;
	buf->f_bfree  = result.f_bfree;
	buf->f_blocks = result.f_blocks;
	buf->f_ffree  = result.f_ffree;
	buf->f_files  = result.f_files;
	buf->f_bsize  = result.f_bsize;

	bool show_warning = !buf->f_bsize || !buf->f_blocks || !buf->f_bavail;

	if (!buf->f_bsize)
		buf->f_bsize = 4096;
	if (!buf->f_blocks)
		buf->f_blocks = 128 * 1024;
	if (!buf->f_bavail)
		buf->f_bavail = buf->f_blocks;

	if (show_warning)
		Genode::warning("statfs provides bogus values for '", path, "' (probably a shared folder)");

	return res;
}

extern "C" long pathconf(char const *path, int name)
{
	if (name == _PC_NAME_MAX) return 255;

	Genode::error("pathconf does not support config option ", name);
	errno = EINVAL;
	return -1;
}
