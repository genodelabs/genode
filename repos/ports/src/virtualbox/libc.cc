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
#include <base/printf.h>
#include <util/string.h>
#include <rtc_session/connection.h>

/* libc includes */
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/mount.h>    /* statfs */
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>        /* open */



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
		return (char *)"+rem_dias.e.l.f"
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
		               ;

	if (Genode::strcmp(name, "VBOX_LOG_FLAGS") == 0 ||
	    Genode::strcmp(name, "VBOX_RELEASE_LOG_FLAGS") == 0)
		return (char *)"thread";

	PWRN("getenv called for non-existent variable \"%s\"", name);
	return 0;
}


extern "C" int sigaction(int signum, const struct sigaction *act,
                         struct sigaction *oldact)
{
	/*
	 * Break infinite loop at 'VBox/Runtime/r3/init.cpp' :451
	 */;
	if (oldact)
		oldact->sa_flags = SA_SIGINFO;

	return 0;
}


/* our libc provides a _nanosleep function */
extern "C" int _nanosleep(const struct timespec *req, struct timespec *rem);
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem)
{
	Assert(req);
/*
	if (req) { // && req->tv_sec == 0 && req->tv_nsec <= 10 *1000000) {
		char _name[64];
		Genode::Thread_base::myself()->name(_name, sizeof(_name));
		PERR("%zd:%ld s:ns rip %p '%s'", req->tv_sec, req->tv_nsec,
		     __builtin_return_address(0), _name);
	}
*/
	return _nanosleep(req, rem);
}



/* Some dummy implementation for LibC functions */

extern "C" pid_t getpid(void)
{
	if (verbose)
		PINF("%s called - rip %p", __func__, __builtin_return_address(0));

	return 1345;
}

extern "C" int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	if (verbose)
		PINF("%s called - rip %p", __func__, __builtin_return_address(0));

	return -1;
}

extern "C" int _sigaction(int, const struct sigaction *, struct sigaction *)
{
	if (verbose)
		PINF("%s called - rip %p", __func__, __builtin_return_address(0));

	return -1;
}

extern "C" int futimes(int fd, const struct timeval tv[2])
{
	PINF("%s called - rip %p", __func__, __builtin_return_address(0));
	return 0;
}

extern "C" int lutimes(const char *filename, const struct timeval tv[2])
{
	PINF("%s called - file '%s' - rip %p", __func__, filename,
	     __builtin_return_address(0));
	return 0;
}

extern "C" int _sigprocmask()
{
	if (verbose)
		PINF("%s called - rip %p", __func__, __builtin_return_address(0));

	return 0;
}



/**
 * Used by Shared Folders Guest additions
 */
extern "C" int _fstatfs(int libc_fd, struct statfs *buf);
extern "C" int statfs(const char *path, struct statfs *buf)
{
	int fd = open(path, 0);

	if (fd < 0)
		return fd;

	int res = _fstatfs(fd, buf);

	close(fd);

	return res;
}

extern "C" long pathconf(char const *path, int name)
{
	if (name == _PC_NAME_MAX) return 255;

	PERR("pathconf does not support config option %d", name);
	errno = EINVAL;
	return -1;
}
